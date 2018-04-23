// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/transports/ugni/ugni_connection.hpp"


namespace nnti {
namespace core {

ugni_cmd_msg::ugni_cmd_msg(
    nnti::transports::ugni_transport *transport,
    const uint32_t                    cmd_msg_size)
: transport_(transport),
  initiator_peer_(nullptr),
  target_peer_(nullptr),
  cmd_msg_buf_(nullptr),
  cmd_msg_size_(cmd_msg_size),
  free_in_dtor_(true),
  unexpected_(false)
{
    get_cmd_msg_buffer();

    return;
}
ugni_cmd_msg::ugni_cmd_msg(
    nnti::transports::ugni_transport *transport,
    const uint32_t                    cmd_msg_size,
    uint32_t                          id,
    nnti::datatype::nnti_work_id     *wid)
: transport_(transport),
  initiator_peer_(nullptr),
  target_peer_(nullptr),
  cmd_msg_buf_(nullptr),
  cmd_msg_size_(cmd_msg_size),
  free_in_dtor_(true),
  unexpected_(false)
{
    get_cmd_msg_buffer();
    pack(id, wid);

    return;
}
ugni_cmd_msg::ugni_cmd_msg(
    nnti::transports::ugni_transport *transport,
    uint32_t                          id,
    nnti::datatype::nnti_work_id     *wid)
: transport_(transport),
  initiator_peer_(nullptr),
  target_peer_(nullptr),
  cmd_msg_buf_(nullptr),
  cmd_msg_size_(2048),
  free_in_dtor_(false),
  unexpected_(false)
{
    if (wid->wr().flags() & NNTI_OF_ZERO_COPY) {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)wid->wr().local_hdl();
        cmd_msg_size_ = wid->wr().length();
        cmd_msg_buf_  = (struct cmd_msg *)(b->payload() + wid->wr().local_offset());
        free_in_dtor_ = false;
    } else {
        get_cmd_msg_buffer();
        free_in_dtor_ = true;
    }
    pack(id, wid);

    return;
}
ugni_cmd_msg::ugni_cmd_msg(
    nnti::transports::ugni_transport *transport,
    char                             *buf,
    uint32_t                          buf_size,
    bool                              copy_buf)
: transport_(transport),
  initiator_peer_(nullptr),
  target_peer_(nullptr),
  cmd_msg_buf_(nullptr),
  cmd_msg_size_(buf_size),
  free_in_dtor_(copy_buf),
  unexpected_(false)
{
    get_cmd_msg_buffer();
    if (copy_buf) {
        cmd_msg_buf_ = internal_cmd_msg_buf_;
        memcpy(cmd_msg_buf_, buf, cmd_msg_size_);
    } else {
        cmd_msg_buf_ = (struct cmd_msg*)buf;
    }

    return;
}
ugni_cmd_msg::ugni_cmd_msg(
    nnti::transports::ugni_transport *transport,
    char                             *buf,
    uint32_t                          buf_size)
: ugni_cmd_msg(transport, buf, buf_size, false)
{
    return;
}
ugni_cmd_msg::~ugni_cmd_msg()
{
    if (free_in_dtor_) free(internal_cmd_msg_buf_);
    return;
}

void
ugni_cmd_msg::set(
    uint32_t                      id,
    nnti::datatype::nnti_work_id *wid)
{
    pack(id, wid);
}

void
ugni_cmd_msg::set(
    char     *buf,
    uint32_t  buf_size,
    bool      copy_buf)
{
    cmd_msg_size_ = buf_size;
    if (copy_buf) {
        // if copying, then make sure we are using the internally allocated buffer
        log_debug("ugni_cmd_msg", "set - cmd_msg_buf_(%p)  internal_cmd_msg_buf_(%p)", cmd_msg_buf_, internal_cmd_msg_buf_);
        cmd_msg_buf_ = internal_cmd_msg_buf_;
        memcpy(cmd_msg_buf_, buf, cmd_msg_size_);
    } else {
        cmd_msg_buf_  = (struct cmd_msg*)buf;
    }

    return;
}

char *
ugni_cmd_msg::buf(void)
{
    return (char *)cmd_msg_buf_;
}
uint32_t
ugni_cmd_msg::size(void)
{
    return cmd_msg_size_;
}

void
ugni_cmd_msg::unpack(void)
{
    log_debug("ugni_cmd_msg", "unpack - enter");

    nnti::core::nnti_connection *conn = transport_->conn_map_.get(cmd_msg_buf_->initiator);
    initiator_peer_ = (nnti::datatype::ugni_peer*)conn->peer();

    if (*(uint32_t*)cmd_msg_buf_->packed_initiator_hdl != 0) {
        initiator_hdl_ = (nnti::datatype::ugni_buffer*)transport_->unpack_buffer(cmd_msg_buf_->packed_initiator_hdl,
                                  packed_buffer_size_);
        initiator_hdl_valid_ = true;
    } else {
        initiator_hdl_valid_ = false;
    }

    log_debug("ugni_cmd_msg", "unpacking message id(%u) from %s with target_base_addr(%lu)",
        cmd_msg_buf_->id, initiator_peer()->url().url().c_str(), cmd_msg_buf_->target_base_addr);

    if (cmd_msg_buf_->target_base_addr != 0) {
        target_hdl_ = (nnti::datatype::ugni_buffer*)transport_->buffer_map_.get((char*)cmd_msg_buf_->target_base_addr);
        target_hdl_valid_ = true;
        unexpected_ = false;
    } else {
        target_hdl_valid_ = false;
        unexpected_ = true;
    }

    log_debug("ugni_cmd_msg", "unpack - exit");

    return;
}

uint64_t
ugni_cmd_msg::header_length(void)
{
    return offsetof(struct cmd_msg, eager_payload);
}

bool
ugni_cmd_msg::unexpected(void)
{
    return unexpected_;
}

uint64_t
ugni_cmd_msg::initiator_offset(void)
{
    return cmd_msg_buf_->initiator_offset;
}
uint64_t
ugni_cmd_msg::target_offset(void)
{
    return cmd_msg_buf_->target_offset;
}

nnti::datatype::ugni_peer*
ugni_cmd_msg::initiator_peer(void)
{
    return initiator_peer_;
}
nnti::datatype::ugni_buffer*
ugni_cmd_msg::initiator_buffer(void)
{
    return initiator_hdl_;
}
nnti::datatype::ugni_peer*
ugni_cmd_msg::target_peer(void)
{
    return target_peer_;
}
nnti::datatype::ugni_buffer*
ugni_cmd_msg::target_buffer(void)
{
    return target_hdl_;
}
bool
ugni_cmd_msg::eager(void)
{
    return (cmd_msg_buf_->payload_length <= (size()-header_length()));
}
char *
ugni_cmd_msg::eager_payload(void)
{
    return cmd_msg_buf_->eager_payload;
}
uint64_t
ugni_cmd_msg::payload_length(void)
{
    return cmd_msg_buf_->payload_length;
}

void
ugni_cmd_msg::src_op_id(uint32_t soi)
{
    cmd_msg_buf_->src_op_id = soi;
}
uint32_t
ugni_cmd_msg::src_op_id(void)
{
    return cmd_msg_buf_->src_op_id;
}

uint32_t
ugni_cmd_msg::id(void)
{
    return cmd_msg_buf_->id;
}

void
ugni_cmd_msg::post_desc(gni_post_descriptor_t *post_desc)
{
    post_desc_ = *post_desc;
}
gni_post_descriptor_t *
ugni_cmd_msg::post_desc(void)
{
    return &post_desc_;
}

std::string
ugni_cmd_msg::toString(void)
{
    std::stringstream out;
    out << "  ugni_cmd_msg = "                     << (void*)this
        << " | ugni_cmd_msg.buf() = "              << buf()
        << " | ugni_cmd_msg.id() = "               << id()
        << " | ugni_cmd_msg.size() = "             << size()
        << " | ugni_cmd_msg.src_op_id() = "        << src_op_id()
        << " | ugni_cmd_msg.header_length() = "    << header_length()
        << " | ugni_cmd_msg.unexpected() = "       << unexpected()
        << " | ugni_cmd_msg.initiator_offset() = " << initiator_offset()
        << " | ugni_cmd_msg.target_offset() = "    << target_offset()
        << " | ugni_cmd_msg.initiator_peer() = "   << initiator_peer()
        << " | ugni_cmd_msg.initiator_buffer() = " << initiator_buffer()
        << " | ugni_cmd_msg.target_buffer() = "    << target_offset()
        << " | ugni_cmd_msg.eager() = "            << eager()
        << " | ugni_cmd_msg.eager_payload() = "    << eager_payload()
        << " | ugni_cmd_msg.payload_length() = "   << payload_length();
    return out.str();
}

void
ugni_cmd_msg::get_cmd_msg_buffer(void)
{
    internal_cmd_msg_buf_ = cmd_msg_buf_ = (struct cmd_msg *)malloc(sizeof(uint8_t) * cmd_msg_size_);

    return;
}

void
ugni_cmd_msg::pack(
    uint32_t                      id,
    nnti::datatype::nnti_work_id *wid)
{
    const nnti::datatype::nnti_work_request &wr = wid->wr();
    nnti::datatype::ugni_buffer *buf;

    log_debug("ugni_cmd_msg", "pack - enter");

    memset(cmd_msg_buf_, 0, header_length());

    cmd_msg_buf_->id               = id;
    cmd_msg_buf_->initiator        = transport_->me_.pid();
    cmd_msg_buf_->initiator_offset = wr.local_offset();
    cmd_msg_buf_->target_offset    = wr.remote_offset();
    cmd_msg_buf_->payload_length   = wr.length();
    if (wid->wr().flags() & NNTI_OF_ZERO_COPY) {
        cmd_msg_buf_->payload_length -= header_length();
    }

    if (wr.local_hdl() != NNTI_INVALID_HANDLE) {
        buf = (nnti::datatype::ugni_buffer *)wr.local_hdl();
        buf->pack(cmd_msg_buf_->packed_initiator_hdl, packed_buffer_size_);

        if (!(wid->wr().flags() & NNTI_OF_ZERO_COPY) && eager()) {
            // message is small, use eager
            log_debug("ugni_cmd_msg", "payload=%p  offset=%lu  length=%lu",
                      buf->payload(), cmd_msg_buf_->initiator_offset, cmd_msg_buf_->payload_length);
            memcpy(cmd_msg_buf_->eager_payload, buf->payload()+cmd_msg_buf_->initiator_offset, cmd_msg_buf_->payload_length);
        }
    } else {
        *(uint32_t*)cmd_msg_buf_->packed_initiator_hdl = 0;
    }

    if (wr.remote_hdl() != NNTI_INVALID_HANDLE) {
        buf = (nnti::datatype::ugni_buffer *)wr.remote_hdl();
        cmd_msg_buf_->target_base_addr = (uint64_t)buf->payload();
        unexpected_ = false;
    } else {
        cmd_msg_buf_->target_base_addr = 0;
        unexpected_ = true;
    }

    target_peer_ = (nnti::datatype::ugni_peer *)wid->wr().peer();
    log_debug("ugni_cmd_msg", "packing message id(%u) from %s to %s with target_base_addr(%lu)",
        cmd_msg_buf_->id, transport_->me_.url().url().c_str(), target_peer_->url().url().c_str(), cmd_msg_buf_->target_base_addr);

    log_debug("ugni_cmd_msg", "pack - exit");

    return;
}

} /* namespace core */
} /* namespace nnti */
