// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_CMD_TGT_HPP_
#define UGNI_CMD_TGT_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "faodel-common/MutexWrapper.hh"

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_state_machine.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/transports/ugni/ugni_connection.hpp"


namespace nnti {
namespace core {

class ugni_cmd_tgt
: public state_machine {
private:
    nnti::transports::ugni_transport *transport_;
    nnti::core::ugni_cmd_msg          cmd_msg_;

    gni_post_descriptor_t post_desc_;

    NNTI_event_t *event_;

    NNTI_buffer_t unexpected_dst_hdl_;
    uint64_t      unexpected_dst_offset_;

    uint64_t actual_offset_;

public:
    uint32_t index;

public:
    ugni_cmd_tgt(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size)
    : transport_(transport),
      cmd_msg_(transport, cmd_msg_size),
      state_(msg_state::INIT)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");
        return;
    }
    ugni_cmd_tgt(
        nnti::transports::ugni_transport *transport,
        char                             *buf,
        uint32_t                          buf_size,
        bool                              copy_buf)
    : transport_(transport),
      cmd_msg_(transport, buf, buf_size, copy_buf),
      event_(nullptr),
      state_(msg_state::INIT),
      active_entries_(0)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");
        return;
    }
    ugni_cmd_tgt(
        nnti::transports::ugni_transport *transport,
        char                             *buf,
        uint32_t                          buf_size)
    : ugni_cmd_tgt(transport, buf, buf_size, false)
    {
        return;
    }
    virtual ~ugni_cmd_tgt()
    {
        delete sm_lock_;
    }

    void
    set(
        char     *buf,
        uint32_t  buf_size,
        bool      copy_buf)
    {
        state_ = msg_state::INIT;
        cmd_msg_.set(buf, buf_size, copy_buf);
    }

    void
    unpack(void)
    {
        cmd_msg_.unpack();
    }

    bool
    eager(void)
    {
        return cmd_msg_.eager();
    }

    bool
    unexpected(void)
    {
        return cmd_msg_.unexpected();
    }

    uint64_t
    initiator_offset(void)
    {
        return cmd_msg_.initiator_offset();
    }
    uint64_t
    target_offset(void)
    {
        return cmd_msg_.target_offset();
    }
    nnti::datatype::ugni_peer*
    initiator_peer(void)
    {
        return cmd_msg_.initiator_peer();
    }
    nnti::datatype::ugni_buffer*
    initiator_buffer(void)
    {
        return cmd_msg_.initiator_buffer();
    }
    nnti::datatype::ugni_buffer*
    target_buffer(void)
    {
        return cmd_msg_.target_buffer();
    }

    char *
    eager_payload(void)
    {
        return cmd_msg_.eager_payload();
    }
    uint64_t
    payload_length(void)
    {
        return cmd_msg_.payload_length();
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

    uint32_t
    id(void)
    {
        return cmd_msg_.id();
    }

    void
    post_desc(gni_post_descriptor_t *post_desc)
    {
        post_desc_ = *post_desc;
    }
    gni_post_descriptor_t *
    post_desc(void)
    {
        return &post_desc_;
    }

    void
    unexpected_dst_hdl(NNTI_buffer_t hdl)
    {
        unexpected_dst_hdl_ = hdl;
    }
    void
    unexpected_dst_offset(uint64_t offset)
    {
        unexpected_dst_offset_ = offset;
    }
    uint64_t
    actual_offset(void)
    {
        return actual_offset_;
    }

    char *
    cmd_buf(void)
    {
        return cmd_msg_.buf();
    }

    uint32_t
    cmd_size(void)
    {
        return cmd_msg_.size();
    }

    virtual std::string
    toString(void)
    {
        return cmd_msg_.toString();
    }


    /*
     * Implement the state_machine interface
     */
private:

    enum msg_state : int {
        INIT = 0,
        UNPACK,

        PUSH_UNEXPECTED_MSG,
        CREATE_UNEXPECTED_EVENT,
        INVOKE_UNEXPECTED_QUEUE_CALLBACK,
        INVOKING_UNEXPECTED_QUEUE_CALLBACK,
        PUSH_UNEXPECTED_EVENT,
        ISSUE_UNEXPECTED_EVENT,
        NEED_UNEXPECTED_RETRIEVAL,
        WAIT_UNEXPECTED_RETRIEVAL,
        UNEXPECTED_COPY_IN,
        UNEXPECTED_LONG_GET,
        UNEXPECTED_LONG_GET_COMPLETE,

        EXPECTED,

        EAGER,
        EAGER_COPY_IN,
        ISSUE_EAGER_EVENT,

        LONG,
        LONG_GET,
        WAIT_LONG_GET,
        LONG_GET_COMPLETE,
        ISSUE_LONG_EVENT,

        SEND_LONG_GET_ACK,
        SEND_LONG_GET_ACK_COMPLETE,

        CLEANUP,
        DONE
    };

    msg_state              state_;
    faodel::MutexWrapper  *sm_lock_;
    std::atomic<uint64_t>  active_entries_;


    NNTI_event_t *
    create_event(
        nnti::core::ugni_cmd_tgt *cmd_tgt,
        uint64_t                  offset)
    {
        NNTI_event_t *e = nullptr;
        log_debug("ugni_cmd_tgt", "create_event(cmd_tgt, offset) - enter");
        if (transport_->event_freelist_->pop(e) == false) {
            e = new NNTI_event_t;
        }

        e->trans_hdl  = nnti::transports::transport::to_hdl(transport_);
        e->result     = NNTI_OK;
        e->op         = NNTI_OP_SEND;
        e->peer       = nnti::datatype::nnti_peer::to_hdl(cmd_tgt->initiator_peer());
        log_debug("ugni_cmd_tgt", "e->peer = %p", e->peer);
        e->length     = cmd_tgt->payload_length();
        if (cmd_tgt->unexpected()) {
            log_debug("ugni_cmd_tgt", "creating unexpected event");
            e->type       = NNTI_EVENT_UNEXPECTED;
            e->start      = nullptr;
            e->offset     = 0;
            e->context    = 0;
        } else {
            log_debug("ugni_cmd_tgt", "creating eager event");
            e->type       = NNTI_EVENT_RECV;
            e->start      = cmd_tgt->target_buffer()->payload();
            e->offset     = offset;
            e->context    = 0;
        }
        log_debug("ugni_cmd_tgt", "create_event(cmd_tgt, offset) - exit");
        return(e);
    }

    NNTI_event_t *
    create_event(
        nnti::core::ugni_cmd_tgt *cmd_tgt)
    {
        NNTI_event_t *e = nullptr;
        log_debug("ugni_cmd_tgt", "create_event(cmd_tgt) - enter");
        e = create_event(cmd_tgt, cmd_tgt->target_offset());
        log_debug("ugni_cmd_tgt", "create_event(cmd_tgt) - exit");
        return(e);
    }

    msg_state
    unpack_msg(void)
    {
        this->unpack();
        log_debug_stream("ugni_cmd_tgt") << this->toString();

        return msg_state::PUSH_UNEXPECTED_MSG;
    }
    msg_state
    push_unexpected_msg(void)
    {
        if (transport_->unexpected_queue_ == nullptr) {
            // If there is no unexpected queue, then there is no way
            // to communicate unexpected messages to the app.
            // Drop this message.
            NNTI_FAST_STAT(transport_->stats_->dropped_unexpected++;);
            return msg_state::CLEANUP;
        }

        transport_->unexpected_msgs_.push_back(this);
        NNTI_FAST_STAT(transport_->stats_->unexpected_recvs++;);
        return msg_state::CREATE_UNEXPECTED_EVENT;
    }
    msg_state
    create_unexpected_event(void)
    {
        event_ = create_event(this);
        return msg_state::INVOKE_UNEXPECTED_QUEUE_CALLBACK;
    }
    msg_state
    invoke_unexpected_queue_callback(void)
    {
        NNTI_result_t rc;

        /*
         * It is legal for the unexpected queue callback to invoke next_unexpected()
         * which could re-entry to the state machine.  The mutex must be unlocked
         * at re-entry.  It will be locked again when the callback returns.
         */
        sm_lock_->Unlock();
        rc = transport_->unexpected_queue_->invoke_cb(event_);
        sm_lock_->Lock();
        if (rc != NNTI_OK) {
            return msg_state::PUSH_UNEXPECTED_EVENT;
        } else if (state_ > msg_state::NEED_UNEXPECTED_RETRIEVAL) {
            return state_;
        } else {
            return msg_state::NEED_UNEXPECTED_RETRIEVAL;
        }
    }
    msg_state
    push_unexpected_event(void)
    {
        transport_->unexpected_queue_->push(event_);
        transport_->unexpected_queue_->notify();
        return msg_state::NEED_UNEXPECTED_RETRIEVAL;
    }
    msg_state
    unexpected_copy_in(void)
    {
        NNTI_result_t rc;
        nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)unexpected_dst_hdl_;
        rc = b->copy_in(unexpected_dst_offset_, eager_payload(), payload_length(), &actual_offset_);
        if (rc != NNTI_OK) {
            log_error("next_unexpected", "copy_in() failed (rc=%d)", rc);
        }
        NNTI_FAST_STAT(transport_->stats_->short_recvs++;);
        return msg_state::CLEANUP;
    }
    msg_state
    unexpected_long_get()
    {
        const uint32_t MSG_ALIGNMENT = NNTI_UGNI_RDMA_ALIGNMENT;
        int gni_rc;
        gni_cq_entry_t ev_data;

        gni_post_descriptor_t       *post_desc_ptr = nullptr;
        nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)this->initiator_peer()->conn();

        nnti::datatype::ugni_buffer *init_buf = this->initiator_buffer();
        nnti::datatype::ugni_buffer *dst_buf  = (nnti::datatype::ugni_buffer *)nnti::datatype::nnti_buffer::to_obj(unexpected_dst_hdl_);

        gni_post_descriptor_t post_desc;
        NNTI_ugni_mem_hdl_p_t mem_hdl;
        
        uint32_t addr_alignment = (MSG_ALIGNMENT - (((uint64_t)init_buf->payload() + this->initiator_offset()) % MSG_ALIGNMENT)) % MSG_ALIGNMENT;
        if (addr_alignment != 0) {
            log_debug("ugni_cmd_tgt", 
                      "long send address is not 4-byte aligned (%p %% %u = %u); copying first %u bytes from eager payload.",
                      ((uint64_t)init_buf->payload() + this->initiator_offset()), 
                      MSG_ALIGNMENT,
                      ((uint64_t)init_buf->payload() + this->initiator_offset())%MSG_ALIGNMENT, 
                      addr_alignment);

            memcpy(dst_buf->payload() + unexpected_dst_offset_,
                   this->eager_payload(),
                   addr_alignment);
        }

        uint32_t extra = (this->payload_length()-addr_alignment) % MSG_ALIGNMENT;
        if (extra > 0) {
            // message length isn't 4-byte alignment, get trailing bytes from eager
            log_debug("ugni_cmd_tgt", 
                      "long send length is not 4-byte aligned (%lu %% %u = %u); copying last %u bytes from eager payload.",
                      this->payload_length()-addr_alignment, 
                      MSG_ALIGNMENT,
                      (this->payload_length()-addr_alignment)%MSG_ALIGNMENT,
                      extra);

            memcpy(dst_buf->payload() + unexpected_dst_offset_ + this->payload_length() - extra,
                   this->eager_payload() + addr_alignment,
                   extra);
        }
        
        uint64_t aligned_local_addr  = (uint64_t)dst_buf->payload() + unexpected_dst_offset_ + addr_alignment;
        uint64_t aligned_remote_addr = (uint64_t)init_buf->payload() + this->initiator_offset() + addr_alignment;
        uint64_t aligned_length      = this->payload_length() - addr_alignment - extra;
        
        log_debug("ugni_cmd_tgt", 
                  "\nlong get RDMA summary:\n"
                  "\taligned_local_addr  = %p (aligned? %c)\n"
                  "\taligned_remote_addr = %p (aligned? %c)\n"
                  "\taligned_length      = %lu (aligned? %c)\n",
                  aligned_local_addr,  (!(aligned_local_addr%MSG_ALIGNMENT))?'Y':'N', 
                  aligned_remote_addr, (!(aligned_remote_addr%MSG_ALIGNMENT))?'Y':'N',
                  aligned_length,      (!(aligned_length%MSG_ALIGNMENT))?'Y':'N');

        memset(&post_desc, 0, sizeof(gni_post_descriptor_t));

//        post_desc.src_cq_hndl = unexpected_cq_hdl_;

        mem_hdl = dst_buf->mem_hdl();
        post_desc.local_addr            = aligned_local_addr;
        post_desc.local_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc.local_mem_hndl.qword2 = mem_hdl.qword2;

        mem_hdl = init_buf->mem_hdl();
        post_desc.remote_addr            = aligned_remote_addr;
        post_desc.remote_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc.remote_mem_hndl.qword2 = mem_hdl.qword2;

        post_desc.length = aligned_length;
        post_desc.type   = GNI_POST_RDMA_GET;

        post_desc.cq_mode   = GNI_CQMODE_GLOBAL_EVENT | GNI_CQMODE_REMOTE_EVENT;
        post_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;

        this->post_desc(&post_desc);

        this->index = transport_->msg_vector_.add(this);

        log_debug("ugni_cmd_tgt", "calling PostRdma(rdma get ; ep_hdl(%llu) transport_.unexpected_long_get_ep_cq_hdl_(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
                conn->unexpected_ep_hdl(), transport_->unexpected_long_get_ep_cq_hdl_,
                post_desc.local_mem_hndl.qword1, post_desc.local_mem_hndl.qword2,
                post_desc.remote_mem_hndl.qword1, post_desc.remote_mem_hndl.qword2);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc=GNI_EpSetEventData(
                conn->unexpected_ep_hdl(),
                this->index,
                this->src_op_id());
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_tgt", "EpSetEventData(rdma_ep_hdl_) failed: %d", gni_rc);
        gni_rc=GNI_PostRdma(
                conn->unexpected_ep_hdl(),
                &post_desc);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_cmd_tgt", "failed to post BTE (gni_rc=%d): %s", gni_rc, strerror(errno));
        }
        log_debug("ugni_cmd_tgt", "called PostRdma(rdma get)");

        nthread_lock(&transport_->ugni_lock_);
        log_debug("ugni_cmd_tgt", "calling CqWaitEvent(unexpected_cq_hdl)");
        gni_rc=GNI_CqWaitEvent(transport_->unexpected_long_get_ep_cq_hdl_, -1, &ev_data);
        log_debug("ugni_cmd_tgt", "called CqWaitEvent(unexpected_cq_hdl)");
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc!=GNI_RC_SUCCESS) {
            log_error("ugni_cmd_tgt", "CqWaitEvent(unexpected_cq_hdl) failed: %d", gni_rc);
        } else {
            log_debug("ugni_cmd_tgt", "CqWaitEvent(unexpected_cq_hdl) success");
        }
        
        log_debug("ugni_cmd_tgt", "got event for cmd_tgt with index %d ; my index is %d", GNI_CQ_GET_INST_ID(ev_data), this->index);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc=GNI_GetCompleted(transport_->unexpected_long_get_ep_cq_hdl_, ev_data, &post_desc_ptr);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc!=GNI_RC_SUCCESS) {
            log_error("ugni_cmd_tgt", "GetCompleted(next_unexpected(%p)) failed: %d", post_desc_ptr, gni_rc);
        } else {
            log_debug("ugni_cmd_tgt", "GetCompleted(next_unexpected(%p)) success", post_desc_ptr);
        }
//        print_post_desc(post_desc_ptr);

        transport_->msg_vector_.remove(this->index);

        NNTI_FAST_STAT(transport_->stats_->long_recvs++;);

        return msg_state::UNEXPECTED_LONG_GET_COMPLETE;
    }
    msg_state
    eager_copy_in(void)
    {
        NNTI_result_t nnti_rc = NNTI_OK;

        nnti::datatype::nnti_buffer *b = this->target_buffer();
        assert(b != nullptr);

        nnti_rc = b->copy_in(this->target_offset(), this->eager_payload(), this->payload_length(), &actual_offset_);

        if (nnti_rc == NNTI_ENOMEM) {

        } else if (nnti_rc != NNTI_OK) {

        } else {

        }
        return msg_state::ISSUE_EAGER_EVENT;
    }
    msg_state
    issue_eager_event()
    {
        nnti::datatype::nnti_buffer      *b             = this->target_buffer();
        assert(b != nullptr);
        nnti::datatype::nnti_event_queue *q             = nnti::datatype::nnti_event_queue::to_obj(b->eq());
        NNTI_event_t                     *e             = nullptr;
        bool                              release_event = true;

        e  = create_event(this, actual_offset_);
        if (b->invoke_cb(e) != NNTI_OK) {
            if (q && q->invoke_cb(e) != NNTI_OK) {
                q->push(e);
                q->notify();
                release_event = false;
            }
        }
        if (release_event) {
            // we're done with the event
            transport_->event_freelist_->push(e);
        }
        NNTI_FAST_STAT(transport_->stats_->short_recvs++;);
        return msg_state::CLEANUP;
    }
    msg_state
    long_get()
    {
        int gni_rc;

        nnti::datatype::ugni_buffer *init_buf = this->initiator_buffer();
        nnti::datatype::ugni_buffer *tgt_buf  = this->target_buffer();

        nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)this->initiator_peer()->conn();

        gni_post_descriptor_t post_desc;
        NNTI_ugni_mem_hdl_p_t mem_hdl;

        memset(&post_desc, 0, sizeof(gni_post_descriptor_t));

//                                        post_desc.src_cq_hndl=rdma_ep_cq_hdl_;

        mem_hdl = tgt_buf->mem_hdl();
        post_desc.local_addr            = (uint64_t)tgt_buf->payload() + this->target_offset();
        post_desc.local_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc.local_mem_hndl.qword2 = mem_hdl.qword2;

        mem_hdl = init_buf->mem_hdl();
        post_desc.remote_addr            = (uint64_t)init_buf->payload() + this->initiator_offset();
        post_desc.remote_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc.remote_mem_hndl.qword2 = mem_hdl.qword2;

        post_desc.length = this->payload_length();
        post_desc.type   = GNI_POST_RDMA_GET;

        post_desc.cq_mode   = GNI_CQMODE_GLOBAL_EVENT | GNI_CQMODE_REMOTE_EVENT;
        post_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;

        this->post_desc(&post_desc);

        this->index = transport_->msg_vector_.add(this);

        log_debug("ugni_cmd_tgt", "calling PostRdma(rdma get ; ep_hdl(%llu) transport_global_data.ep_cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
                conn->long_get_ep_hdl(), transport_->long_get_ep_cq_hdl_,
                post_desc.local_mem_hndl.qword1, post_desc.local_mem_hndl.qword2,
                post_desc.remote_mem_hndl.qword1, post_desc.remote_mem_hndl.qword2);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc=GNI_EpSetEventData(
                conn->long_get_ep_hdl(),
                this->index,
                this->src_op_id());
        if (gni_rc != GNI_RC_SUCCESS) log_error("ugni_cmd_tgt", "EpSetEventData(long_get_ep_hdl_) failed: %d", gni_rc);
        gni_rc=GNI_PostRdma(
                conn->long_get_ep_hdl(),
                this->post_desc());
        if (gni_rc!=GNI_RC_SUCCESS) log_error("ugni_cmd_tgt", "failed to post BTE (rc=%d): %s", gni_rc, strerror(errno));
        nthread_unlock(&transport_->ugni_lock_);

        NNTI_FAST_STAT(transport_->stats_->long_recvs++;);
        return msg_state::WAIT_LONG_GET;
    }
    msg_state
    send_long_get_ack()
    {
        gni_return_t  gni_rc  = GNI_RC_SUCCESS;

        log_debug("ugni_cmd_tgt", "enter");

        nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)this->initiator_peer()->conn();

        nnti::core::ugni_cmd_msg ack_cmd_msg(transport_, 2048);

        ack_cmd_msg.src_op_id(cmd_msg_.src_op_id());

        log_debug_stream("ugni_cmd_tgt") << "posting cmd_tgt " << this->toString();
        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_SmsgSendWTag(
                conn->mbox_ep_hdl(),
                ack_cmd_msg.buf(),
                ack_cmd_msg.size(),
                NULL,
                0,
                0xFFFFFF,
                NNTI_SMSG_TAG_LONG_GET_ACK);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_cmd_tgt", "SmsgSend(send_mbox.ep_hdl) failed: %d", gni_rc);
            abort();
        }

        log_debug("ugni_cmd_tgt", "exit");

        return msg_state::SEND_LONG_GET_ACK_COMPLETE;
    }
    msg_state
    issue_long_event()
    {
        nnti::datatype::nnti_buffer      *b             = this->target_buffer();
        assert(b != nullptr);
        nnti::datatype::nnti_event_queue *q             = nnti::datatype::nnti_event_queue::to_obj(b->eq());
        NNTI_event_t                     *e             = nullptr;
        bool                              release_event = true;

        e  = create_event(this);
        if (b->invoke_cb(e) != NNTI_OK) {
            if (q && q->invoke_cb(e) != NNTI_OK) {
                q->push(e);
                q->notify();
                release_event = false;
            }
        }
        if (release_event) {
            // we're done with the event
            transport_->event_freelist_->push(e);
        }
        return msg_state::CLEANUP;
    }

    void
    event_cleanup(void)
    {
        event_ = nullptr;
    }

    inline void state_update(msg_state new_state) {
        state_ = new_state;
    }
public:
    int
    update(NNTI_event_t *event)
    {
        int done = 0;

        active_entries_++;

        sm_lock_->Lock();
        while (1) {
            log_debug("ugni_cmd_tgt", "(this=%p) current state_ is %d", this, state_);
            switch (state_) {
                case msg_state::INIT:
                    state_update(msg_state::UNPACK);
                    break;
                case msg_state::UNPACK:
                    state_update(unpack_msg());
                    if (this->unexpected()) {
                        state_update(msg_state::PUSH_UNEXPECTED_MSG);
                    } else {
                        state_update(msg_state::EXPECTED);
                    }
                    break;
                case msg_state::PUSH_UNEXPECTED_MSG:
                    state_update(push_unexpected_msg());
                    break;
                case msg_state::CREATE_UNEXPECTED_EVENT:
                    state_update(create_unexpected_event());
                    break;
                case msg_state::INVOKE_UNEXPECTED_QUEUE_CALLBACK:
                    state_update(msg_state::INVOKING_UNEXPECTED_QUEUE_CALLBACK);
                    state_update(invoke_unexpected_queue_callback());
                    if (state_ > msg_state::NEED_UNEXPECTED_RETRIEVAL) {
                        // the unexpected queue callback already called next_unexpected() on this message, so we're done for now
                        goto exit;
                    }
                    break;
                case msg_state::INVOKING_UNEXPECTED_QUEUE_CALLBACK:
                    // We invoked the callback and we were reentered.
                    // Most likely the callback called t->next_unexpected().
                    state_update(msg_state::WAIT_UNEXPECTED_RETRIEVAL);
                    break;
                case msg_state::PUSH_UNEXPECTED_EVENT:
                    state_update(push_unexpected_event());
                    break;
                case msg_state::NEED_UNEXPECTED_RETRIEVAL:
                    state_update(msg_state::WAIT_UNEXPECTED_RETRIEVAL);
                    goto exit;
                    break;
                case msg_state::WAIT_UNEXPECTED_RETRIEVAL:
                    if (this->eager()) {
                        state_update(msg_state::UNEXPECTED_COPY_IN);
                    } else {
                        state_update(msg_state::UNEXPECTED_LONG_GET);
                    }
                    break;
                case msg_state::UNEXPECTED_COPY_IN:
                    state_update(unexpected_copy_in());
                    break;
                case msg_state::UNEXPECTED_LONG_GET:
                    state_update(unexpected_long_get());
                    break;
                case msg_state::UNEXPECTED_LONG_GET_COMPLETE:
                    state_update(msg_state::SEND_LONG_GET_ACK);
                    break;
                case msg_state::EXPECTED:
                    if (this->eager()) {
                        state_update(msg_state::EAGER);
                    } else {
                        state_update(msg_state::LONG);
                    }
                    break;
                case msg_state::EAGER:
                    state_update(msg_state::EAGER_COPY_IN);
                    break;
                case msg_state::EAGER_COPY_IN:
                    state_update(eager_copy_in());
                    break;
                case msg_state::ISSUE_EAGER_EVENT:
                    state_update(issue_eager_event());
                    break;
                case msg_state::LONG:
                    state_update(msg_state::LONG_GET);
                    break;
                case msg_state::LONG_GET:
                    state_update(long_get());
                    goto exit;
                    break;
                case msg_state::WAIT_LONG_GET:
                    state_update(msg_state::LONG_GET_COMPLETE);
                    break;
                case msg_state::LONG_GET_COMPLETE:
                    state_update(msg_state::SEND_LONG_GET_ACK);
                    break;
                case msg_state::SEND_LONG_GET_ACK:
                    state_update(send_long_get_ack());
                    break;
                case msg_state::SEND_LONG_GET_ACK_COMPLETE:
                    if (this->unexpected()) {
                        state_update(msg_state::CLEANUP);
                    } else {
                        state_update(msg_state::ISSUE_LONG_EVENT);
                    }
                    break;
                case msg_state::ISSUE_LONG_EVENT:
                    state_update(issue_long_event());
                    break;
                case msg_state::CLEANUP:
//                    update_stats();
                    event_cleanup();
                    state_update(msg_state::DONE);
                    break;
                case msg_state::DONE:
                    done = 1; // we're done
                    goto exit;
                    break;
                default:
                    log_error("ugni_cmd_tgt", "invalid state");
                    abort();
            }
        }

exit:
        sm_lock_->Unlock();
        active_entries_--;
        return done;
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_CMD_TGT_HPP_ */
