// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_RDMA_OP_HPP_
#define UGNI_RDMA_OP_HPP_

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
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/nnti_state_machine.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_wr.hpp"


namespace nnti {
namespace core {

class ugni_rdma_op
: public nnti_op, public state_machine {
private:
    nnti::transports::ugni_transport *transport_;

    gni_post_descriptor_t        post_desc_;
    nnti::datatype::ugni_buffer *local_buf_;
    nnti::datatype::ugni_buffer *remote_buf_;

public:
    ugni_rdma_op(
        nnti::transports::ugni_transport *transport)
    : nnti_op(),
      transport_(transport),
      state_(op_state::INIT),
      result_(NNTI_OK)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");

        return;
    }
    ugni_rdma_op(
        nnti::transports::ugni_transport *transport,
        nnti::datatype::nnti_work_id        *wid)
    : nnti_op(wid),
      transport_(transport),
      state_(op_state::INIT),
      result_(NNTI_OK)
    {
        sm_lock_ = faodel::GenerateMutex("pthreads");

        set(wid);

        return;
    }
    virtual ~ugni_rdma_op()
    {
        delete sm_lock_;
    }

    void
    set(nnti::datatype::nnti_work_id *wid)
    {
        id_     = next_id_.fetch_add(1);
        wid_    = wid;
        state_  = op_state::INIT;
        result_ = NNTI_OK;

        populate_post_desc(wid);

        return;
    }
    NNTI_result_t
    result()
    {
        return result_;
    }

    virtual std::string
    toString()
    {
        std::stringstream out;
        out << "id_==" << id_;
//        out << "id_=="         << id_
//            << " wr_id=="       << (void*)sq_wr_.wr_id
//            << " opcode=="      << sq_wr_.opcode
//            << " send_flags=="  << sq_wr_.send_flags
//            << " imm_data=="    << sq_wr_.imm_data
//            << " remote_addr==" << (void*)sq_wr_.wr.rdma.remote_addr
//            << " rkey=="        << std::hex << sq_wr_.wr.rdma.rkey << std::dec
//            << " addr=="        << (void*)sq_wr_.sg_list[0].addr
//            << " length=="      << sq_wr_.sg_list[0].length
//            << " lkey=="        << std::hex << sq_wr_.sg_list[0].lkey << std::dec;
        return out.str();
    }

private:
    void
    populate_post_desc(nnti::datatype::nnti_work_id *wid)
    {
        const nnti::datatype::ugni_work_request &wr = (const nnti::datatype::ugni_work_request &)wid->wr();
        NNTI_ugni_mem_hdl_p_t mem_hdl;

        local_buf_  = (nnti::datatype::ugni_buffer *)nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
        remote_buf_ = (nnti::datatype::ugni_buffer *)nnti::datatype::nnti_buffer::to_obj(wr.remote_hdl());

        memset(&post_desc_, 0, sizeof(gni_post_descriptor_t));

        mem_hdl = local_buf_->mem_hdl();
        post_desc_.local_addr            = (uint64_t)local_buf_->payload() + wr.local_offset();
        post_desc_.local_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc_.local_mem_hndl.qword2 = mem_hdl.qword2;

        mem_hdl = remote_buf_->mem_hdl();
        post_desc_.remote_addr            = (uint64_t)remote_buf_->payload() + wr.remote_offset();
        post_desc_.remote_mem_hndl.qword1 = mem_hdl.qword1;
        post_desc_.remote_mem_hndl.qword2 = mem_hdl.qword2;

        post_desc_.length = wr.length();
        if (wr.op() == NNTI_OP_PUT) {
            post_desc_.type   = GNI_POST_RDMA_PUT;
        }
        if (wr.op() == NNTI_OP_GET) {
            post_desc_.type   = GNI_POST_RDMA_GET;
        }

        post_desc_.cq_mode   = GNI_CQMODE_GLOBAL_EVENT | GNI_CQMODE_REMOTE_EVENT;
        post_desc_.dlvr_mode = GNI_DLVMODE_PERFORMANCE;

        return;
    }


    /*
     * Implement the state_machine interface
     */
private:
    enum op_state : int {
        INIT = 0,

        WAIT_RDMA_COMPLETE,
        ISSUE_RDMA_EVENT,

        ERROR,

        CLEANUP,
        DONE
    };

    op_state              state_;
    faodel::MutexWrapper *sm_lock_;
    NNTI_result_t         result_;

    op_state
    execute_rdma(void)
    {
        int gni_rc = GNI_RC_SUCCESS;

        log_debug("ugni_rdma_op", "enter");

        log_debug("ugni_rdma_op", "looking up connection for peer pid=%016lX", wid_->wr().peer_pid());

        nnti::datatype::nnti_peer   *peer = (nnti::datatype::nnti_peer *)wid_->wr().peer();
        nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)peer->conn();

        log_debug("ugni_rdma_op", "calling PostRdma(ep_hdl(%llu) transport_global_data.ep_cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
                conn->unexpected_ep_hdl(), conn->unexpected_cq_hdl(),
                post_desc_.local_mem_hndl.qword1, post_desc_.local_mem_hndl.qword2,
                post_desc_.remote_mem_hndl.qword1, post_desc_.remote_mem_hndl.qword2);

        nthread_lock(&transport_->ugni_lock_);
        gni_rc=GNI_EpSetEventData(
                conn->rdma_ep_hdl(),
                this->index,
                0);
        if (gni_rc != GNI_RC_SUCCESS) {
            log_error("ugni_rdma_op", "EpSetEventData(rdma_ep_hdl_) failed: %d", gni_rc);
            goto error;
        }
        gni_rc=GNI_PostRdma(
                conn->rdma_ep_hdl(),
                &post_desc_);
        if (gni_rc != GNI_RC_SUCCESS) {
          // log an error message and set a generic error code; if possible, set a specific code below 
          log_error("ugni_rdma_op", "failed to post BTE (gni_rc=%d): %s", gni_rc, strerror(errno));
          result_ = NNTI_EIO;

          if (gni_rc == GNI_RC_ALIGNMENT_ERROR) {
              result_ = NNTI_EALIGN;
          } else if (gni_rc == GNI_RC_ERROR_NOMEM) {
              result_ = NNTI_ENOMEM;
          } else if (gni_rc == GNI_RC_INVALID_PARAM) {
              result_ = NNTI_EINVAL;
          } else if (gni_rc == GNI_RC_PERMISSION_ERROR) {
              result_ = NNTI_EPERM;
          }

          goto error;
        }
        nthread_unlock(&transport_->ugni_lock_);
        log_debug("ugni_rdma_op", "called PostRdma()");

        log_debug("ugni_rdma_op", "exit - success");
        return op_state::WAIT_RDMA_COMPLETE;

error:
        nthread_unlock(&transport_->ugni_lock_);
        log_debug("ugni_rdma_op", "exit - failure");
        return op_state::ERROR;
    }
    NNTI_event_t *
    create_event(void)
    {
        const nnti::datatype::nnti_work_request &wr = wid_->wr();
        NNTI_event_t                            *e  = nullptr;

        log_debug("ugni_rdma_op", "create_event(rdma_op) - enter");

        if (transport_->event_freelist_->pop(e) == false) {
            e = new NNTI_event_t;
        }

        e->trans_hdl  = nnti::transports::transport::to_hdl(transport_);
        e->result     = result_;
        e->op         = wr.op();
        e->peer       = wr.peer();
        e->length     = wr.length();

        if (wr.op() == NNTI_OP_PUT) {
            e->type = NNTI_EVENT_PUT;
        }
        if (wr.op() == NNTI_OP_GET) {
            e->type = NNTI_EVENT_GET;
        }
        e->start      = local_buf_->payload();
        e->offset     = wr.local_offset();
        e->context    = 0;

        log_debug("ugni_rdma_op", "create_event(rdma_op) - exit");

        return(e);
    }
    op_state
    issue_rdma_event(void)
    {
        nnti::datatype::nnti_work_request &wr             = wid_->wr();
        nnti::datatype::nnti_event_queue  *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
        nnti::datatype::nnti_event_queue  *buf_q          = nullptr;
        NNTI_event_t                      *e              = create_event();
        bool                               event_complete = false;
        bool                               release_event  = true;

        if (wr.invoke_cb(e) == NNTI_OK) {
            event_complete = true;
        }
        if (!event_complete && alt_q && alt_q->invoke_cb(e) == NNTI_OK) {
            event_complete = true;
        }
        if (!event_complete) {
            nnti::datatype::nnti_buffer *b = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());

            buf_q = nnti::datatype::nnti_event_queue::to_obj(b->eq());
            if (buf_q && buf_q->invoke_cb(e) == NNTI_OK) {
                event_complete = true;
            }
        }
        if (!event_complete && alt_q) {
            alt_q->push(e);
            alt_q->notify();
            event_complete = true;
            release_event = false;
        }
        if (!event_complete && buf_q) {
            buf_q->push(e);
            buf_q->notify();
            event_complete = true;
            release_event = false;
        }
        if (release_event) {
            transport_->event_freelist_->push(e);
        }

        return op_state::CLEANUP;
    }
    void
    update_stats(void)
    {
        nnti::datatype::nnti_work_request &wr = wid_->wr();

        if (wr.op() == NNTI_OP_PUT) {
            NNTI_FAST_STAT(transport_->stats_->puts++;)
        }
        if (wr.op() == NNTI_OP_GET) {
            NNTI_FAST_STAT(transport_->stats_->gets++;)
        }
    }

public:
    int
    update(NNTI_event_t *event)
    {
        int done = 0;

        sm_lock_->Lock();
        while (1) {
            log_debug("ugni_rdma_op", "current state_ is %d", state_);
            switch (state_) {
                case op_state::INIT:
                    state_ = execute_rdma();
                    goto exit;
                    break;
                case op_state::WAIT_RDMA_COMPLETE:
                    state_ = op_state::ISSUE_RDMA_EVENT;
                    break;
                case op_state::ISSUE_RDMA_EVENT:
                    state_ = issue_rdma_event();
                    break;
                case op_state::ERROR:
                    state_ = op_state::CLEANUP;
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
                    log_error("ugni_rdma_op", "invalid state");
                    abort();
            }
        }
exit:
        sm_lock_->Unlock();
        return done;
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_RDMA_OP_HPP_ */
