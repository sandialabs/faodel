// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_MAILBOX_HPP_
#define UGNI_MAILBOX_HPP_

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

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"


namespace nnti {
namespace core {

class ugni_mailbox {

    friend class nnti::core::ugni_cmd_msg;

private:
    nnti::transports::ugni_transport       *transport_;
    nnti::core::ugni_connection            *conn_;

    uint32_t                               cmd_size_;
    uint32_t                               cmd_count_;

    uint32_t                               cmd_offset_;

    std::vector<nnti::core::ugni_cmd_msg*> msgs_;

    gni_ep_handle_t ep_hdl_;
    gni_smsg_attr_t local_attrs_;
    gni_smsg_attr_t remote_attrs_;

public:
    ugni_mailbox(
        nnti::transports::ugni_transport *transport,
        nnti::core::ugni_connection      *conn,
        const uint32_t                    cmd_size,
        const uint32_t                    cmd_count);
    virtual ~ugni_mailbox();

    std::string
    query_string(void);
    std::string
    reply_string(void);

    void
    transition_to_ready(
        uint32_t          peer_local_addr,
        NNTI_instance_id  peer_instance,
        char             *peer_smsg_msg_buffer,
        gni_mem_handle_t  peer_smsg_mem_hdl);

    gni_ep_handle_t
    ep_hdl(void);

private:
    void
    setup_command_buffer(void);
    void
    teardown_command_buffer(void);
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_MAILBOX_HPP_ */
