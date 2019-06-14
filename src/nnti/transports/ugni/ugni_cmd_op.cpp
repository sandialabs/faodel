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

#include "faodel-common/MutexWrapper.hh"

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_eq.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_state_machine.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/transports/ugni/ugni_connection.hpp"


namespace nnti {
namespace core {


    ugni_cmd_op::ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size)
    : nnti_op(),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size),
      state_(op_state::INIT)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");
        return;
    }
    ugni_cmd_op::ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size,
        nnti::datatype::nnti_work_id     *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, cmd_msg_size, id_, wid),
      state_(op_state::INIT)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");
        return;
    }
    ugni_cmd_op::ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        nnti::datatype::nnti_work_id     *wid)
    : nnti_op(wid),
      transport_(transport),
      cmd_msg_(transport, id_, wid),
      state_(op_state::INIT)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");
        return;
    }
    ugni_cmd_op::~ugni_cmd_op()
    {
        delete sm_lock_;
    }

    void
    ugni_cmd_op::set(nnti::datatype::nnti_work_id *wid)
    {
        id_    = next_id_.fetch_add(1);
        wid_   = wid;
        state_ = op_state::INIT;
        cmd_msg_.set(id_, wid);
        log_debug("ugni_cmd_op", "cmd_op(%p) id_(%u)", this, id_);
    }

    bool
    ugni_cmd_op::eager(void)
    {
        return cmd_msg_.eager();
    }

    char *
    ugni_cmd_op::cmd_buf(void)
    {
        return cmd_msg_.buf();
    }

    uint32_t
    ugni_cmd_op::cmd_size(void)
    {
        return cmd_msg_.size();
    }

    void
    ugni_cmd_op::src_op_id(uint32_t soi)
    {
        cmd_msg_.src_op_id(soi);
    }
    uint32_t
    ugni_cmd_op::src_op_id(void)
    {
        return cmd_msg_.src_op_id();
    }

    nnti::datatype::ugni_peer*
    ugni_cmd_op::target_peer(void)
    {
        return cmd_msg_.target_peer();
    }

    std::string
    ugni_cmd_op::toString(void)
    {
        return cmd_msg_.toString();
    }


    /*
     * Implement the state_machine interface
     */
    ugni_cmd_op::op_state
    ugni_cmd_op::execute_send(void)
    {
        int gni_rc = GNI_RC_SUCCESS;

        log_debug("ugni_cmd_op", "enter");

        log_debug("ugni_cmd_op", "looking up connection for peer pid=%016lX", wid_->wr().peer_pid());

        nnti::datatype::nnti_peer   *peer = (nnti::datatype::nnti_peer *)wid_->wr().peer();
        nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)peer->conn();

        log_debug_stream("ugni_cmd_op") << "posting cmd_op " << this->toString();
        nthread_lock(&transport_->ugni_lock_);
        gni_rc = GNI_SmsgSendWTag(
                conn->mbox_ep_hdl(),
                this->cmd_buf(),
                this->cmd_size(),
                NULL,
                0,
                this->index,
                NNTI_SMSG_TAG_REQUEST);
        nthread_unlock(&transport_->ugni_lock_);
        if (gni_rc == GNI_RC_NOT_DONE) {
            log_debug("ugni_cmd_op", "SmsgSend(send_mbox.ep_hdl) says no credits available: %d", gni_rc);
            return op_state::NEED_SEND_CREDITS;
        } else if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_cmd_op", "SmsgSend(send_mbox.ep_hdl) failed: %d", gni_rc);
            abort();
        }

        log_debug("ugni_cmd_op", "exit");

        return op_state::NEED_SEND_COMPLETE;
    }
    NNTI_event_t *
    ugni_cmd_op::create_event(void)
    {
        const nnti::datatype::nnti_work_request &wr = wid_->wr();
        NNTI_event_t                            *e  = nullptr;

        log_debug("ugni_cmd_op", "create_event(cmd_op) - enter");

        if (transport_->event_freelist_->pop(e) == false) {
            e = new NNTI_event_t;
        }

        e->trans_hdl  = nnti::transports::transport::to_hdl(transport_);
        e->result     = NNTI_OK;
        e->op         = wr.op();
        e->peer       = wr.peer();
        e->length     = wr.length();

        e->type       = NNTI_EVENT_SEND;
        e->start      = nullptr;
        e->offset     = 0;
        e->context    = 0;

        log_debug("ugni_cmd_op", "create_event(cmd_op) - exit");

        return(e);
    }
    ugni_cmd_op::op_state
    ugni_cmd_op::issue_send_event(void)
    {
        nnti::datatype::nnti_work_request &wr             = wid_->wr();
        nnti::datatype::nnti_event_queue  *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
        nnti::datatype::nnti_buffer       *buf            = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
        nnti::datatype::nnti_event_queue  *buf_q          = nnti::datatype::nnti_event_queue::to_obj(buf->eq());
        NNTI_event_t                      *e              = create_event();
        bool                               event_complete = false;
        bool                               release_event  = true;

        if (wr.invoke_cb(e) == NNTI_OK) {
            log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - wr.invoke_cb() NNTI_OK");
            event_complete = true;
        }
        if (!event_complete && alt_q && alt_q->invoke_cb(e) == NNTI_OK) {
            log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - alt_q.invoke_cb() NNTI_OK");
            event_complete = true;
        }
        if (!event_complete && buf_q && buf_q->invoke_cb(e) == NNTI_OK) {
            log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - buf_q.invoke_cb() NNTI_OK");
            event_complete = true;
        }
        log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - event_complete == %d  alt_q == %p", event_complete ? 1 : 0, alt_q);
        if (!event_complete && alt_q) {
            log_debug("ugni_cmd_op", "issue_send_event() - pushing on alt_q");
            alt_q->push(e);
            alt_q->notify();
            event_complete = true;
            release_event = false;
        }
        log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - event_complete == %d  buf_q == %p", event_complete ? 1 : 0, buf_q);
        if (!event_complete && buf_q) {
            log_debug("ugni_cmd_op", "issue_send_event() - pushing on buf_q");
            buf_q->push(e);
            buf_q->notify();
            event_complete = true;
            release_event = false;
        }
        if (release_event) {
            transport_->event_freelist_->push(e);
        }

        log_debug("ugni_cmd_op", "issue_send_event(cmd_op) - event_complete == %d", event_complete ? 1 : 0);

        return op_state::CLEANUP;
    }
    ugni_cmd_op::op_state
    ugni_cmd_op::update_stats(void)
    {
        nnti::datatype::nnti_work_request &wr = wid_->wr();

        if (this->eager()) {
            NNTI_FAST_STAT(transport_->stats_->short_sends++;)
        } else {
            NNTI_FAST_STAT(transport_->stats_->long_sends++;)
        }

        if (wr.remote_hdl() == NNTI_INVALID_HANDLE) {
            NNTI_FAST_STAT(transport_->stats_->unexpected_sends++;)
        }
    }

    int
    ugni_cmd_op::update(NNTI_event_t *event)
    {
        int done = 0;

        sm_lock_->Lock();
        while (1) {
            log_debug("ugni_cmd_op", "current state_ of %p is %d", this, state_);
            switch (state_) {
                case op_state::INIT:
                    state_ = op_state::EXECUTE_SEND;
                    break;
                case op_state::EXECUTE_SEND:
                    state_ = execute_send();
                    break;
                case op_state::NEED_SEND_CREDITS:
                    state_ = op_state::WAIT_SEND_CREDITS;
                    done = 2;
                    goto exit;
                    break;
                case op_state::WAIT_SEND_CREDITS:
                    state_ = op_state::EXECUTE_SEND;
                    break;
                case op_state::NEED_SEND_COMPLETE:
                    state_ = op_state::WAIT_SEND_COMPLETE;
                    goto exit;
                    break;
                case op_state::WAIT_SEND_COMPLETE:
                    if (!this->eager()) {
                        state_ = op_state::WAIT_RDMA_ACK;
                        goto exit;
                    } else {
                        state_ = op_state::ISSUE_SEND_EVENT;
                    }
                    break;
                case op_state::WAIT_RDMA_ACK:
                    state_ = op_state::ISSUE_SEND_EVENT;
                    break;
                case op_state::ISSUE_SEND_EVENT:
                    state_ = issue_send_event();
                    break;
                case op_state::CLEANUP:
                    update_stats();
                    state_ = op_state::DONE;
                    break;
                case op_state::DONE:
                    done = 1; // we're done
                    goto exit;
                    break;
                default:
                    log_error("ugni_cmd_op", "invalid state");
                    abort();
            }
        }
exit:
        sm_lock_->Unlock();
        return done;
    }

} /* namespace core */
} /* namespace nnti */
