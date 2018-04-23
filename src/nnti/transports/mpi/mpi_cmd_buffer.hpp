// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef MPI_CMD_BUFFER_HPP_
#define MPI_CMD_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <mpi.h>

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_cmd_msg.hpp"


namespace nnti {
namespace core {

class mpi_cmd_buffer {

//    friend class nnti::core::mpi_cmd_msg;

private:
    nnti::transports::mpi_transport       *transport_;

    uint32_t                               cmd_size_;
    uint32_t                               cmd_count_;

    char                                  *cmd_buf_;
    uint32_t                               cmd_offset_;

    std::vector<nnti::core::mpi_cmd_msg*>  msgs_;

public:
    mpi_cmd_buffer(
        nnti::transports::mpi_transport *transport,
        const uint32_t                   cmd_size,
        const uint32_t                   cmd_count);
    virtual ~mpi_cmd_buffer();

    std::vector<nnti::core::mpi_cmd_msg*>::iterator
    begin(void);
    std::vector<nnti::core::mpi_cmd_msg*>::iterator
    end(void);

    nnti::core::mpi_cmd_msg*
    cmd_msg(int index);

protected:
//    void
//    post_recv(
//        nnti::core::mpi_cmd_msg *cmd_msg);

private:
    void
    setup_command_buffer(void);
    void
    teardown_command_buffer(void);
};

} /* namespace core */
} /* namespace nnti */

#endif /* MPI_CMD_BUFFER_HPP_ */
