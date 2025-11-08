// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_CMD_BUFFER_HPP_
#define UGNI_CMD_BUFFER_HPP_

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

class ugni_cmd_buffer {

    friend class nnti::core::ugni_cmd_msg;

private:
    nnti::transports::ugni_transport       *transport_;
    nnti::core::ugni_connection            *conn_;

    uint32_t                               cmd_size_;
    uint32_t                               cmd_count_;

    uint32_t                               cmd_offset_;

    std::vector<nnti::core::ugni_cmd_msg*> msgs_;

    struct mbox {
        gni_ep_handle_t ep_hdl;
        gni_smsg_attr_t mbox_local_attrs;
        gni_smsg_attr_t mbox_remote_attrs;
    };
    struct mbox send_mbox;
    struct mbox recv_mbox;

public:
    ugni_cmd_buffer(
        nnti::transports::ugni_transport *transport,
        nnti::core::ugni_connection      *conn,
        const uint32_t                    cmd_size,
        const uint32_t                    cmd_count);
    virtual ~ugni_cmd_buffer();

protected:
//    void
//    post_recv(
//        nnti::core::ugni_cmd_msg *cmd_msg);

private:
    void
    setup_command_buffer(void);
    void
    teardown_command_buffer(void);
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_CMD_BUFFER_HPP_ */
