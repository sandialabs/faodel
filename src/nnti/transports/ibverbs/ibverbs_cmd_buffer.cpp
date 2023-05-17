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

#include <infiniband/verbs.h>

#include "nnti/transports/ibverbs/ibverbs_cmd_buffer.hpp"


namespace nnti {
namespace core {

ibverbs_cmd_buffer::ibverbs_cmd_buffer(
    nnti::transports::ibverbs_transport *transport,
    nnti::core::ibverbs_connection      *conn,
    const uint32_t                       cmd_size,
    const uint32_t                       cmd_count)
: transport_(transport),
  conn_(conn),
  cmd_size_(cmd_size),
  cmd_count_(cmd_count),
  cmd_offset_(0)
{
    setup_command_buffer();
}
ibverbs_cmd_buffer::~ibverbs_cmd_buffer()
{
    teardown_command_buffer();

    return;
}

void
ibverbs_cmd_buffer::post_recv(
    nnti::core::ibverbs_cmd_msg *cmd_msg)
{
    int ibv_rc;
    struct ibv_sge     sge;
    struct ibv_recv_wr rq_wr, *bad_wr;

    sge.addr   = (uint64_t)cmd_msg->buf();
    sge.length = cmd_msg->size();
    sge.lkey   = cmd_mr_->lkey;

    rq_wr.next    = nullptr;
    rq_wr.sg_list = &sge;
    rq_wr.num_sge = 1;
    rq_wr.wr_id = (uint64_t)cmd_msg;

    log_debug("ibverbs_cmd_buffer", "post_recv() - cmd_msg=%p - sge.addr=%lx ; sge.length=%lu ; sge.lkey=%x",
        cmd_msg, sge.addr, sge.length, sge.lkey);

    ibv_rc=ibv_post_srq_recv(transport_->cmd_srq_, &rq_wr, &bad_wr);
    if (ibv_rc) {
        log_error("ibverbs_cmd_buffer", "failed to post SRQ recv (rq_wr=%p ; bad_wr=%p): %s",
                &rq_wr, bad_wr, strerror(ibv_rc));
    }
}

void
ibverbs_cmd_buffer::setup_command_buffer(void)
{
    enum ibv_access_flags ibv_flags = static_cast<enum ibv_access_flags>(IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);

    log_debug("ibverbs_cmd_buffer", "setup_command_buffer: enter");

    cmd_buf_ = new char[cmd_size_ * cmd_count_];

//    if (transport_->use_odp_) {
//        cmd_mr_ = transport_->odp_mr_;
//    } else {
        log_debug("ibverbs_cmd_buffer", "registering ibverbs_cmd_buffer (cmd_buf_=%x)", cmd_buf_);
        cmd_mr_  = ibv_reg_mr(transport_->pd_, cmd_buf_, cmd_size_ * cmd_count_, ibv_flags);
//    }

    for (uint32_t i=0;i<cmd_count_;i++) {
        char *cmd_addr = (char*)cmd_buf_+(cmd_size_ * i);
        log_debug("ibverbs_cmd_buffer", "cmd_addr = %p = %lx + (%u * %d)", cmd_addr, cmd_buf_, cmd_size_, i);
        nnti::core::ibverbs_cmd_msg *cm = new nnti::core::ibverbs_cmd_msg(transport_, this, cmd_addr, cmd_size_);
        msgs_.push_back(cm);
        post_recv(cm);
    }

    log_debug("ibverbs_cmd_buffer", "setup_command_buffer: exit (cmd_buf_=%p  cmd_mr_=%p)", cmd_buf_, cmd_mr_);
}
void
ibverbs_cmd_buffer::teardown_command_buffer(void)
{
    log_debug("ibverbs_cmd_buffer", "teardown_command_buffer: enter");

    for (uint32_t i=0;i<cmd_count_;i++) {
        delete msgs_[i];
    }
    if (transport_->use_odp_) {
        log_debug("ibverbs_buffer", "using ODP - unregister is a no-op");
    } else {
        log_debug("ibverbs_cmd_buffer", "deregistering ibverbs_cmd_buffer (cmd_buf_=%x)", cmd_buf_);
        ibv_dereg_mr(cmd_mr_);
    }
    delete[] cmd_buf_;

    log_debug("ibverbs_cmd_buffer", "teardown_command_buffer: exit");
}

} /* namespace core */
} /* namespace nnti */
