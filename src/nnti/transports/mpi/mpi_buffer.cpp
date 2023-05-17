// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <assert.h>

#include <deque>
#include <map>
#include <sstream>
#include <string>

#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"

#include "nnti/nnti_callback.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_buffer.hpp"


namespace nnti  {
namespace datatype {

std::atomic<uint32_t> mpi_buffer::buffer_tag_counter_ = {mpi_buffer::max_reserved_tag_ + 1};


mpi_buffer::mpi_buffer()
    : nnti_buffer()
{
    return;
}
//mpi_buffer::mpi_buffer(
//    nnti::transports::mpi_transport *transport,
//    NNTI_buffer_p_t                     *packable)
//: nnti_buffer(transport,
//              packable)
//{
//    return;
//}
mpi_buffer::mpi_buffer(
    nnti::datatype::mpi_buffer &b)
: nnti_buffer(b)
{
    memcpy(packed_, b.packed_, packed_size_);

    return;
}
mpi_buffer::mpi_buffer(
    nnti::transports::mpi_transport *transport,
    const uint64_t                   size,
    const NNTI_buffer_flags_t        flags,
    NNTI_event_queue_t               eq,
    nnti_event_callback              cb,
    void                            *cb_context)
: nnti_buffer(transport,
              size,
              flags,
              eq,
              cb,
              cb_context)
{
    register_buffer();
    internal_pack();

    return;
}
mpi_buffer::mpi_buffer(
    nnti::transports::mpi_transport *transport,
    char                            *buffer,
    const uint64_t                   size,
    const NNTI_buffer_flags_t        flags,
    NNTI_event_queue_t               eq,
    nnti_event_callback              cb,
    void                            *cb_context)
: nnti_buffer(transport,
              buffer,
              size,
              flags,
              eq,
              cb,
              cb_context)
{
    register_buffer();
    internal_pack();

    return;
}
mpi_buffer::mpi_buffer(
    nnti::transports::transport *transport,
    char                        *packed_buf,
    const uint64_t               packed_len)
: nnti_buffer(transport,
              packed_buf,
              packed_len)
{
    payload_      = (char *)packable_.buffer.NNTI_remote_addr_p_t_u.mpi.buf;
    payload_size_ = packable_.buffer.NNTI_remote_addr_p_t_u.mpi.size;

    log_debug("mpi_buffer", "ctor unpack - segments[0].size(%u)",
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.size);

    return;
}

mpi_buffer::~mpi_buffer()
{
    return;
}

char*
mpi_buffer::payload(void)
{
    log_debug("mpi_buffer", "payload enter");
    if (payload_ == nullptr) {
        log_error("mpi_buffer", "remote buffer doesn't have a payload.");
        return nullptr;
    } else {
        log_debug("mpi_buffer", "returning payload_");
        return payload_;
    }
}

size_t
mpi_buffer::length(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.mpi.size;
}

uint32_t
mpi_buffer::cmd_tag(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.mpi.cmd_tag;
}

uint32_t
mpi_buffer::get_tag(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.mpi.get_data_tag;
}

uint32_t
mpi_buffer::put_tag(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.mpi.put_data_tag;
}

uint32_t
mpi_buffer::atomic_tag(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.mpi.atomic_data_tag;
}


NNTI_result_t
mpi_buffer::register_buffer(void)
{
    NNTI_result_t         rc = NNTI_OK;

    log_debug("mpi_buffer", "enter buffer(%p) len(%d)", payload_, payload_size_);

    memset(&packable_, 0, sizeof(NNTI_buffer_p_t));

    packable_.buffer.transport_id                               = NNTI_TRANSPORT_MPI;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.buf             = (uint64_t)payload_;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.size            = payload_size_;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.cmd_tag         = buffer_tag_counter_++;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.get_data_tag    = buffer_tag_counter_++;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.put_data_tag    = buffer_tag_counter_++;
    packable_.buffer.NNTI_remote_addr_p_t_u.mpi.atomic_data_tag = buffer_tag_counter_++;

    log_debug("mpi_buffer", "exit (payload_==%p, buf==%p, size==%u, cmd_tag==%u, get_data_tag=%u, put_data_tag=%u, atomic_data_tag=%u)",
        payload_,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.buf,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.size,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.cmd_tag,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.get_data_tag,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.put_data_tag,
        packable_.buffer.NNTI_remote_addr_p_t_u.mpi.atomic_data_tag);

    return rc;
}

NNTI_result_t
mpi_buffer::post_receive(void)
{
    NNTI_result_t         rc = NNTI_OK;

    log_debug("mpi_buffer::post_receive", "enter");

//    MPI_Irecv();

    log_debug("mpi_buffer::post_receive", "exit");

    return rc;
}

} /* namespace datatype */
} /* namespace nnti */
