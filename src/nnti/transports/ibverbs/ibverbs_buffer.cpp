// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <deque>
#include <map>
#include <sstream>
#include <string>

#include <infiniband/verbs.h>

#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"

#include "nnti/nnti_callback.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_buffer.hpp"

namespace nnti  {
namespace datatype {

ibverbs_buffer::ibverbs_buffer()
    : nnti_buffer(),
      registered_(false)
{
    mr = nullptr;
    return;
}
//ibverbs_buffer::ibverbs_buffer(
//    nnti::transports::ibverbs_transport *transport,
//    NNTI_buffer_p_t                     *packable)
//: nnti_buffer(transport,
//              packable)
//{
//    return;
//}
ibverbs_buffer::ibverbs_buffer(
    nnti::datatype::ibverbs_buffer &b)
: nnti_buffer(b),
  registered_(false)
{
    mr = b.mr;
    memcpy(packed_, b.packed_, packed_size_);

    return;
}
ibverbs_buffer::ibverbs_buffer(
    nnti::transports::ibverbs_transport *transport,
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti_event_callback                  cb,
    void                                *cb_context)
: nnti_buffer(transport,
              size,
              flags,
              eq,
              cb,
              cb_context),
  registered_(false)
{
    register_buffer();
    internal_pack();

    return;
}
ibverbs_buffer::ibverbs_buffer(
    nnti::transports::ibverbs_transport *transport,
    char                                *buffer,
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti_event_callback                  cb,
    void                                *cb_context)
: nnti_buffer(transport,
              buffer,
              size,
              flags,
              eq,
              cb,
              cb_context),
  registered_(false)
{
    register_buffer();
    internal_pack();

    return;
}
ibverbs_buffer::ibverbs_buffer(
    nnti::transports::transport *transport,
    char                        *packed_buf,
    const uint64_t               packed_len)
: nnti_buffer(transport,
              packed_buf,
              packed_len),
  registered_(false)
{
    payload_      = (char *)packable_.buffer.NNTI_remote_addr_p_t_u.ib.buf;
    payload_size_ = packable_.buffer.NNTI_remote_addr_p_t_u.ib.size;

    log_debug("ibverbs_buffer", "ctor unpack - segments[0].buf(%016lX) segments[0].size(%u)",
        packable_.buffer.NNTI_remote_addr_p_t_u.ib.buf, packable_.buffer.NNTI_remote_addr_p_t_u.ib.size);

    return;
}

ibverbs_buffer::~ibverbs_buffer()
{
    if (registered_) {
        unregister_buffer();
    }

    return;
}

char*
ibverbs_buffer::payload(void)
{
    return payload_;
}

void *
ibverbs_buffer::addr(void)
{
    return (void *)packable_.buffer.NNTI_remote_addr_p_t_u.ib.buf;
}

size_t
ibverbs_buffer::length(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.ib.size;
}

uint32_t
ibverbs_buffer::lkey(void)
{
    return mr->lkey;
}

uint32_t
ibverbs_buffer::rkey(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.ib.key;
}


NNTI_result_t
ibverbs_buffer::register_buffer(void)
{
    NNTI_result_t         rc = NNTI_OK;
    enum ibv_access_flags access;

    transports::ibverbs_transport *ibv_transport = (transports::ibverbs_transport*)transport_;

    log_debug("ibverbs_buffer", "enter buffer(%p) len(%d)", payload_, payload_size_);

    memset(&packable_, 0, sizeof(NNTI_buffer_p_t));

    access = nnti_to_ib_flags(flags_);

    if ((ibv_transport->use_odp_) &&
        (!(flags_ & NNTI_BF_REMOTE_ATOMIC))) {
        mr = ibv_transport->odp_mr_;
    } else {
        log_debug("ibverbs_buffer", "registering ibverbs_buffer (payload_=%x)", payload_);
        mr = ibv_reg_mr(ibv_transport->pd_, payload_, payload_size_, access);
        if (!mr) {
            if (errno == EFAULT) {
                log_debug("ibverbs_buffer", "ibv_reg_mr failed with EFAULT.  trying to register with IBV_ACCESS_REMOTE_READ.");
                mr = ibv_reg_mr(ibv_transport->pd_, payload_, payload_size_, IBV_ACCESS_REMOTE_READ);
                if (!mr) {
                    log_error("ibverbs_buffer", "failed to register memory region with IBV_ACCESS_REMOTE_READ: %s", strerror(errno));
                    rc = NNTI_EPERM;
                    goto cleanup;
                }
            } else {
                log_error("ibverbs_buffer", "failed to register memory region: %s", strerror(errno));
                rc = NNTI_EPERM;
                goto cleanup;
            }
        }
    }

    packable_.buffer.transport_id                   = NNTI_TRANSPORT_IBVERBS;
    packable_.buffer.NNTI_remote_addr_p_t_u.ib.size = payload_size_;
    packable_.buffer.NNTI_remote_addr_p_t_u.ib.buf  = (uint64_t)payload_;
    packable_.buffer.NNTI_remote_addr_p_t_u.ib.key  = mr->rkey;

    registered_ = true;

    log_debug("ibverbs_buffer", "exit (payload_==%p, mr==%p, lkey %x, rkey %x)...", payload_, mr, mr->lkey, mr->rkey);

cleanup:
    return rc;
}

NNTI_result_t
ibverbs_buffer::unregister_buffer(void)
{
    transports::ibverbs_transport *ibv_transport = (transports::ibverbs_transport*)transport_;

    if (ibv_transport->use_odp_) {
        log_debug("ibverbs_buffer", "using ODP - unregister is a no-op");
    } else {
        log_debug("ibverbs_buffer", "deregistering ibverbs_buffer (payload_=%x)", mr->addr);
        int ibv_rc=ibv_dereg_mr(mr);
        if (ibv_rc != 0) {
            log_error("ibverbs_buffer", "deregistering the memory buffer failed");
            return NNTI_EINVAL;
        }
        registered_ = false;
    }
    mr = NULL;

    return NNTI_OK;
}

NNTI_result_t
ibverbs_buffer::post_receive(void)
{
    NNTI_result_t         rc = NNTI_OK;
    int                   ibv_rc;
    enum ibv_access_flags access;
    struct ibv_recv_wr    rq_wr, *bad_wr;
    struct ibv_sge        sge;

    transports::ibverbs_transport *ibv_transport = (transports::ibverbs_transport*)transport_;

    log_debug("ibverbs_buffer::post_receive", "enter");

    memset(&rq_wr, 0, sizeof(struct ibv_recv_wr));
    memset(&sge,   0, sizeof(struct ibv_sge));

    rq_wr.next    = NULL;
    rq_wr.wr_id   = (uint64_t)this;
    rq_wr.sg_list = &sge;
    rq_wr.num_sge = 1;

    sge.addr   = (uint64_t)payload_;
    sge.length = payload_size_;
    sge.lkey   = mr->lkey;

    ibv_rc=ibv_post_srq_recv(ibv_transport->rdma_srq_, &rq_wr, &bad_wr);
    if (ibv_rc) {
        log_error("ibverbs_buffer::post_receive", "failed to post SRQ recv (rc=%d)", ibv_rc);
        return (NNTI_result_t)errno;
    }
    log_debug("ibverbs_buffer::post_receive", "post_srq_recv(addr=%p, length=%d, lkey=%x)", sge.addr, sge.length, sge.lkey);

    log_debug("ibverbs_buffer::post_receive", "exit");

    return rc;
}

enum ibv_access_flags
ibverbs_buffer::nnti_to_ib_flags(NNTI_buffer_flags_t nnti_flags)
{
    enum ibv_access_flags ibv_flags=static_cast<enum ibv_access_flags>(0);

    if (nnti_flags & NNTI_BF_LOCAL_READ) {
        // there is no equivalent flag in ibverbs
    }
    if (nnti_flags & NNTI_BF_LOCAL_WRITE) {
        ibv_flags = static_cast<enum ibv_access_flags>(ibv_flags | IBV_ACCESS_LOCAL_WRITE);
    }
    if (nnti_flags & NNTI_BF_REMOTE_READ) {
        ibv_flags = static_cast<enum ibv_access_flags>(ibv_flags | IBV_ACCESS_REMOTE_READ);
    }
    if (nnti_flags & NNTI_BF_REMOTE_WRITE) {
        ibv_flags = static_cast<enum ibv_access_flags>(ibv_flags | IBV_ACCESS_REMOTE_WRITE);
    }
    if (nnti_flags & NNTI_BF_REMOTE_ATOMIC) {
        ibv_flags = static_cast<enum ibv_access_flags>(ibv_flags | IBV_ACCESS_REMOTE_ATOMIC);
    }

    return ibv_flags;
}

} /* namespace datatype */
} /* namespace nnti */
