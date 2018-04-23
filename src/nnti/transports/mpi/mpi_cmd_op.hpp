// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef MPI_CMD_OP_HPP_
#define MPI_CMD_OP_HPP_

#include "nnti/nntiConfig.h"

#include <mpi.h>

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
#include "nnti/transports/mpi/mpi_cmd_msg.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_cmd_msg.hpp"


namespace nnti {
namespace core {

class mpi_cmd_op
: public nnti_op {
private:
    enum cmd_state {
        SEND_INIT=1,
        SEND_COMPLETE,
        RECV_INIT,
        RECV_COMPLETE,
        RDMA_WRITE_INIT,
        RDMA_RTS_COMPLETE,
        RDMA_WRITE_COMPLETE,
        RDMA_READ_INIT,
        RDMA_RTR_COMPLETE,
        RDMA_READ_COMPLETE
    };
    struct atomic_op_header_t {
        int64_t operand1;
        int64_t operand2;
    };

private:
    MPI_Request                      cmd_request_;
    MPI_Request                      long_send_request_;
    MPI_Request                      rdma_request_;
//    MPI_Request                      get_recv_request_;
//    MPI_Request                      get_send_request_;
//    MPI_Request                      put_recv_request_;
//    MPI_Request                      put_send_request_;

    size_t                           index_;

    nnti::transports::mpi_transport *transport_;
    nnti::core::mpi_cmd_msg          cmd_msg_;

    enum cmd_state                   state_;

public:
    mpi_cmd_op(
        nnti::transports::mpi_transport *transport,
        const uint32_t                   cmd_msg_size)
    : nnti_op(),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size)
    {
        return;
    }
    mpi_cmd_op(
        nnti::transports::mpi_transport *transport,
        const uint32_t                   cmd_msg_size,
        nnti::datatype::nnti_work_id    *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size)
    {
        set(wid);
        return;
    }
    mpi_cmd_op(
        nnti::transports::mpi_transport *transport,
        nnti::datatype::nnti_work_id    *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, id_, wid)
    {
        return;
    }
    virtual ~mpi_cmd_op()
    {
        return;
    }

    void
    set(nnti::datatype::nnti_work_id *wid)
    {
        id_  = next_id_.fetch_add(1);
        wid_ = wid;
        cmd_msg_.set(id_, wid);
        if (wid_->wr().op() == NNTI_OP_ATOMIC_FADD) {
            atomic_op_header_t *hdr = (atomic_op_header_t *)cmd_msg_.eager_payload();
            hdr->operand1 = wid_->wr().operand1();
        }
        if (wid_->wr().op() == NNTI_OP_ATOMIC_CSWAP) {
            atomic_op_header_t *hdr = (atomic_op_header_t *)cmd_msg_.eager_payload();
            hdr->operand1 = wid_->wr().operand1();
            hdr->operand2 = wid_->wr().operand2();
        }
    }

    bool
    eager(void)
    {
        return cmd_msg_.eager();
    }

    MPI_Request&
    cmd_request(void)
    {
        return cmd_request_;
    }
    MPI_Request&
    long_send_request(void)
    {
        return long_send_request_;
    }
    MPI_Request&
    rdma_request(void)
    {
        return rdma_request_;
    }
//    MPI_Request&
//    get_recv_request(void)
//    {
//        return get_recv_request_;
//    }
//    MPI_Request&
//    get_send_request(void)
//    {
//        return get_send_request_;
//    }
//    MPI_Request&
//    put_recv_request(void)
//    {
//        return put_recv_request_;
//    }
//    MPI_Request&
//    put_send_request(void)
//    {
//        return put_send_request_;
//    }

    void
    index(size_t index)
    {
        index_ = index;
    }
    size_t
    index(void)
    {
        return index_;
    }

    char *
    cmd_msg(void)
    {
        cmd_msg_.buf();
    }
    uint32_t
    cmd_msg_size(void)
    {
        cmd_msg_.size();
    }

    virtual std::string
    toString(void)
    {
        std::stringstream out;
        out << "id_==" << id_;
        return out.str();
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* MPI_CMD_OP_HPP_ */
