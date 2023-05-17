// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef MPI_CMD_MSG_HPP_
#define MPI_CMD_MSG_HPP_

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_buffer.hpp"
#include "nnti/transports/mpi/mpi_cmd_buffer.hpp"
#include "nnti/transports/mpi/mpi_peer.hpp"


namespace nnti {
namespace core {

class mpi_cmd_msg {
private:
    const static uint32_t packed_buffer_size_ = 180;

    struct cmd_msg {
        uint64_t cmd_header_size;  // 8
        NNTI_process_id_t initiator;   // 8
        uint64_t initiator_offset; // 8
        uint64_t target_offset;    // 8
        uint64_t payload_length;   // 8
        uint64_t target_base_addr; // 8
        uint32_t id;               // 4
        uint8_t  op;               // 1
        char     packed_initiator_hdl[packed_buffer_size_]; // 180
        // total header is 233 bytes
        // eager_payload[] is a just a place holder.  The actual
        // eager payload size is number of bytes allocated to for
        // each command message minus 233 bytes of header.
        char     eager_payload[1];
    };

private:
    MPI_Request                      request_;
    size_t                           index_;

    nnti::transports::mpi_transport *transport_;
    struct cmd_msg                  *cmd_msg_buf_;
    uint32_t                         cmd_msg_size_;

    bool free_in_dtor_;

    bool unexpected_;

    nnti::datatype::mpi_peer        *initiator_peer_;
    nnti::datatype::mpi_buffer      *initiator_hdl_;
    nnti::datatype::mpi_buffer      *target_hdl_;

    bool initiator_hdl_valid_;
    bool target_hdl_valid_;

public:
    mpi_cmd_msg(
        nnti::transports::mpi_transport *transport,
        const uint32_t                   cmd_msg_size);
    mpi_cmd_msg(
        nnti::transports::mpi_transport *transport,
        uint32_t                         id,
        nnti::datatype::nnti_work_id    *wid);
    mpi_cmd_msg(
        nnti::transports::mpi_transport *transport,
        nnti::core::mpi_cmd_buffer      *cmd_buf,
        char                            *buf,
        uint32_t                         buf_size);
    virtual ~mpi_cmd_msg();

    MPI_Request&
    cmd_request(void);
    void
    index(
        size_t index);
    size_t
    index(void);

    void
    set(
        uint32_t                      id,
        nnti::datatype::nnti_work_id *wid);

    char *
    buf(void);
    uint32_t
    size(void);

    void
    unpack(void);

    static uint64_t
    header_length(void);

    bool
    unexpected(void);

    uint8_t
    op(void);

    uint64_t
    initiator_offset(void);
    uint64_t
    target_offset(void);

    nnti::datatype::mpi_peer*
    initiator_peer(void);
    nnti::datatype::mpi_buffer*
    initiator_buffer(void);
    nnti::datatype::mpi_buffer*
    target_buffer(void);
    bool
    eager(void);
    char *
    eager_payload(void);
    uint64_t
    payload_length(void);

    void
    post_recv(void);

    virtual std::string
    toString(void);

private:
    void
    get_cmd_msg_buffer(void);

    void
    pack(
        uint32_t                      id,
        nnti::datatype::nnti_work_id *wid);
};

} /* namespace core */
} /* namespace nnti */

#endif /* MPI_CMD_MSG_HPP_ */
