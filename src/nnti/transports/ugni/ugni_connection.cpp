// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <list>
#include <iostream>
#include <sstream>
#include <string>
#include <mutex>

#include <alps/libalpslli.h>
#include <gni_pub.h>

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_mailbox.hpp"
#include "nnti/transports/ugni/ugni_cmd_op.hpp"


namespace nnti {
namespace core {

    ugni_connection::ugni_connection(
        nnti::transports::ugni_transport *transport,
        uint32_t                          cmd_msg_size,
        uint32_t                          cmd_msg_count)
    : nnti_connection(),
      transport_(transport),
      cmd_msg_size_(cmd_msg_size),
      cmd_msg_count_(cmd_msg_count),
      smsg_waitlisted(false)
    {
        int rc;

        setup_mailbox();
        setup_rdma();
    }
    ugni_connection::ugni_connection(
        nnti::transports::ugni_transport        *transport,
        uint32_t                                 cmd_msg_size,
        uint32_t                                 cmd_msg_count,
        const std::map<std::string,std::string> &peer)
    : nnti_connection(),
      transport_(transport),
      cmd_msg_size_(cmd_msg_size),
      cmd_msg_count_(cmd_msg_count),
      peer_params_(peer),
      smsg_waitlisted(false)
    {
        int rc;

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();
        peer_     = new nnti::datatype::nnti_peer(transport, url);
        peer_->conn(this);

        setup_mailbox();
        setup_rdma();

        log_debug("", "hostname           = %s",   peer_params_.hostname.c_str());
        log_debug("", "addr               = %lu",  peer_params_.addr);
        log_debug("", "port               = %d",   peer_params_.port);
        log_debug("", "local_addr         = %d",   peer_params_.local_addr);
        log_debug("", "instance           = %d",   peer_params_.instance);
        log_debug("", "smsg_msg_buffer    = %llu",  peer_params_.smsg_msg_buffer);
        log_debug("", "smsg_mem_hdl_word1 = %llu",  peer_params_.smsg_mem_hdl.qword1);
        log_debug("", "smsg_mem_hdl_word2 = %llu",  peer_params_.smsg_mem_hdl.qword2);
        log_debug("", "conn_index         = %u",   peer_params_.conn_index);
    }
    ugni_connection::~ugni_connection()
    {
        teardown_mailbox();
        teardown_rdma();

        return;
    }

    void
    ugni_connection::peer_params(
        const std::map<std::string,std::string> &params)
    {
        int rc;

        peer_params_ = connection_params(params);

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();

        log_debug("", "hostname           = %s",   peer_params_.hostname.c_str());
        log_debug("", "addr               = %lu",  peer_params_.addr);
        log_debug("", "port               = %d",   peer_params_.port);
        log_debug("", "local_addr         = %d",   peer_params_.local_addr);
        log_debug("", "instance           = %d",   peer_params_.instance);
        log_debug("", "smsg_msg_buffer    = %llu",  peer_params_.smsg_msg_buffer);
        log_debug("", "smsg_mem_hdl_word1 = %llu",  peer_params_.smsg_mem_hdl.qword1);
        log_debug("", "smsg_mem_hdl_word2 = %llu",  peer_params_.smsg_mem_hdl.qword2);
        log_debug("", "conn_index         = %u",   peer_params_.conn_index);
    }

    void
    ugni_connection::peer_params(
        const std::string &params)
    {
        int rc;
        std::map<std::string,std::string> param_map;

        std::istringstream iss(params);
        std::string line;
        while (std::getline(iss, line)) {
            std::pair<std::string,std::string> res = split_string(line, '=');
            param_map[res.first] = res.second;
        }

        peer_params_ = connection_params(param_map);

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();

        log_debug("", "hostname           = %s",   peer_params_.hostname.c_str());
        log_debug("", "addr               = %lu",  peer_params_.addr);
        log_debug("", "port               = %d",   peer_params_.port);
        log_debug("", "local_addr         = %d",   peer_params_.local_addr);
        log_debug("", "instance           = %d",   peer_params_.instance);
        log_debug("", "smsg_msg_buffer    = %llu",  peer_params_.smsg_msg_buffer);
        log_debug("", "smsg_mem_hdl_word1 = %llu",  peer_params_.smsg_mem_hdl.qword1);
        log_debug("", "smsg_mem_hdl_word2 = %llu",  peer_params_.smsg_mem_hdl.qword2);
        log_debug("", "conn_index         = %u",   peer_params_.conn_index);
    }



    /*
     * generate a string that can be added into a URL query string
     */
    std::string
    ugni_connection::query_string(void)
    {
        std::stringstream ss;
        ss << "&conn_index=" << this->index;
        ss << mailbox_->query_string();
        return ss.str();
    }
    /*
     * generate a key=value (one per line) string that can be included in a Whookie reply
     */
    std::string
    ugni_connection::reply_string(void)
    {
        std::stringstream ss;
        ss << "conn_index=" << this->index << std::endl;
        ss << mailbox_->reply_string();
        return ss.str();
    }

    void
    ugni_connection::transition_to_ready(void)
    {
        int gni_rc;

        mailbox_->transition_to_ready(
            peer_params_.local_addr,
            peer_params_.instance,
            peer_params_.smsg_msg_buffer,
            peer_params_.smsg_mem_hdl);

        log_debug("ugni_connection", "this->index = %u  conn_index = %u", this->index, peer_params_.conn_index);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_EpSetEventData(
                mailbox_->ep_hdl(),
                this->index,
                peer_params_.conn_index);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpSetEventData(mbox_ep_hdl()) failed: %d", gni_rc);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_EpBind (
                long_get_ep_hdl_,
                peer_params_.local_addr,
                peer_params_.instance);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpBind(long_get_ep_hdl_) failed: %d", gni_rc);
        gni_rc = GNI_EpSetEventData(
                long_get_ep_hdl_,
                this->index,
                peer_params_.conn_index);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpSetEventData(long_get_ep_hdl_) failed: %d", gni_rc);
        nthread_unlock(&transport_->ugni_lock_);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_EpBind (
                rdma_ep_hdl_,
                peer_params_.local_addr,
                peer_params_.instance);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpBind(rdma_ep_hdl_) failed: %d", gni_rc);
        gni_rc = GNI_EpSetEventData(
                rdma_ep_hdl_,
                this->index,
                peer_params_.conn_index);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpSetEventData(rdma_ep_hdl_) failed: %d", gni_rc);
        nthread_unlock(&transport_->ugni_lock_);

        log_debug("ugni_connection", "rdma_ep_hdl_(%llu) bound to instance(%llu) at local_addr(%llu)", rdma_ep_hdl_, peer_params_.instance, peer_params_.local_addr);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_CqCreate (transport_->nic_hdl_, 64, 0, GNI_CQ_BLOCKING, NULL, NULL, &unexpected_ep_cq_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_transport", "CqCreate(unexpected_ep_cq_hdl_) failed: %d", gni_rc);
        }
        gni_rc = GNI_EpCreate(transport_->nic_hdl_, unexpected_ep_cq_hdl_, &unexpected_ep_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_mailbox", "EpCreate(unexpected_ep_hdl_) failed: %d", gni_rc);
        }
        gni_rc = GNI_EpBind (
                unexpected_ep_hdl_,
                peer_params_.local_addr,
                peer_params_.instance);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_connection", "EpBind(rdma_ep_hdl_) failed: %d", gni_rc);
        nthread_unlock(&transport_->ugni_lock_);


    }

    gni_ep_handle_t
    ugni_connection::mbox_ep_hdl(void)
    {
        return mailbox_->ep_hdl();
    }

    gni_ep_handle_t
    ugni_connection::long_get_ep_hdl(void)
    {
        return long_get_ep_hdl_;
    }

    gni_ep_handle_t
    ugni_connection::rdma_ep_hdl(void)
    {
        return rdma_ep_hdl_;
    }

    uint32_t
    ugni_connection::peer_conn_index(void)
    {
        return peer_params_.conn_index;
    }

    gni_ep_handle_t
    ugni_connection::unexpected_ep_hdl(void)
    {
        return unexpected_ep_hdl_;
    }

    gni_cq_handle_t
    ugni_connection::unexpected_cq_hdl(void)
    {
        return unexpected_ep_cq_hdl_;
    }

    bool
    ugni_connection::waitlisted(void)
    {
        return smsg_waitlisted;
    }

    int
    ugni_connection::waitlist_add(nnti::core::ugni_cmd_op *op)
    {
        std::lock_guard<std::mutex> lock(smsg_waitlist_lock_);
        smsg_waitlist_.push_back(op);
        smsg_waitlisted = true;
    }

    int
    ugni_connection::waitlist_execute(void)
    {
        std::lock_guard<std::mutex> lock(smsg_waitlist_lock_);
        while(!smsg_waitlist_.empty()) {
            nnti::core::ugni_cmd_op *op = smsg_waitlist_.front();
            if (op->update(nullptr) == 2) {
                // no smsg credits available
                goto need_credits;
            }
            smsg_waitlist_.pop_front();
        }
        smsg_waitlisted = false;
        return 0;
need_credits:
        return 2;
    }

    void
    ugni_connection::setup_mailbox(void)
    {
        log_debug("ugni_connection", "setup_mailbox: enter");

        mailbox_ = new nnti::core::ugni_mailbox(transport_, this, cmd_msg_size_, cmd_msg_count_);

        log_debug("ugni_connection", "setup_mailbox: exit (mailbox_=%p)", mailbox_);
    }
    void
    ugni_connection::teardown_mailbox(void)
    {
        log_debug("ugni_connection", "teardown_mailbox: enter");

        delete mailbox_;

        log_debug("ugni_connection", "teardown_mailbox: exit");
    }

    void
    ugni_connection::setup_rdma(void)
    {
        gni_return_t gni_rc = GNI_RC_SUCCESS;

        log_debug("ugni_connection", "setup_rdma: enter");

        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_EpCreate(transport_->nic_hdl_, transport_->long_get_ep_cq_hdl_, &long_get_ep_hdl_);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_mailbox", "EpCreate(long_get_ep_hdl_) failed: %d", gni_rc);
        }
        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_EpCreate(transport_->nic_hdl_, transport_->rdma_ep_cq_hdl_, &rdma_ep_hdl_);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_mailbox", "EpCreate(rdma_ep_hdl_) failed: %d", gni_rc);
        }

        log_debug("ugni_connection", "setup_rdma: exit (rdma_ep_hdl_=%llu)", rdma_ep_hdl_);
    }
    void
    ugni_connection::teardown_rdma(void)
    {
        gni_return_t gni_rc = GNI_RC_SUCCESS;

        log_debug("ugni_connection", "teardown_rdma: enter");

        gni_rc = GNI_EpUnbind (long_get_ep_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpUnbind(ep_hdl_) failed: %d", gni_rc);
        gni_rc = GNI_EpDestroy (long_get_ep_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpDestroy(ep_hdl_) failed: %d", gni_rc);

        gni_rc = GNI_EpUnbind (rdma_ep_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpUnbind(ep_hdl_) failed: %d", gni_rc);
        gni_rc = GNI_EpDestroy (rdma_ep_hdl_);
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_mailbox", "EpDestroy(ep_hdl_) failed: %d", gni_rc);

        log_debug("ugni_connection", "teardown_rdma: exit");
    }

    std::pair<std::string,std::string>
    ugni_connection::split_string(
        const std::string &item,
        const char         delim)
    {
        size_t p1 = item.find(delim);

        if(p1==std::string::npos) return std::pair<std::string,std::string>(item,"");
        if(p1==item.size())       return std::pair<std::string,std::string>(item.substr(0,p1),"");
        if(p1==0)                 return std::pair<std::string,std::string>("",item.substr(p1+1));

        return std::pair<std::string,std::string>(item.substr(0,p1), item.substr(p1+1));
    }

} /* namespace core */
} /* namespace nnti */
