// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_cmd_buffer.hpp
 *
 * @brief nnti_cmd_buffer.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef IBVERBS_CMD_BUFFER_HPP_
#define IBVERBS_CMD_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <infiniband/verbs.h>

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_msg.hpp"


namespace nnti {
namespace core {

class ibverbs_connection;

class ibverbs_cmd_buffer {

    friend class nnti::core::ibverbs_cmd_msg;

private:
    nnti::transports::ibverbs_transport       *transport_;
    nnti::core::ibverbs_connection            *conn_;

    uint32_t                                   cmd_size_;
    uint32_t                                   cmd_count_;

    char                                      *cmd_buf_;
    uint32_t                                   cmd_offset_;

    std::vector<nnti::core::ibverbs_cmd_msg*>  msgs_;

    struct ibv_mr                             *cmd_mr_;

public:
    ibverbs_cmd_buffer(
        nnti::transports::ibverbs_transport *transport,
        nnti::core::ibverbs_connection      *conn,
        const uint32_t                       cmd_size,
        const uint32_t                       cmd_count);
    virtual ~ibverbs_cmd_buffer();

protected:
    void
    post_recv(
        nnti::core::ibverbs_cmd_msg *cmd_msg);

private:
    void
    setup_command_buffer(void);
    void
    teardown_command_buffer(void);
};

} /* namespace core */
} /* namespace nnti */

#endif /* IBVERBS_CMD_BUFFER_HPP_ */
