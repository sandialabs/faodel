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

#include <alps/libalpslli.h>
#include <gni_pub.h>

#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"

#include "nnti/nnti_callback.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_buffer.hpp"

namespace nnti  {
namespace datatype {

ugni_buffer::ugni_buffer()
    : nnti_buffer(),
      registered_(false)
{
    memset(&hdl_, 0, sizeof(gni_mem_handle_t));
    return;
}
ugni_buffer::ugni_buffer(
    nnti::datatype::ugni_buffer &b)
: nnti_buffer(b),
  registered_(false)
{
    hdl_ = b.hdl_;
    memcpy(packed_, b.packed_, packed_size_);

    return;
}
ugni_buffer::ugni_buffer(
    nnti::transports::ugni_transport *transport,
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
              cb_context),
  registered_(false)
{
    register_buffer();
    internal_pack();

    return;
}
ugni_buffer::ugni_buffer(
    nnti::transports::ugni_transport *transport,
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
              cb_context),
  registered_(false)
{
    register_buffer();
    internal_pack();

    return;
}
ugni_buffer::ugni_buffer(
    nnti::transports::transport *transport,
    char                        *packed_buf,
    const uint64_t               packed_len)
: nnti_buffer(transport,
              packed_buf,
              packed_len),
  registered_(false)
{
    payload_      = (char *)packable_.buffer.NNTI_remote_addr_p_t_u.ugni.buf;
    payload_size_ = packable_.buffer.NNTI_remote_addr_p_t_u.ugni.size;

    log_debug("ugni_buffer", "ctor unpack - segments[0].buf(%016lX) segments[0].size(%u)",
        packable_.buffer.NNTI_remote_addr_p_t_u.ugni.buf, packable_.buffer.NNTI_remote_addr_p_t_u.ugni.size);

    return;
}

ugni_buffer::~ugni_buffer()
{
    if (registered_) {
        unregister_buffer();
    }

    return;
}

char*
ugni_buffer::payload(void)
{
    return payload_;
}

void *
ugni_buffer::addr(void)
{
    return (void *)packable_.buffer.NNTI_remote_addr_p_t_u.ugni.buf;
}

size_t
ugni_buffer::length(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.ugni.size;
}

NNTI_ugni_mem_hdl_p_t
ugni_buffer::mem_hdl(void)
{
    return packable_.buffer.NNTI_remote_addr_p_t_u.ugni.mem_hdl;
}


NNTI_result_t
ugni_buffer::register_buffer(void)
{
    gni_return_t  gni_rc  = GNI_RC_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;
    uint32_t      access;

    transports::ugni_transport *ugni_transport = (transports::ugni_transport*)transport_;

    log_debug("ugni_buffer", "enter buffer(%p) len(%d)", payload_, payload_size_);

    memset(&packable_, 0, sizeof(NNTI_buffer_p_t));

    access = nnti_to_ugni_flags(flags_);

    gni_rc = GNI_MemRegister(
                ugni_transport->nic_hdl_,
                (uint64_t)payload_,
                payload_size_,
                ugni_transport->rdma_mem_cq_hdl_,
                access,
                (uint32_t)-1,
                &hdl_);
    if (gni_rc!=GNI_RC_SUCCESS) {
        log_error("ugni_buffer", "MemRegister(mem_hdl) failed: gni_rc=%d, %s", gni_rc, strerror(errno));
        nnti_rc = NNTI_EPERM;
        goto cleanup;
    }

    packable_.buffer.transport_id                               = NNTI_TRANSPORT_UGNI;
    packable_.buffer.NNTI_remote_addr_p_t_u.ugni.size           = payload_size_;
    packable_.buffer.NNTI_remote_addr_p_t_u.ugni.buf            = (uint64_t)payload_;
    packable_.buffer.NNTI_remote_addr_p_t_u.ugni.mem_hdl.qword1 = (uint64_t)hdl_.qword1;
    packable_.buffer.NNTI_remote_addr_p_t_u.ugni.mem_hdl.qword2 = (uint64_t)hdl_.qword2;

    registered_ = true;

    log_debug("ugni_buffer", "register rdma_mem_cq_hdl          = %llu", (uint64_t)ugni_transport->rdma_mem_cq_hdl_);
    log_debug("ugni_buffer", "register hdl_->mem_hdl = (%llu,%llu)", (uint64_t)mem_hdl().qword1, (uint64_t)mem_hdl().qword2);

cleanup:
    switch(gni_rc) {
        case GNI_RC_SUCCESS:
            nnti_rc = NNTI_OK;
            break;
        default:
            nnti_rc = NNTI_EIO;
            break;
    }

    log_debug("ugni_buffer", "exit payload_(%p) payload_size_(%llu) gni_rc(%d) nnti_rc(%d)", payload_, payload_size_, gni_rc, nnti_rc);

    return nnti_rc;
}

NNTI_result_t
ugni_buffer::unregister_buffer(void)
{
    int rc=GNI_RC_SUCCESS; /* return code */

    transports::ugni_transport *ugni_transport = (transports::ugni_transport*)transport_;

    log_debug("ugni_buffer", "enter mem_hdl(%p) mem_hdl=(%llu,%llu)", &hdl_, (uint64_t)hdl_.qword1, (uint64_t)hdl_.qword2);

    rc=GNI_MemDeregister (ugni_transport->nic_hdl_, &hdl_);
    if (rc!=GNI_RC_SUCCESS) {
        log_error("ugni_buffer", "MemDeregister(mem_hdl) failed: %d", rc);
    }

    switch(rc) {
        case GNI_RC_SUCCESS:
            rc=NNTI_OK;
        default:
            rc=NNTI_EIO;
    }

    log_debug("ugni_buffer", "exit mem_hdl(%p)", &hdl_);

    registered_ = false;

    return NNTI_OK;
}

uint32_t
ugni_buffer::nnti_to_ugni_flags(
    NNTI_buffer_flags_t nnti_flags)
{
    uint32_t ugni_flags=0;

    if (nnti_flags & NNTI_BF_LOCAL_READ) {
        // there is no equivalent flag in ugni
    }
    if (nnti_flags & NNTI_BF_LOCAL_WRITE) {
        // there is no equivalent flag in ugni
    }
    if ((nnti_flags & NNTI_BF_REMOTE_READ) && !(nnti_flags & NNTI_BF_REMOTE_WRITE)) {
        ugni_flags = GNI_MEM_READ_ONLY;
    }
    if (nnti_flags & NNTI_BF_REMOTE_WRITE) {
        ugni_flags = GNI_MEM_READWRITE;
    }

    return ugni_flags;
}

} /* namespace datatype */
} /* namespace nnti */
