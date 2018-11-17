// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef IBVERBS_CMD_OP_HPP_
#define IBVERBS_CMD_OP_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_msg.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_msg.hpp"


namespace nnti {
namespace core {

class ibverbs_cmd_op
: public nnti_op {
private:
    nnti::transports::ibverbs_transport *transport_;
    nnti::core::ibverbs_cmd_msg          cmd_msg_;
    bool                                 is_ack_;
    struct ibv_sge                       sge_;
    struct ibv_send_wr                   sq_wr_;
    struct ibv_mr                       *cmd_mr_;

public:
    ibverbs_cmd_op(
        nnti::transports::ibverbs_transport *transport,
        const uint32_t                       cmd_msg_size)
    : nnti_op(),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size),
      is_ack_(false),
      cmd_mr_(nullptr)
    {
        register_cmd_msg();

        sq_wr_.wr_id      = (uint64_t)this;
        sq_wr_.next       = nullptr;
        sq_wr_.sg_list    = &sge_;
        sq_wr_.num_sge    = 1;
        sq_wr_.opcode     = IBV_WR_SEND;
        sq_wr_.send_flags = IBV_SEND_SIGNALED;

        sge_.addr   = (uint64_t)cmd_mr_->addr;
        sge_.length = cmd_msg_.size();
        sge_.lkey   = cmd_mr_->lkey;

        return;
    }
    ibverbs_cmd_op(
        nnti::transports::ibverbs_transport *transport,
        const uint32_t                       cmd_msg_size,
        nnti::datatype::nnti_work_id        *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size, id_, wid),
      is_ack_(false),
      cmd_mr_(nullptr)
    {
        if (!(wid->wr().flags() & NNTI_OF_ZERO_COPY)) {
            register_cmd_msg();
        }

        sq_wr_.wr_id      = (uint64_t)this;
        sq_wr_.next       = nullptr;
        sq_wr_.sg_list    = &sge_;
        sq_wr_.num_sge    = 1;
        sq_wr_.opcode     = IBV_WR_SEND;
        sq_wr_.send_flags = IBV_SEND_SIGNALED;

        sge_.addr   = (uint64_t)cmd_mr_->addr;
        sge_.length = cmd_msg_.size();
        sge_.lkey   = cmd_mr_->lkey;

        return;
    }
    ibverbs_cmd_op(
        nnti::transports::ibverbs_transport *transport,
        nnti::datatype::nnti_work_id        *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, id_, wid),
      cmd_mr_(nullptr),
      is_ack_(false)
    {
        sq_wr_.wr_id      = (uint64_t)this;
        sq_wr_.next       = nullptr;
        sq_wr_.sg_list    = &sge_;
        sq_wr_.num_sge    = 1;
        sq_wr_.opcode     = IBV_WR_SEND;
        sq_wr_.send_flags = IBV_SEND_SIGNALED;

        if ((wid->wr().flags() & NNTI_OF_ZERO_COPY)) {
            nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)wid->wr().local_hdl();
            sge_.addr   = (uint64_t)b->addr()+wid->wr().local_offset();
            sge_.lkey   = b->lkey();
        } else {
            register_cmd_msg();

            sge_.addr   = (uint64_t)cmd_mr_->addr;
            sge_.lkey   = cmd_mr_->lkey;
        }

        sge_.length = cmd_msg_.header_length();
        if (cmd_msg_.eager()) {
            sge_.length += cmd_msg_.payload_length();
        }

        return;
    }

  ~ibverbs_cmd_op() override {
        if ((wid_) && !(wid_->wr().flags() & NNTI_OF_ZERO_COPY)) {
            log_debug("ibverbs_buffer", "deregistering ibverbs_cmd_op (cmd_msg_.buf()=%x)", cmd_mr_->addr);
            ibv_dereg_mr(cmd_mr_);
        }
        return;
    }

    void
    set(nnti::datatype::nnti_work_id *wid)
    {
        id_  = next_id_.fetch_add(1);
        wid_ = wid;
        cmd_msg_.set(id_, wid);
        is_ack_ = false;
        sge_.length = cmd_msg_.header_length();
        if (cmd_msg_.eager()) {
            sge_.length += cmd_msg_.payload_length();
        }
    }

    void
    set(uint32_t src_op_id)
    {
        id_  = next_id_.fetch_add(1);
        cmd_msg_.set(id_, src_op_id);
        is_ack_ = true;
    }

    bool
    eager(void)
    {
        return cmd_msg_.eager();
    }

    bool
    ack(void)
    {
        return is_ack_;
    }

    struct ibv_send_wr*
    sq_wr(void)
    {
        return &sq_wr_;
    }

    void
    src_op_id(uint32_t soi)
    {
        cmd_msg_.src_op_id(soi);
    }
    uint32_t
    src_op_id(void)
    {
        return cmd_msg_.src_op_id();
    }

  std::string
    toString(void) override {
        std::stringstream out;
        out << "id_==" << id_;
        return out.str();
    }

private:
    void
    register_cmd_msg(void)
    {
        log_debug("ibverbs_cmd_op", "registering ibverbs_cmd_op (cmd_msg_.buf()=%x)", cmd_msg_.buf());
        cmd_mr_ = ibv_reg_mr(transport_->pd_, cmd_msg_.buf(), cmd_msg_.size(), 0);
        if (!cmd_mr_) {
            log_error("ibverbs_cmd_op", "failed to register memory region: %s", strerror(errno));
            return;
        }

        return;
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* IBVERBS_CMD_OP_HPP_ */
