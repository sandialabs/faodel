// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef NNTI_BUFFER_HPP_
#define NNTI_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <algorithm>
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
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"


namespace nnti  {
namespace datatype {

class default_buffer_callback {
public:
    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        // this default callback returns !NNTI_OK so the event will pushed into the EQ
        return NNTI_EIO;
    }
};

class nnti_buffer
: public nnti_datatype {
private:
    static std::atomic<uint32_t> next_id_;

protected:
    NNTI_buffer_p_t        packable_;
    const static uint64_t  max_packed_size_=256;
    char                   packed_[max_packed_size_];
    uint64_t               packed_size_;

    uint32_t               id_;
    bool                   free_in_dtor_;

    /** @brief the process in which this buffer resides */
    NNTI_peer_t            buffer_owner_;

    /** @brief permitted operations */
    NNTI_buffer_flags_t    flags_;

    /** @brief size of this buffer */
    uint64_t               payload_size_;

    /** @brief local address of the memory buffer */
    char                  *payload_;

    uint64_t  next_read_offset_;
    uint64_t  next_write_offset_;

    NNTI_event_queue_t     eq_;
    nnti_event_callback    cb_;
    void                  *cb_context_;

public:
    nnti_buffer()
    : nnti_datatype(NNTI_dt_buffer),
      free_in_dtor_(false),
      flags_(NNTI_BF_UNSET),
      payload_size_(0),
      payload_(nullptr),
      next_read_offset_(0),
      next_write_offset_(0),
      eq_(0),
      cb_(),
      cb_context_(nullptr)
    {
        id_ = next_id_.fetch_add(1);

        return;
    }
    nnti_buffer(
        nnti::transports::transport *transport)
    : nnti_datatype(transport,
                    NNTI_dt_buffer),
      free_in_dtor_(false),
      flags_(NNTI_BF_UNSET),
      payload_size_(0),
      payload_(nullptr),
      next_read_offset_(0),
      next_write_offset_(0),
      eq_(0),
      cb_(transport, default_buffer_callback()),
      cb_context_(nullptr)
    {
        id_ = next_id_.fetch_add(1);

        return;
    }
    nnti_buffer(
        nnti::transports::transport *transport,
        const uint64_t               size,
        const NNTI_buffer_flags_t    flags,
        NNTI_event_queue_t           eq,
        nnti_event_callback          cb,
        void                        *cb_context)
    : nnti_datatype(transport,
                    NNTI_dt_buffer),
      free_in_dtor_(true),
      flags_(flags),
      payload_size_(size),
      payload_(nullptr),
      next_read_offset_(0),
      next_write_offset_(0),
      eq_(eq),
      cb_(cb),
      cb_context_(cb_context)
    {
        posix_memalign((void**)&payload_, 64, payload_size_);

        id_ = next_id_.fetch_add(1);

        return;
    }
    nnti_buffer(
        nnti::transports::transport *transport,
        char                        *buffer,
        const uint64_t               size,
        const NNTI_buffer_flags_t    flags,
        NNTI_event_queue_t           eq,
        nnti_event_callback          cb,
        void                        *cb_context)
    : nnti_datatype(transport,
                    NNTI_dt_buffer),
      free_in_dtor_(false),
      flags_(flags),
      payload_size_(size),
      payload_(buffer),
      next_read_offset_(0),
      next_write_offset_(0),
      eq_(eq),
      cb_(cb),
      cb_context_(cb_context)
    {
        id_ = next_id_.fetch_add(1);

        return;
    }
    nnti_buffer(
        nnti::transports::transport *transport,
        char                        *packed_buf,
        const uint64_t               packed_len)
    : nnti_datatype(transport,
                    NNTI_dt_buffer),
      free_in_dtor_(false),
      flags_(NNTI_BF_UNSET),
      payload_size_(0),
      payload_(nullptr),
      next_read_offset_(0),
      next_write_offset_(0),
      eq_(0),
      cb_(transport, default_buffer_callback()),
      cb_context_(nullptr)
    {
        unpack(packed_buf, packed_len);

#if NNTI_USE_XDR == 1
        memcpy(&packed_[0], packed_buf, std::min(packed_len, packed_size_));
#endif

        log_debug("nnti_buffer", "flags_=0x%04X", flags_);

        return;
    }
    virtual ~nnti_buffer()
    {
        if (free_in_dtor_) free(payload_);
        return;
    }
    virtual uint32_t id(void)
    {
        return id_;
    }
    virtual char*
    payload(void)
    {
        return payload_;
    }
    virtual uint64_t
    size(void)
    {
        return payload_size_;
    }

    virtual NNTI_event_queue_t
    eq(void)
    {
        return eq_;
    }
    virtual nnti_event_callback
    callback(void)
    {
        return cb_;
    }
    virtual void *
    cb_context(void)
    {
        return cb_context_;
    }
    NNTI_result_t invoke_cb(NNTI_event_t *event) {
        log_debug("nnti_event_queue", "invoking the BUFFER callback");
        return cb_.invoke(event, cb_context_);
    }

    virtual bool
    queuing(void)
    {
        return (flags_ & NNTI_BF_QUEUING);
    }

    uint64_t
    packed_size(void)
    {
        if (packed_size_ == 0) {
            packed_size_ = nnti::serialize::packed_buffer_size(&packable_);
        }
        return packed_size_;
    }

    NNTI_result_t
    internal_pack(void)
    {
        packable_.flags = flags_;
        return nnti::serialize::pack_buffer(&packable_, &packed_[0], max_packed_size_, &packed_size_);
    }

    NNTI_result_t
    pack(
        char           *packed_buf,
        const uint64_t  packed_buflen)
    {
        memcpy(packed_buf, &packed_[0], (packed_size_ < packed_buflen) ? packed_size_ : packed_buflen);
        return NNTI_OK;
    }

    NNTI_result_t
    unpack(
        char           *packed_buf,
        const uint64_t  packed_buflen)
    {
        packed_size_ = packed_buflen;
        memcpy(&packed_[0], &packed_buf[0], packed_buflen);
        nnti::serialize::unpack_buffer(&packable_, &packed_buf[0], packed_buflen);
        flags_       = (NNTI_buffer_flags_t)packable_.flags;
        return NNTI_OK;
    }

    NNTI_result_t
    free_packable(void)
    {
        return nnti::serialize::free_buffer(&packable_);
    }

    NNTI_result_t
    copy_in(
        uint64_t  requested_offset,
        char     *buf,
        uint64_t  buf_size,
        uint64_t *actual_offset)
    {
        NNTI_result_t rc = NNTI_OK;

        *actual_offset = requested_offset;

        if (queuing()) {
            log_debug("nnti_buffer", "copy_in - queuing");
            uint64_t current_offset = next_write_offset_;
            if (next_write_offset_ < next_read_offset_) {
                // next_write_offset_ is behind next_read_offset_.
                // there are bytes available in the middle of the buffer.
                if ((next_read_offset_ - next_write_offset_) >= buf_size) {
                    next_write_offset_ += buf_size;
                } else {
                    // not enough space
                    rc = NNTI_ENOMEM;
                }
            } else {
                // next_write_offset_ is not behind next_read_offset_.
                // there may be enough space at the back or front of the buffer.
                if (payload_size_-next_write_offset_ >= buf_size) { // back of buffer
                    next_write_offset_ = (next_write_offset_ + buf_size) % payload_size_;
                } else if (next_read_offset_ > buf_size) { // front of buffer
                    current_offset     = 0;
                    next_write_offset_ = buf_size;
                } else {
                    log_debug("nnti_buffer", "copy_in - OVERFLOW");
                }
            }
            if (rc == NNTI_OK) {
                *actual_offset = current_offset;
                memcpy(payload()+current_offset, buf, buf_size);
            }
        } else {
            log_debug("nnti_buffer", "copy_in - non-queuing (requested_offset(%lu)  buf_size(%lu)  payload_size_(%lu)",
                      requested_offset, buf_size, payload_size_);
            if (requested_offset+buf_size <= payload_size_) {
                memcpy(payload()+requested_offset, buf, buf_size);
            } else {
                rc = NNTI_ENOMEM;
            }
        }

        log_debug("nnti_buffer", "copy_in - next_read_offet(%lu) next_write_offset(%lu) actual_offset(%lu)", next_read_offset_, next_write_offset_, *actual_offset);

        return rc;
    }

    NNTI_result_t
    event_complete(
        NNTI_event_t *event)
    {
        NNTI_result_t rc = NNTI_OK;

        if (queuing()) {
            /*
             * If events are not completed in the order generated,
             * next_read_offset_ could be advanced passed messages
             * that have not been processed.
             *
             * TODO: Out-of-order completions should be appended
             * to a list that is processed after each in-order
             * event.
             */
            if (event->offset != next_read_offset_) {
                log_error("nnti_buffer", "out of order completion (e->offset(%lu) next_read_offset_(%lu) - messages could be lost", event->offset, next_read_offset_);
            }
            next_read_offset_ = (event->offset + event->length) % payload_size_;
        }

        return rc;
    }

    static inline nnti_buffer*
    to_obj(
        const NNTI_buffer_t buffer_hdl)
    {
        return (nnti_buffer *)buffer_hdl;
    }

    static inline NNTI_buffer_t
    to_hdl(
        const nnti_buffer *buffer)
    {
        return (NNTI_buffer_t)buffer;
    }


    virtual std::string
    toString() const override {
        std::stringstream out;
        out << "nnti_buffer.to_hdl(this)="        << to_hdl(this)
            << " | nnti_buffer.id_="               << this->id_
            << " | nnti_buffer.buffer_owner_="     << this->buffer_owner_
            << " | nnti_buffer.flags_="            << this->flags_
            << " | nnti_buffer.payload_size_="     << this->payload_size_
            << " | nnti_buffer.payload_="          << (uint64_t)this->payload_
            << " | nnti_buffer.eq_="               << this->eq_
            << " | nnti_buffer.cb_="               << (void*)&this->cb_
            << " | nnti_buffer.cb_context_="       << (void*)this->cb_context_;

        return out.str();
    }

};

class nnti_buffer_map {
public:
    nnti_buffer_map()
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_buffer_map()
    {
        nthread_lock_fini(&lock_);
    }

    void
    insert(
        nnti_buffer *buf)
    {
        nthread_lock(&lock_);
        assert(id_map_.find(buf->id()) == id_map_.end());
        id_map_[buf->id()] = buf;
        assert(payload_map_.find(buf->payload()) == payload_map_.end());
        payload_map_[buf->payload()] = buf;
        nthread_unlock(&lock_);
        return;
    }

    nnti_buffer *
    get(
        uint32_t id)
    {
        nnti_buffer *buf=NULL;
        nthread_lock(&lock_);
        if (id_map_.find(id) != id_map_.end()) {
            buf = id_map_[id];
        }
        nthread_unlock(&lock_);
        return(buf);
    }
//    nnti_buffer *
//    get(
//        NNTI_buffer_t *target)
//    {
//        uint32_t key=nnti::util::hash6432shift(target->payload);
//        nnti_buffer *buf=NULL;
//        nthread_lock(&lock_);
//        if (map_.find(key) != map_.end()) {
//            buf = map_[key];
//        }
//        nthread_unlock(&lock_);
//        return(buf);
//    }
    nnti_buffer *
    get(
        char *target)
    {
        nnti_buffer *buf=NULL;
        nthread_lock(&lock_);
        if (payload_map_.find(target) != payload_map_.end()) {
            buf = payload_map_[target];
        }
        nthread_unlock(&lock_);
        return(buf);
    }

    nnti_buffer *
    remove(
        nnti_buffer *buf)
    {
        return(remove(buf->id()));
    }
//    nnti_buffer *
//    remove(
//        NNTI_buffer_t *packable)
//    {
//        return(remove(nnti::util::hash6432shift(packable->payload)));
//    }
    nnti_buffer *
    remove(
        uint32_t id)
    {
        nnti_buffer *buf=NULL;
        nthread_lock(&lock_);
        if (id_map_.find(id) != id_map_.end()) {
            buf = id_map_[id];
        }
        if (buf != NULL) {
            payload_map_.erase(buf->payload());
            id_map_.erase(id);
        }
        nthread_unlock(&lock_);
        return(buf);
    }

    bool
    empty()
    {
        nthread_lock(&lock_);
        bool rc=id_map_.empty();
        nthread_unlock(&lock_);
        return(rc);
    }

    virtual std::string
    toString()
    {
        std::stringstream out;

        for (std::map<uint32_t, nnti_buffer *>::iterator iter = id_map_.begin() ; iter != id_map_.end() ; iter++) {
            out << iter->second->toString() << std::endl;
        }

        return out.str();
    }

private:
    std::map<uint32_t, nnti_buffer *>           id_map_;
    std::map<char *, nnti_buffer *>             payload_map_;
    std::map<uint32_t, nnti_buffer *>::iterator map_iter_;

    nthread_lock_t lock_;
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_BUFFER_HPP_ */
