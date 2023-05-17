// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_cmd_msg.hpp"
#include "nnti/transports/mpi/mpi_connection.hpp"


namespace nnti {
namespace core {

mpi_cmd_msg::mpi_cmd_msg(
    nnti::transports::mpi_transport *transport,
    const uint32_t                   cmd_msg_size)
: transport_(transport),
  cmd_msg_buf_(nullptr),
  cmd_msg_size_(cmd_msg_size),
  free_in_dtor_(true),
  unexpected_(false)
{
    get_cmd_msg_buffer();

    return;
}
mpi_cmd_msg::mpi_cmd_msg(
    nnti::transports::mpi_transport *transport,
    uint32_t                         id,
    nnti::datatype::nnti_work_id    *wid)
: transport_(transport),
  cmd_msg_buf_(nullptr),
  unexpected_(false)
{
    if (wid->wr().flags() & NNTI_OF_ZERO_COPY) {
        nnti::datatype::mpi_buffer *b = (nnti::datatype::mpi_buffer *)wid->wr().local_hdl();
        cmd_msg_size_ = wid->wr().length();
        cmd_msg_buf_  = (struct cmd_msg *)(b->payload() + wid->wr().local_offset());
        free_in_dtor_ = false;
    } else {
        cmd_msg_size_ = wid->wr().length();
        get_cmd_msg_buffer();
        free_in_dtor_ = true;
    }
    pack(id, wid);

    return;
}
mpi_cmd_msg::mpi_cmd_msg(
    nnti::transports::mpi_transport *transport,
    nnti::core::mpi_cmd_buffer      *cmd_buf,
    char                            *buf,
    uint32_t                         buf_size)
: transport_(transport),
  cmd_msg_buf_((struct cmd_msg*)buf),
  cmd_msg_size_(buf_size),
  free_in_dtor_(false),
  unexpected_(false)
{
    return;
}
mpi_cmd_msg::~mpi_cmd_msg()
{
    if (free_in_dtor_) free(cmd_msg_buf_);
    return;
}

MPI_Request&
mpi_cmd_msg::cmd_request(void)
{
    return request_;
}

void
mpi_cmd_msg::index(
    size_t index)
{
    index_ = index;
}
size_t
mpi_cmd_msg::index(void)
{
    return index_;
}

void
mpi_cmd_msg::set(
    uint32_t                      id,
    nnti::datatype::nnti_work_id *wid)
{
    pack(id, wid);
}

char *
mpi_cmd_msg::buf(void)
{
    return (char *)cmd_msg_buf_;
}
uint32_t
mpi_cmd_msg::size(void)
{
    return cmd_msg_size_;
}

void
mpi_cmd_msg::unpack(void)
{
    log_debug("mpi_cmd_msg", "unpack - enter");

    initiator_peer_ = (nnti::datatype::mpi_peer*)transport_->conn_map_.get(cmd_msg_buf_->initiator)->peer();

    if (*(uint32_t*)cmd_msg_buf_->packed_initiator_hdl != 0) {
        initiator_hdl_ = (nnti::datatype::mpi_buffer*)transport_->unpack_buffer(cmd_msg_buf_->packed_initiator_hdl,
                                  packed_buffer_size_);
        initiator_hdl_valid_ = true;
    } else {
        initiator_hdl_valid_ = false;
    }

    log_debug("mpi_cmd_msg", "unpacking message id(%u) from %s with target_base_addr(%lu)",
        cmd_msg_buf_->id, initiator_peer()->url().url().c_str(), cmd_msg_buf_->target_base_addr);

    if (cmd_msg_buf_->target_base_addr != 0) {
        target_hdl_ = (nnti::datatype::mpi_buffer*)transport_->buffer_map_.get((char*)cmd_msg_buf_->target_base_addr);
        target_hdl_valid_ = true;
        unexpected_ = false;
    } else {
        target_hdl_valid_ = false;
        unexpected_ = true;
    }

    log_debug_stream("mpi_cmd_msg") << toString() << std::endl;

    log_debug("mpi_cmd_msg", "unpack - exit");

    return;
}

uint64_t
mpi_cmd_msg::header_length(void)
{
    return offsetof(struct cmd_msg, eager_payload);
}

bool
mpi_cmd_msg::unexpected(void)
{
    return unexpected_;
}

uint8_t
mpi_cmd_msg::op(void)
{
    return cmd_msg_buf_->op;
}

uint64_t
mpi_cmd_msg::initiator_offset(void)
{
    return cmd_msg_buf_->initiator_offset;
}
uint64_t
mpi_cmd_msg::target_offset(void)
{
    return cmd_msg_buf_->target_offset;
}

nnti::datatype::mpi_peer*
mpi_cmd_msg::initiator_peer(void)
{
    return initiator_peer_;
}
nnti::datatype::mpi_buffer*
mpi_cmd_msg::initiator_buffer(void)
{
    return initiator_hdl_;
}
nnti::datatype::mpi_buffer*
mpi_cmd_msg::target_buffer(void)
{
    return target_hdl_;
}
bool
mpi_cmd_msg::eager(void)
{
    return (cmd_msg_buf_->payload_length <= (size()-header_length()));
}
char *
mpi_cmd_msg::eager_payload(void)
{
    return cmd_msg_buf_->eager_payload;
}
uint64_t
mpi_cmd_msg::payload_length(void)
{
    return cmd_msg_buf_->payload_length;
}

void
mpi_cmd_msg::post_recv()
{
    MPI_Irecv(
        buf(),
        size(),
        MPI_BYTE,
        MPI_ANY_SOURCE,
        nnti::transports::mpi_transport::NNTI_MPI_CMD_TAG,
        MPI_COMM_WORLD,
        &cmd_request());
}

std::string
mpi_cmd_msg::toString(void)
{
    std::stringstream out;
    out << "  mpi_cmd_msg.buf() = "               << buf()
        << " | mpi_cmd_msg.size() = "             << size()
        << " | mpi_cmd_msg.header_length() = "    << header_length()
        << " | mpi_cmd_msg.unexpected() = "       << unexpected()
        << " | mpi_cmd_msg.op() = "               << op()
        << " | mpi_cmd_msg.initiator_offset() = " << initiator_offset()
        << " | mpi_cmd_msg.target_offset() = "    << target_offset()
        << " | mpi_cmd_msg.initiator_peer() = "   << initiator_peer()
        << " | mpi_cmd_msg.initiator_peer().url() = " << initiator_peer()->url().url()
        << " | mpi_cmd_msg.initiator_buffer() = " << initiator_buffer()
        << " | mpi_cmd_msg.target_buffer() = "    << target_buffer()
        << " | mpi_cmd_msg.eager() = "            << eager()
        << " | mpi_cmd_msg.eager_payload() = "    << eager_payload()
        << " | mpi_cmd_msg.payload_length() = "   << payload_length();
    return out.str();
}

void
mpi_cmd_msg::get_cmd_msg_buffer(void)
{
    cmd_msg_buf_ = (struct cmd_msg *)malloc(sizeof(uint8_t) * cmd_msg_size_);

    return;
}

void
mpi_cmd_msg::pack(
    uint32_t                      id,
    nnti::datatype::nnti_work_id *wid)
{
    const nnti::datatype::nnti_work_request &wr = wid->wr();
    nnti::datatype::mpi_buffer *buf;

    log_debug("mpi_cmd_msg", "pack - enter");

    memset(cmd_msg_buf_, 0, header_length());

    cmd_msg_buf_->cmd_header_size  = offsetof(struct cmd_msg, eager_payload);
    cmd_msg_buf_->id               = id;
    cmd_msg_buf_->op               = wr.op();
    cmd_msg_buf_->initiator        = transport_->me_.pid();
    cmd_msg_buf_->initiator_offset = wr.local_offset();
    cmd_msg_buf_->target_offset    = wr.remote_offset();
    cmd_msg_buf_->payload_length   = wr.length();
    if (wid->wr().flags() & NNTI_OF_ZERO_COPY) {
        cmd_msg_buf_->payload_length -= header_length();
    }

    if (wr.local_hdl() != NNTI_INVALID_HANDLE) {
        buf = (nnti::datatype::mpi_buffer *)wr.local_hdl();
        buf->pack(cmd_msg_buf_->packed_initiator_hdl, packed_buffer_size_);

        if (!(wid->wr().flags() & NNTI_OF_ZERO_COPY) && eager()) {
            // message is small, use eager
            log_debug("mpi_cmd_msg", "payload=%08x  offset=%lu  length=%lu",
                      buf->payload(), cmd_msg_buf_->initiator_offset, cmd_msg_buf_->payload_length);
            memcpy(cmd_msg_buf_->eager_payload, buf->payload()+cmd_msg_buf_->initiator_offset, cmd_msg_buf_->payload_length);
        }
    } else {
        *(uint32_t*)cmd_msg_buf_->packed_initiator_hdl = 0;
    }

    if (wr.remote_hdl() != NNTI_INVALID_HANDLE) {
        buf = (nnti::datatype::mpi_buffer *)wr.remote_hdl();
        cmd_msg_buf_->target_base_addr = (uint64_t)buf->payload();
        unexpected_ = false;
    } else {
        cmd_msg_buf_->target_base_addr = 0;
        unexpected_ = true;
    }

    nnti::datatype::nnti_peer *peer = (nnti::datatype::nnti_peer *)wid->wr().peer();
    log_debug("mpi_cmd_msg", "packing message id(%u) from %s to %s with target_base_addr(%lu)",
        cmd_msg_buf_->id, transport_->me_.url().url().c_str(), peer->url().url().c_str(), cmd_msg_buf_->target_base_addr);

    log_debug("mpi_cmd_msg", "pack - exit");

    return;
}

} /* namespace core */
} /* namespace nnti */
