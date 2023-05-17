// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_CONNECTION_HPP_
#define UGNI_CONNECTION_HPP_

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

class ugni_connection
: public nnti_connection {
private:
    struct connection_params {
        std::string       hostname;
        uint32_t          addr;
        uint32_t          port;
        std::string       fingerprint;
        uint32_t          local_addr;
        NNTI_instance_id  instance;
        char             *smsg_msg_buffer;
        gni_mem_handle_t  smsg_mem_hdl;
        uint32_t          conn_index;

        connection_params(void) {
            return;
        }
        connection_params(const std::map<std::string,std::string> &peer) {
            for(auto &k_v : peer){
                log_debug_stream("connection_params") << "Key: " << k_v.first << " val: " << k_v.second;
            }

            try {
                hostname                 = peer.at("hostname");
                addr                     = nnti::util::str2uint32(peer.at("addr"));
                port                     = nnti::util::str2uint32(peer.at("port"));
                fingerprint              = peer.at("fingerprint");
                local_addr               = nnti::util::str2uint32(peer.at("local_addr"));
                instance                 = nnti::util::str2uint32(peer.at("instance"));
                smsg_msg_buffer          = (char*)nnti::util::str2uint64(peer.at("smsg_msg_buffer"));
                smsg_mem_hdl.qword1      = nnti::util::str2uint64(peer.at("smsg_mem_hdl_word1"));
                smsg_mem_hdl.qword2      = nnti::util::str2uint64(peer.at("smsg_mem_hdl_word2"));
                conn_index               = nnti::util::str2uint32(peer.at("conn_index"));
            }
            catch (const std::out_of_range& oor) {
                log_error_stream("connection_params") << "Out of Range error: " << oor.what();
            }
        }
    };

private:
    nnti::transports::ugni_transport *transport_;

    connection_params peer_params_;

    uint32_t cmd_msg_size_;
    uint32_t cmd_msg_count_;

    nnti::core::ugni_mailbox *mailbox_;

    gni_cq_handle_t unexpected_ep_cq_hdl_;
    gni_ep_handle_t unexpected_ep_hdl_;

    gni_ep_handle_t long_get_ep_hdl_;
    gni_ep_handle_t rdma_ep_hdl_;

    std::atomic<bool>                   smsg_waitlisted;
    std::mutex                          smsg_waitlist_lock_;
    std::list<nnti::core::ugni_cmd_op*> smsg_waitlist_;

public:
    ugni_connection(
        nnti::transports::ugni_transport *transport,
        uint32_t                          cmd_msg_size,
        uint32_t                          cmd_msg_count);
    ugni_connection(
        nnti::transports::ugni_transport        *transport,
        uint32_t                                 cmd_msg_size,
        uint32_t                                 cmd_msg_count,
        const std::map<std::string,std::string> &peer);
    virtual ~ugni_connection();

    void
    peer_params(
        const std::map<std::string,std::string> &params);
    void
    peer_params(
        const std::string &params);

    /*
     * generate a string that can be added into a URL query string
     */
    std::string
    query_string(void);

    /*
     * generate a key=value (one per line) string that can be included in a Whookie reply
     */
    std::string
    reply_string(void);

    void
    transition_to_ready(void);

    gni_ep_handle_t
    mbox_ep_hdl(void);

    gni_ep_handle_t
    long_get_ep_hdl(void);

    gni_ep_handle_t
    rdma_ep_hdl(void);

    uint32_t
    peer_conn_index(void);

    gni_ep_handle_t
    unexpected_ep_hdl(void);

    gni_cq_handle_t
    unexpected_cq_hdl(void);

    bool
    need_credit_msg(void);

    bool
    waitlisted(void);

    int
    waitlist_add(nnti::core::ugni_cmd_op *op);

    int
    waitlist_execute(void);

private:
    void
    setup_mailbox(void);
    void
    teardown_mailbox(void);

    void
    setup_rdma(void);
    void
    teardown_rdma(void);

    std::pair<std::string,std::string>
    split_string(
        const std::string &item,
        const char         delim);
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_CONNECTION_HPP_ */
