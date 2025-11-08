// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_connection.hpp
 *
 * @brief nnti_connection.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef IBVERBS_CONNECTION_HPP_
#define IBVERBS_CONNECTION_HPP_

#include "nnti/nntiConfig.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <infiniband/verbs.h>
#if (NNTI_HAVE_VERBS_EXP_H)
#include <infiniband/verbs_exp.h>
#endif

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_buffer.hpp"


namespace nnti {
namespace core {

class ibverbs_connection
: public nnti_connection {
private:
    struct connection_params {
        std::string hostname;
        uint32_t    addr;
        uint32_t    port;
        uint64_t    gid1;
        uint64_t    gid2;
        std::string fingerprint;
        uint32_t    lid;
        uint32_t    cmd_qpn;
        uint32_t    rdma_qpn;
        uint32_t    long_get_qpn;

        connection_params(void) {
            return;
        }
        connection_params(const std::map<std::string,std::string> &peer) {
            for(auto &k_v : peer){
                log_debug_stream("connection_params") << "Key: " << k_v.first << " val: " << k_v.second;
            }

            try {
                hostname     = peer.at("hostname");
                addr         = nnti::util::str2uint32(peer.at("addr"));
                port         = nnti::util::str2uint32(peer.at("port"));
                gid1         = nnti::util::str2uint64(peer.at("gid1"));
                gid2         = nnti::util::str2uint64(peer.at("gid2"));
                fingerprint  = peer.at("fingerprint");
                lid          = nnti::util::str2uint32(peer.at("lid"));
                cmd_qpn      = nnti::util::str2uint32(peer.at("cmd_qpn"));
                rdma_qpn     = nnti::util::str2uint32(peer.at("rdma_qpn"));
                long_get_qpn = nnti::util::str2uint32(peer.at("long_get_qpn"));
            }
            catch (const std::out_of_range& oor) {
                log_error_stream("connection_params") << "Out of Range error: " << oor.what();
            }
        }
    };

private:
    nnti::transports::ibverbs_transport *transport_;
    struct ibv_qp                       *cmd_qp_;
    struct ibv_qp                       *rdma_qp_;
    struct ibv_qp                       *long_get_qp_;

    connection_params                    peer_params_;

public:
    ibverbs_connection(
        nnti::transports::ibverbs_transport *transport,
        uint32_t                             cmd_msg_size,
        uint32_t                             cmd_msg_count)
    : nnti_connection(),
      transport_(transport)
    {
        setup_command_qp();
        setup_rdma_qp();
        setup_long_get_qp();

        if (ibv_req_notify_cq(transport_->rdma_cq_, 0)) {
            log_error("ibverbs_connection", "Couldn't request CQ notification: %s", strerror(errno));
        }
    }
    ibverbs_connection(
        nnti::transports::ibverbs_transport     *transport,
        uint32_t                                 cmd_msg_size,
        uint32_t                                 cmd_msg_count,
        const std::map<std::string,std::string> &peer)
    : nnti_connection(),
      transport_(transport),
      peer_params_(peer)
    {
        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);

        peer_pid_ = url.pid();
        peer_     = new nnti::datatype::nnti_peer(transport, url);
        peer_->conn(this);

        fingerprint_ = peer_params_.fingerprint;

        setup_command_qp();
        setup_rdma_qp();
        setup_long_get_qp();

        if (ibv_req_notify_cq(transport_->rdma_cq_, 0)) {
            log_error("ibverbs_connection", "Couldn't request CQ notification: %s", strerror(errno));
        }

        log_debug("", "hostname     = %s",  peer_params_.hostname.c_str());
        log_debug("", "addr         = %lu", peer_params_.addr);
        log_debug("", "port         = %d",  peer_params_.port);
        log_debug("", "gid1         = %lu", peer_params_.gid1);
        log_debug("", "gid2         = %lu", peer_params_.gid2);
        log_debug("", "fingerprint  = %s",  peer_params_.fingerprint.c_str());
        log_debug("", "lid          = %d",  peer_params_.lid);
        log_debug("", "cmd_qpn      = %d",  peer_params_.cmd_qpn);
        log_debug("", "rdma_qpn     = %d",  peer_params_.rdma_qpn);
        log_debug("", "long_get_qpn = %d",  peer_params_.long_get_qpn);
    }

  ~ibverbs_connection() override {
        teardown_command_qp();
        teardown_rdma_qp();
        teardown_long_get_qp();

        return;
    }

    void
    peer_params(
        const std::map<std::string,std::string> &params)
    {
        peer_params_ = connection_params(params);

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);

        peer_pid_ = url.pid();

        log_debug("", "hostname     = %s",  peer_params_.hostname.c_str());
        log_debug("", "addr         = %lu", peer_params_.addr);
        log_debug("", "port         = %d",  peer_params_.port);
        log_debug("", "gid1         = %lu", peer_params_.gid1);
        log_debug("", "gid2         = %lu", peer_params_.gid2);
        log_debug("", "fingerprint  = %s",  peer_params_.fingerprint.c_str());
        log_debug("", "lid          = %d",  peer_params_.lid);
        log_debug("", "cmd_qpn      = %d",  peer_params_.cmd_qpn);
        log_debug("", "rdma_qpn     = %d",  peer_params_.rdma_qpn);
        log_debug("", "long_get_qpn = %d",  peer_params_.long_get_qpn);
    }

    void
    peer_params(
        const std::string &params)
    {
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

        log_debug("", "hostname     = %s",  peer_params_.hostname.c_str());
        log_debug("", "addr         = %lu", peer_params_.addr);
        log_debug("", "port         = %d",  peer_params_.port);
        log_debug("", "gid1         = %lu", peer_params_.gid1);
        log_debug("", "gid2         = %lu", peer_params_.gid2);
        log_debug("", "fingerprint  = %s",  peer_params_.fingerprint.c_str());
        log_debug("", "lid          = %d",  peer_params_.lid);
        log_debug("", "cmd_qpn      = %d",  peer_params_.cmd_qpn);
        log_debug("", "rdma_qpn     = %d",  peer_params_.rdma_qpn);
        log_debug("", "long_get_qpn = %d",  peer_params_.long_get_qpn);
    }

    /*
     * generate a string that can be added into a URL query string
     */
    std::string
    query_string(void) override {
        std::stringstream ss;
        ss << "&cmd_qpn=" << cmd_qpn();
        ss << "&rdma_qpn=" << rdma_qpn();
        ss << "&long_get_qpn=" << long_get_qpn();
        return ss.str();
    }
    /*
     * generate a key=value (one per line) string that can be included in a Whookie reply
     */
    std::string
    reply_string(void) override {
        std::stringstream ss;
        ss << "cmd_qpn=" << cmd_qpn() << std::endl;
        ss << "rdma_qpn=" << rdma_qpn() << std::endl;
        ss << "long_get_qpn=" << long_get_qpn() << std::endl;
        return ss.str();
    }

    struct ibv_qp *
    cmd_qp(void)
    {
        return cmd_qp_;
    }
    struct ibv_qp *
    rdma_qp(void)
    {
        return rdma_qp_;
    }
    struct ibv_qp *
    long_get_qp(void)
    {
        return long_get_qp_;
    }

    uint32_t
    cmd_qpn(void)
    {
        return cmd_qp_->qp_num;
    }
    uint32_t
    rdma_qpn(void)
    {
        return rdma_qp_->qp_num;
    }
    uint32_t
    long_get_qpn(void)
    {
        return long_get_qp_->qp_num;
    }

    void
    transition_to_ready(void)
    {
        int min_rnr_timer;
        int ack_timeout;
        int retry_count;

        /* bring the two QPs up to RTR */

        min_rnr_timer = 1;  /* means 0.01ms delay before sending RNR NAK */
        ack_timeout   = 17; /* time to wait for ACK/NAK before retransmitting.  4.096us * 2^17 == 0.536s */
        retry_count   = 7;  /* number of retries if no answer on primary path or if remote sends RNR NAK.  7 has special meaning of infinite retries. */
        transition_qp_from_reset_to_ready(cmd_qp_,
                                          peer_params_.cmd_qpn,
                                          peer_params_.lid,
                                          peer_params_.gid1,
                                          peer_params_.gid2,
                                          min_rnr_timer,
                                          ack_timeout,
                                          retry_count);

        min_rnr_timer = 31;  /* means 491.52ms delay before sending RNR NAK */
        ack_timeout   = 17;  /* time to wait for ACK/NAK before retransmitting.  4.096us * 2^17 == 0.536s */
        retry_count   = 7;   /* number of retries if no answer on primary path or if remote sends RNR NAK.  7 has special meaning of infinite retries. */
        transition_qp_from_reset_to_ready(rdma_qp_,
                                          peer_params_.rdma_qpn,
                                          peer_params_.lid,
                                          peer_params_.gid1,
                                          peer_params_.gid2,
                                          min_rnr_timer,
                                          ack_timeout,
                                          retry_count);

        min_rnr_timer = 31;  /* means 491.52ms delay before sending RNR NAK */
        ack_timeout   = 17;  /* time to wait for ACK/NAK before retransmitting.  4.096us * 2^17 == 0.536s */
        retry_count   = 7;   /* number of retries if no answer on primary path or if remote sends RNR NAK.  7 has special meaning of infinite retries. */
        transition_qp_from_reset_to_ready(long_get_qp_,
                                          peer_params_.long_get_qpn,
                                          peer_params_.lid,
                                          peer_params_.gid1,
                                          peer_params_.gid2,
                                          min_rnr_timer,
                                          ack_timeout,
                                          retry_count);
    }
    void
    transition_to_error(void)
    {
        /* bring the two QPs down to error */
        transition_qp_to_error(cmd_qp_,
                               peer_params_.cmd_qpn,
                               peer_params_.lid);
        transition_qp_to_error(rdma_qp_,
                               peer_params_.rdma_qpn,
                               peer_params_.lid);
        transition_qp_to_error(long_get_qp_,
                               peer_params_.long_get_qpn,
                               peer_params_.lid);
    }

private:
    void
    setup_command_qp(void)
    {
        cmd_qp_ = create_qp(transport_->cmd_cq_, transport_->cmd_srq_, transport_->qp_count_);
        if (!cmd_qp_) {
            log_error("ibverbs_connection", "failed to create QP: %s", strerror(errno));
        }
    }
    void
    teardown_command_qp(void)
    {
        int rc = ibv_destroy_qp(cmd_qp_);
        if (rc < 0)
            log_error("ibverbs_connection", "failed to destroy QP");
    }

    void
    setup_rdma_qp(void)
    {
        rdma_qp_ = create_qp(transport_->rdma_cq_, transport_->rdma_srq_, transport_->qp_count_);
        if (!rdma_qp_) {
            log_error("ibverbs_connection", "failed to create QP: %s", strerror(errno));
        }
    }

    void
    teardown_rdma_qp(void)
    {
        int rc = ibv_destroy_qp(rdma_qp_);
        if (rc < 0)
            log_error("ibverbs_connection", "failed to destroy QP");
    }

    void
    setup_long_get_qp(void)
    {
        long_get_qp_ = create_qp(transport_->long_get_cq_, transport_->long_get_srq_, transport_->qp_count_);
        if (!long_get_qp_) {
            log_error("ibverbs_connection", "failed to create QP: %s", strerror(errno));
        }
    }

    void
    teardown_long_get_qp(void)
    {
        int rc = ibv_destroy_qp(long_get_qp_);
        if (rc < 0)
            log_error("ibverbs_connection", "failed to destroy QP");
    }

    ibv_qp *
    create_qp(
        ibv_cq  *cq,
        ibv_srq *srq,
        int      wr_count)
    {
        if ((transport_->have_exp_qp_) && (transport_->byte_swap_atomic_result_)) {
#if NNTI_HAVE_IBV_EXP_CREATE_QP
            struct ibv_exp_qp_init_attr att;

            memset(&att, 0, sizeof(att));
            att.qp_context       = this;
            att.send_cq          = cq;
            att.recv_cq          = cq;
            att.srq              = srq;
            att.cap.max_recv_wr  = wr_count;
            att.cap.max_send_wr  = wr_count;
            att.cap.max_recv_sge = 1;
            att.cap.max_send_sge = 1;
            att.qp_type          = IBV_QPT_RC;

            /* use expanded verbs qp create to enable use of mlx5 atomics */
            att.comp_mask = IBV_EXP_QP_INIT_ATTR_PD;
            att.pd = transport_->pd_;

#if NNTI_HAVE_IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG
            att.comp_mask |= IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG;
            att.max_atomic_arg = sizeof (uint64_t);
#endif

#if NNTI_HAVE_IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY
            att.exp_create_flags = IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY;
            att.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS;
#endif

            return ibv_exp_create_qp(transport_->ctx_, &att);
#endif
        } else {
            struct ibv_qp_init_attr att;

            memset(&att, 0, sizeof(att));
            att.qp_context       = this;
            att.send_cq          = cq;
            att.recv_cq          = cq;
            att.srq              = srq;
            att.cap.max_recv_wr  = wr_count;
            att.cap.max_send_wr  = wr_count;
            att.cap.max_recv_sge = 1;
            att.cap.max_send_sge = 1;
            att.qp_type          = IBV_QPT_RC;

            return ibv_create_qp(transport_->pd_, &att);
        }

        return nullptr;
    }

    void
    transition_qp_from_reset_to_ready(
        struct ibv_qp *qp,
        uint32_t       peer_qpn,
        int            peer_lid,
        uint64_t       peer_gid1,
        uint64_t       peer_gid2,
        int            min_rnr_timer,
        int            ack_timeout,
        int            retry_count)
    {
        enum ibv_qp_attr_mask mask;
        struct ibv_qp_attr attr;

        log_debug("ibverbs_connection", "enter (qp=%p ; qp->qp_num=%u ; peer_qpn=%u ; peer_lid=%u)", qp, qp->qp_num, peer_qpn, peer_lid);

        /* Transition QP to Init */
        mask = (enum ibv_qp_attr_mask)
               ( IBV_QP_STATE
               | IBV_QP_ACCESS_FLAGS
               | IBV_QP_PKEY_INDEX
               | IBV_QP_PORT );
        memset(&attr, 0, sizeof(attr));
        attr.qp_state        = IBV_QPS_INIT;
        attr.pkey_index      = 0;
        attr.port_num        = transport_->nic_port_;
        attr.qp_access_flags =
               ( IBV_ACCESS_LOCAL_WRITE
               | IBV_ACCESS_REMOTE_WRITE
               | IBV_ACCESS_REMOTE_READ
               | IBV_ACCESS_REMOTE_ATOMIC );
        if (ibv_modify_qp(qp, &attr, mask)) {
            log_error("ibverbs_connection", "failed to modify qp from RESET to INIT state");
        }

        /* Transition QP to Ready-to-Receive (RTR) */
        mask = (enum ibv_qp_attr_mask)
               ( IBV_QP_STATE
               | IBV_QP_MAX_DEST_RD_ATOMIC
               | IBV_QP_AV
               | IBV_QP_PATH_MTU
               | IBV_QP_RQ_PSN
               | IBV_QP_DEST_QPN
               | IBV_QP_MIN_RNR_TIMER );
        memset(&attr, 0, sizeof(attr));
        attr.qp_state           = IBV_QPS_RTR;
        attr.max_dest_rd_atomic = 1;
        attr.path_mtu           = IBV_MTU_1024;
        attr.rq_psn             = 0;
        attr.dest_qp_num        = peer_qpn;
        attr.min_rnr_timer      = min_rnr_timer; /* delay before sending RNR NAK */
        attr.ah_attr.dlid       = peer_lid;
        attr.ah_attr.port_num   = transport_->nic_port_;
        attr.ah_attr.is_global  = 0;

        if (transport_->is_roce_)
        {
            ibv_gid tmp_gid;
            
            attr.ah_attr.is_global = 1;
            *(uint64_t*)&(attr.ah_attr.grh.dgid.raw[0]) = peer_gid1;
            *(uint64_t*)&(attr.ah_attr.grh.dgid.raw[8]) = peer_gid2;
            attr.ah_attr.grh.flow_label = 1;
            attr.ah_attr.grh.hop_limit = 16;
            attr.ah_attr.grh.sgid_index = transport_->nic_gid_idx_;
            attr.ah_attr.grh.traffic_class = 0;

            log_debug("ibverbs_connection", "This is RoCE - setting sgid_index to %d", attr.ah_attr.grh.sgid_index);

            ibv_ah *ah = ibv_create_ah(transport_->pd_, &attr.ah_attr);
            if (ah == NULL) {
                log_error("ibverbs_connection", "ibv_create_ah failed - gid not operational");
            } else {
                ibv_destroy_ah(ah);
            }
        }

        if (ibv_modify_qp(qp, &attr, mask)) {
            log_error("ibverbs_connection", "failed to modify qp from INIT to RTR state");
        }

        /* Transition QP to Ready-to-Send (RTS) */
        mask = (enum ibv_qp_attr_mask)
               ( IBV_QP_STATE
               | IBV_QP_SQ_PSN
               | IBV_QP_MAX_QP_RD_ATOMIC
               | IBV_QP_TIMEOUT
               | IBV_QP_RETRY_CNT
               | IBV_QP_RNR_RETRY );
        memset(&attr, 0, sizeof(attr));
        attr.qp_state      = IBV_QPS_RTS;
        attr.sq_psn        = 0;
        attr.max_rd_atomic = 1;
        attr.timeout       = ack_timeout;   /* time to wait for ACK/NAK before retransmitting.  4.096us * 2^ack_timeout */
        attr.retry_cnt     = retry_count; /* number of retries if no answer on primary path */
        attr.rnr_retry     = retry_count; /* number of retries if remote sends RNR NAK */
        if (ibv_modify_qp(qp, &attr, mask)) {
            log_error("ibverbs_connection", "failed to modify qp from RTR to RTS state");
        }

        log_debug("ibverbs_connection", "exit");
    }

    void
    transition_qp_from_error_to_ready(
        struct ibv_qp *qp,
        uint32_t       peer_qpn,
        int            peer_lid,
        uint64_t       peer_gid1,
        uint64_t       peer_gid2,
        int            min_rnr_timer,
        int            ack_timeout,
        int            retry_count)
    {
        enum ibv_qp_attr_mask mask;
        struct ibv_qp_attr attr;

        log_debug("ibverbs_connection", "enter (qp=%p ; qp->qp_num=%u ; peer_qpn=%u ; peer_lid=%u)", qp, qp->qp_num, peer_qpn, peer_lid);

        /* Transition QP to Reset */
        mask = (enum ibv_qp_attr_mask)
               IBV_QP_STATE;
        memset(&attr, 0, sizeof(attr));
        attr.qp_state = IBV_QPS_RESET;
        if (ibv_modify_qp(qp, &attr, mask)) {
            log_error("ibverbs_connection", "failed to modify qp from ERROR to RESET state");
        }

        transition_qp_from_reset_to_ready(qp,
                                          peer_qpn,
                                          peer_lid,
                                          peer_gid1,
                                          peer_gid2,
                                          min_rnr_timer,
                                          ack_timeout,
                                          retry_count);

        log_debug("ibverbs_connection", "exit");
    }

    void
    transition_qp_to_error(
        struct ibv_qp *qp,
        uint32_t       peer_qpn,
        int            peer_lid)
    {
        enum ibv_qp_attr_mask mask;
        struct ibv_qp_attr attr;

        /* Transition QP to Error */
        mask = (enum ibv_qp_attr_mask)
               IBV_QP_STATE;
        memset(&attr, 0, sizeof(attr));
        attr.qp_state = IBV_QPS_ERR;
        if (ibv_modify_qp(qp, &attr, mask)) {
            log_error("ibverbs_connection", "failed to modify qp to ERROR state");
        }
    }

    std::pair<std::string,std::string>
    split_string(
        const std::string &item,
        const char         delim)
    {
        size_t p1 = item.find(delim);

        if(p1==std::string::npos) return std::pair<std::string,std::string>(item,"");
        if(p1==item.size())       return std::pair<std::string,std::string>(item.substr(0,p1),"");
        if(p1==0)                 return std::pair<std::string,std::string>("",item.substr(p1+1));

        return std::pair<std::string,std::string>(item.substr(0,p1), item.substr(p1+1));
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* IBVERBS_CONNECTION_HPP_ */
