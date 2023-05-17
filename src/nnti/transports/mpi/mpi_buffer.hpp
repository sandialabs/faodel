// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef MPI_BUFFER_HPP_
#define MPI_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <assert.h>

#include <map>
#include <sstream>
#include <string>

#include <mpi.h>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_callback.hpp"


namespace nnti  {

namespace transports {
    // forward declaration
    class mpi_transport;
}

namespace datatype {

class mpi_buffer
: public nnti_buffer {
private:
    const static uint32_t        max_reserved_tag_ = 128;
    static std::atomic<uint32_t> buffer_tag_counter_;

private:
    MPI_Request request;

private:
    NNTI_result_t
    register_buffer(void);
    NNTI_result_t
    register_segments(void);
    NNTI_result_t
    post_receive(void);

public:
    mpi_buffer(void);
//    mpi_buffer(
//        nnti::transports::mpi_transport *transport,
//        NNTI_buffer_p_t                 *packable);
    mpi_buffer(
        nnti::datatype::mpi_buffer &b);
    mpi_buffer(
        nnti::transports::mpi_transport *transport,
        const uint64_t                   size,
        const NNTI_buffer_flags_t        flags,
        NNTI_event_queue_t               eq,
        nnti_event_callback              cb,
        void                            *cb_context);
    mpi_buffer(
        nnti::transports::mpi_transport *transport,
        char                            *buffer,
        const uint64_t                   size,
        const NNTI_buffer_flags_t        flags,
        NNTI_event_queue_t               eq,
        nnti_event_callback              cb,
        void                            *cb_context);
    mpi_buffer(
        nnti::transports::transport *transport,
        char                        *packed_buf,
        const uint64_t               packed_len);
    virtual ~mpi_buffer();

  char*
    payload(void) override;
    size_t
    length(void);
    uint32_t
    cmd_tag(void);
    uint32_t
    get_tag(void);
    uint32_t
    put_tag(void);
    uint32_t
    atomic_tag(void);
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* MPI_BUFFER_HPP_ */
