// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nntiConfig.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <alps/libalpslli.h>
#include <gni_pub.h>

#include "nnti/transports/ugni/ugni_mailbox.hpp"


namespace nnti {
namespace core {

ugni_mailbox::ugni_mailbox(
    nnti::transports::ugni_transport *transport,
    nnti::core::ugni_connection      *conn,
    const uint32_t                   cmd_size,
    const uint32_t                   cmd_count)
: transport_(transport),
  conn_(conn),
  cmd_size_(cmd_size),
  cmd_count_(cmd_count),
  cmd_offset_(0)
{
    setup_command_buffer();
}
ugni_mailbox::~ugni_mailbox()
{
    teardown_command_buffer();
}

std::string
ugni_mailbox::query_string(void)
{
    std::stringstream ss;
    ss << "&smsg_msg_buffer="    << (uint64_t)local_attrs_.msg_buffer;
    ss << "&smsg_mem_hdl_word1=" << local_attrs_.mem_hndl.qword1;
    ss << "&smsg_mem_hdl_word2=" << local_attrs_.mem_hndl.qword2;
    return ss.str();
}

std::string
ugni_mailbox::reply_string(void)
{
    std::stringstream ss;
    ss << "smsg_msg_buffer="    << (uint64_t)local_attrs_.msg_buffer << std::endl;
    ss << "smsg_mem_hdl_word1=" << local_attrs_.mem_hndl.qword1 << std::endl;
    ss << "smsg_mem_hdl_word2=" << local_attrs_.mem_hndl.qword2 << std::endl;
    return ss.str();
}

void
ugni_mailbox::transition_to_ready(
    uint32_t          peer_local_addr,
    NNTI_instance_id  peer_instance,
    char             *peer_smsg_msg_buffer,
    gni_mem_handle_t  peer_smsg_mem_hdl)
{
     int rc;
    // now we know enough to wire up the mailboxes.
    // create/bind the send and receive EPs
    // call SmsgInit()
    nthread_lock(&transport_->ugni_lock_);
    rc=GNI_EpBind (
            ep_hdl_,
            peer_local_addr,
            peer_instance);
    nthread_unlock(&transport_->ugni_lock_);
    if (rc!=GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpBind(ep_hdl_) failed: %d", rc);

    remote_attrs_.msg_buffer = peer_smsg_msg_buffer;
    remote_attrs_.mem_hndl   = peer_smsg_mem_hdl;
    nthread_lock(&transport_->ugni_lock_);
    rc=GNI_SmsgInit(
            ep_hdl_,
            &local_attrs_,
            &remote_attrs_);
    nthread_unlock(&transport_->ugni_lock_);
    if (rc!=GNI_RC_SUCCESS) log_error("ugni_mailbox", "SmsgInit(ep_hdl_) failed: %d", rc);

    log_debug("ugni_mailbox", "new connection ep_hdl_(%llu)", ep_hdl_);
}

gni_ep_handle_t
ugni_mailbox::ep_hdl(void)
{
    return ep_hdl_;
}


void
ugni_mailbox::setup_command_buffer(void)
{
    gni_return_t gni_rc = GNI_RC_SUCCESS;

    const unsigned int CACHELINE_SIZE = 64;

    unsigned int bytes_per_mbox          = 0;
    unsigned int adjusted_bytes_per_mbox = 0;

    gni_smsg_attr_t smsg_attributes;
    smsg_attributes.msg_type       = GNI_SMSG_TYPE_MBOX_AUTO_RETRANSMIT;
    smsg_attributes.mbox_maxcredit = cmd_count_;
    smsg_attributes.msg_maxsize    = cmd_size_;

    gni_rc = GNI_SmsgBufferSizeNeeded(&smsg_attributes, &bytes_per_mbox);

    adjusted_bytes_per_mbox = bytes_per_mbox + cmd_count_ * cmd_size_;
    adjusted_bytes_per_mbox = ((adjusted_bytes_per_mbox + CACHELINE_SIZE - 1) / CACHELINE_SIZE) * CACHELINE_SIZE;

    log_debug("ugni_mailbox", "GNI_SmsgBufferSizeNeeded says %d credits needs bytes_per_mbox=%u.  Adjusting to %u.",
            cmd_count_, bytes_per_mbox, adjusted_bytes_per_mbox);

    local_attrs_.msg_type       = GNI_SMSG_TYPE_MBOX_AUTO_RETRANSMIT;
    local_attrs_.buff_size      = adjusted_bytes_per_mbox;
    local_attrs_.mbox_offset    = 0;
    local_attrs_.mbox_maxcredit = cmd_count_;
    local_attrs_.msg_maxsize    = cmd_size_;
    local_attrs_.msg_buffer     = new char[adjusted_bytes_per_mbox];

    remote_attrs_ = local_attrs_;

    nthread_lock(&transport_->ugni_lock_);
    gni_rc = GNI_MemRegister(
            transport_->nic_hdl_,
            (uint64_t)local_attrs_.msg_buffer,
            adjusted_bytes_per_mbox,
            transport_->smsg_mem_cq_hdl_,
            GNI_MEM_READWRITE,
            -1,
            &local_attrs_.mem_hndl);
    nthread_unlock(&transport_->ugni_lock_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_mailbox", "MemRegister(cmd_buf_) failed: %d", gni_rc);
    }

    nthread_lock(&transport_->ugni_lock_);
    gni_rc = GNI_EpCreate(transport_->nic_hdl_, transport_->smsg_ep_cq_hdl_, &ep_hdl_);
    nthread_unlock(&transport_->ugni_lock_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_mailbox", "EpCreate(send_ep_hdl_) failed: %d", gni_rc);
    }

    log_debug("ugni_mailbox", "setup_command_buffer: exit (smsg.buf=%p)", local_attrs_.msg_buffer);
}
void
ugni_mailbox::teardown_command_buffer(void)
{
    gni_return_t gni_rc = GNI_RC_SUCCESS;

    log_debug("ugni_mailbox", "teardown_command_buffer: enter");

    gni_rc = GNI_EpUnbind (ep_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpUnbind(ep_hdl_) failed: %d", gni_rc);
    gni_rc = GNI_EpDestroy (ep_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpDestroy(ep_hdl_) failed: %d", gni_rc);

    gni_rc = GNI_MemDeregister (transport_->nic_hdl_, &local_attrs_.mem_hndl);

    delete[] (char*)local_attrs_.msg_buffer;

    log_debug("ugni_mailbox", "teardown_command_buffer: exit");
}

} /* namespace core */
} /* namespace nnti */
