// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef IBVERBS_RDMA_OP_HPP_
#define IBVERBS_RDMA_OP_HPP_

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
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_wr.hpp"


namespace nnti {
namespace core {

class ibverbs_rdma_op
: public nnti_op {
private:
    nnti::transports::ibverbs_transport *transport_;
    struct ibv_sge                       sge_;
    struct ibv_send_wr                   sq_wr_;

public:
    ibverbs_rdma_op(
        nnti::transports::ibverbs_transport *transport)
    : nnti_op(),
      transport_(transport)
    {
        sq_wr_.wr_id      = (uint64_t)this;
        sq_wr_.next       = nullptr;
        sq_wr_.sg_list    = &sge_;
        sq_wr_.num_sge    = 1;

        sq_wr_.send_flags = IBV_SEND_SIGNALED;

        return;
    }
    ibverbs_rdma_op(
        nnti::transports::ibverbs_transport *transport,
        nnti::datatype::nnti_work_id        *wid)
    : nnti_op(wid),
      transport_(transport)
    {
        const nnti::datatype::ibverbs_work_request &wr = (const nnti::datatype::ibverbs_work_request &)wid->wr();

        sq_wr_.wr_id      = (uint64_t)this;
        sq_wr_.next       = nullptr;
        sq_wr_.sg_list    = &sge_;
        sq_wr_.num_sge    = 1;

        if (wr.op() == NNTI_OP_PUT) {
            sq_wr_.opcode = IBV_WR_RDMA_WRITE;
        }
        if (wr.op() == NNTI_OP_GET) {
            sq_wr_.opcode = IBV_WR_RDMA_READ;
        }

        sq_wr_.send_flags = IBV_SEND_SIGNALED;
        sq_wr_.imm_data   = id();

        sq_wr_.wr.rdma.remote_addr = (uint64_t)wr.remote_addr() + wr.remote_offset();
        sq_wr_.wr.rdma.rkey        = wr.remote_rkey();

        sge_.addr   = (uint64_t)wr.local_addr() + wr.local_offset();
        sge_.length = wr.length();
        sge_.lkey   = wr.local_lkey();

        return;
    }

  ~ibverbs_rdma_op() override {
        return;
    }

    void
    set(nnti::datatype::nnti_work_id *wid)
    {
        const nnti::datatype::ibverbs_work_request &wr = (const nnti::datatype::ibverbs_work_request &)wid->wr();

        id_  = next_id_.fetch_add(1);
        wid_ = wid;

        if (wr.op() == NNTI_OP_PUT) {
            sq_wr_.opcode = IBV_WR_RDMA_WRITE;
        }
        if (wr.op() == NNTI_OP_GET) {
            sq_wr_.opcode = IBV_WR_RDMA_READ;
        }

        sq_wr_.imm_data   = id();

        sq_wr_.wr.rdma.remote_addr = (uint64_t)wr.remote_addr() + wr.remote_offset();
        sq_wr_.wr.rdma.rkey        = wr.remote_rkey();

        sge_.addr   = (uint64_t)wr.local_addr() + wr.local_offset();
        sge_.length = wr.length();
        sge_.lkey   = wr.local_lkey();

        return;
    }

    struct ibv_send_wr*
    sq_wr(void)
    {
        return &sq_wr_;
    }

  std::string
    toString() override {
        std::stringstream out;
        out << "id_=="         << id_
            << " wr_id=="       << (void*)sq_wr_.wr_id
            << " opcode=="      << sq_wr_.opcode
            << " send_flags=="  << sq_wr_.send_flags
            << " imm_data=="    << sq_wr_.imm_data
            << " remote_addr==" << (void*)sq_wr_.wr.rdma.remote_addr
            << " rkey=="        << std::hex << sq_wr_.wr.rdma.rkey << std::dec
            << " addr=="        << (void*)sq_wr_.sg_list[0].addr
            << " length=="      << sq_wr_.sg_list[0].length
            << " lkey=="        << std::hex << sq_wr_.sg_list[0].lkey << std::dec;
        return out.str();
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* IBVERBS_RDMA_OP_HPP_ */
