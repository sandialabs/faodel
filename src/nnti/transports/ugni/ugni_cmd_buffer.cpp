// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


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

#include "nnti/transports/ugni/ugni_cmd_buffer.hpp"


namespace nnti {
namespace core {

ugni_cmd_buffer::ugni_cmd_buffer(
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
    int rc;

    setup_command_buffer();
}
ugni_cmd_buffer::~ugni_cmd_buffer()
{
    teardown_command_buffer();

    return;
}

//void
//ugni_cmd_buffer::post_recv(
//    nnti::core::ugni_cmd_msg *cmd_msg)
//{
//    int ibv_rc;
//    struct ibv_sge     sge;
//    struct ibv_recv_wr rq_wr, *bad_wr;
//
//    sge.addr   = (uint64_t)cmd_msg->buf();
//    sge.length = cmd_msg->size();
//    sge.lkey   = cmd_mr_->lkey;
//
//    rq_wr.next    = nullptr;
//    rq_wr.sg_list = &sge;
//    rq_wr.num_sge = 1;
//    rq_wr.wr_id = (uint64_t)cmd_msg;
//
//    ibv_rc=ibv_post_srq_recv(transport_->cmd_srq_, &rq_wr, &bad_wr);
//    if (ibv_rc) {
//        log_error("ugni_cmd_buffer", "failed to post SRQ recv (rq_wr=%p ; bad_wr=%p): %s",
//                &rq_wr, bad_wr, strerror(ibv_rc));
//    }
//}

void
ugni_cmd_buffer::setup_command_buffer(void)
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

    log_debug("ugni_cmd_buffer", "GNI_SmsgBufferSizeNeeded says %d credits needs bytes_per_mbox=%u.  Adjusting to %u.",
            cmd_count_, bytes_per_mbox, adjusted_bytes_per_mbox);

    gni_rc = GNI_EpCreate(transport_->nic_hdl_, transport_->req_send_ep_cq_hdl_, &send_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_cmd_buffer", "EpCreate(send_mbox.ep_hdl) failed: %d", gni_rc);
    }

    send_mbox.mbox_local_attrs.msg_type       = GNI_SMSG_TYPE_MBOX_AUTO_RETRANSMIT;
    send_mbox.mbox_local_attrs.buff_size      = adjusted_bytes_per_mbox;
    send_mbox.mbox_local_attrs.mbox_offset    = 0;
    send_mbox.mbox_local_attrs.mbox_maxcredit = cmd_count_;
    send_mbox.mbox_local_attrs.msg_maxsize    = cmd_size_;
    send_mbox.mbox_local_attrs.msg_buffer     = new char[adjusted_bytes_per_mbox];

    gni_rc = GNI_MemRegister(
            transport_->nic_hdl_,
            (uint64_t)send_mbox.mbox_local_attrs.msg_buffer,
            adjusted_bytes_per_mbox,
            transport_->req_recv_mem_cq_hdl_,
            GNI_MEM_READWRITE,
            -1,
            &send_mbox.mbox_local_attrs.mem_hndl);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_cmd_buffer", "MemRegister(cmd_buf_) failed: %d", gni_rc);
    }

    gni_rc = GNI_EpCreate(transport_->nic_hdl_, transport_->req_recv_ep_cq_hdl_, &recv_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_cmd_buffer", "EpCreate(recv_mbox.ep_hdl) failed: %d", gni_rc);
    }

    recv_mbox.mbox_local_attrs.msg_type       = GNI_SMSG_TYPE_MBOX_AUTO_RETRANSMIT;
    recv_mbox.mbox_local_attrs.buff_size      = adjusted_bytes_per_mbox;
    recv_mbox.mbox_local_attrs.mbox_offset    = 0;
    recv_mbox.mbox_local_attrs.mbox_maxcredit = cmd_count_;
    recv_mbox.mbox_local_attrs.msg_maxsize    = cmd_size_;
    recv_mbox.mbox_local_attrs.msg_buffer     = new char[adjusted_bytes_per_mbox];

    gni_rc = GNI_MemRegister(
            transport_->nic_hdl_,
            (uint64_t)recv_mbox.mbox_local_attrs.msg_buffer,
            adjusted_bytes_per_mbox,
            transport_->req_recv_mem_cq_hdl_,
            GNI_MEM_READWRITE,
            -1,
            &recv_mbox.mbox_local_attrs.mem_hndl);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_cmd_buffer", "MemRegister(cmd_buf_) failed: %d", gni_rc);
    }

    log_debug("ugni_cmd_buffer", "setup_command_buffer: exit (send_buf=%p  recv_buf=%p)",
              send_mbox.mbox_local_attrs.msg_buffer, recv_mbox.mbox_local_attrs.msg_buffer);
}
void
ugni_cmd_buffer::teardown_command_buffer(void)
{
    gni_return_t gni_rc = GNI_RC_SUCCESS;

    log_debug("ugni_cmd_buffer", "teardown_command_buffer: enter");

    gni_rc = GNI_EpUnbind (send_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_buffer", "EpUnbind(send_mbox.ep_hdl) failed: %d", gni_rc);
    gni_rc = GNI_EpDestroy (send_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_buffer", "EpDestroy(send_mbox.ep_hdl) failed: %d", gni_rc);

    gni_rc = GNI_EpUnbind (recv_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_buffer", "EpUnbind(recv_mbox.ep_hdl) failed: %d", gni_rc);
    gni_rc = GNI_EpDestroy (recv_mbox.ep_hdl);
    if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_buffer", "EpDestroy(recv_mbox.ep_hdl) failed: %d", gni_rc);

    gni_rc = GNI_MemDeregister (transport_->nic_hdl_, &send_mbox.mbox_local_attrs.mem_hndl);
    gni_rc = GNI_MemDeregister (transport_->nic_hdl_, &recv_mbox.mbox_local_attrs.mem_hndl);

    delete[] (char*)send_mbox.mbox_local_attrs.msg_buffer;
    delete[] (char*)recv_mbox.mbox_local_attrs.msg_buffer;

    log_debug("ugni_cmd_buffer", "teardown_command_buffer: exit");
}

} /* namespace core */
} /* namespace nnti */
