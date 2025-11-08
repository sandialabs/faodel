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

#include <mpi.h>

#include "nnti/transports/mpi/mpi_cmd_buffer.hpp"


namespace nnti {
namespace core {

mpi_cmd_buffer::mpi_cmd_buffer(
    nnti::transports::mpi_transport *transport,
    const uint32_t                   cmd_size,
    const uint32_t                   cmd_count)
: transport_(transport),
  cmd_size_(cmd_size),
  cmd_count_(cmd_count)
{
    setup_command_buffer();
}
mpi_cmd_buffer::~mpi_cmd_buffer()
{
    teardown_command_buffer();

    return;
}

std::vector<nnti::core::mpi_cmd_msg*>::iterator
mpi_cmd_buffer::begin(void)
{
    return msgs_.begin();
}
std::vector<nnti::core::mpi_cmd_msg*>::iterator
mpi_cmd_buffer::end(void)
{
    return msgs_.end();
}

nnti::core::mpi_cmd_msg*
mpi_cmd_buffer::cmd_msg(int index)
{
    return msgs_[index];
}

void
mpi_cmd_buffer::setup_command_buffer(void)
{
    log_debug("mpi_cmd_buffer", "setup_command_buffer: enter (count=%u size=%u)", cmd_count_, cmd_size_);

    cmd_buf_ = new char[cmd_size_ * cmd_count_];

    for (uint32_t i=0;i<cmd_count_;i++) {
        char *cmd_addr = cmd_buf_+(cmd_size_ * i);
        log_debug("mpi_cmd_buffer", "cmd_addr = %p = %lx + (%u * %d)", cmd_addr, cmd_buf_, cmd_size_, i);
        nnti::core::mpi_cmd_msg *cm = new nnti::core::mpi_cmd_msg(transport_, this, cmd_addr, cmd_size_);
        msgs_.push_back(cm);
        cm->post_recv();
    }

    log_debug("mpi_cmd_buffer", "setup_command_buffer: exit (cmd_buf_=%p)", cmd_buf_);
}
void
mpi_cmd_buffer::teardown_command_buffer(void)
{
    log_debug("mpi_cmd_buffer", "teardown_command_buffer: enter");

    for (uint32_t i=0;i<cmd_count_;i++) {
        MPI_Cancel(&msgs_[i]->cmd_request());
        delete msgs_[i];
    }
    delete[] cmd_buf_;

    log_debug("mpi_cmd_buffer", "teardown_command_buffer: exit");
}

} /* namespace core */
} /* namespace nnti */
