// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti.cpp
 *
 * @brief nnti.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <string.h>

#include <cstdlib>

#include "nnti/nnti.h"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_packable.h"
#include "nnti/nnti_transport.hpp"
#include "nnti/transport_factory.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/nnti_util.hpp"


/*
 *********************************************************
 *********************************************************
 *
 * C API
 *
 *********************************************************
 *********************************************************
 */

static void setup_logging(void);

/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * Enable the use of a particular transport by this process.  <tt>my_url</tt>
 * allows the process to have some control (if possible) over the
 * URL assigned for the transport.  For example, a Portals URL to put
 * might be "ptl://-1,128".  This would tell Portals to use the default
 * network ID, but use PID=128.  If the transport
 * can be initialized without this info (eg. a Portals client), <tt>my_url</tt> can
 * be NULL or empty.
 *
 */
extern "C"
NNTI_result_t NNTI_init(
        const NNTI_transport_id_t  trans_id,
        const char                *my_url,
        NNTI_transport_t          *trans_hdl)
{
    NNTI_result_t  rc = NNTI_OK;
    const char    *url = (my_url==nullptr) ? "" : my_url;

    setup_logging();

    nnti::transports::transport *transport = nnti::transports::factory::get_instance(trans_id);

    transport->start();

    *trans_hdl = nnti::transports::transport::to_hdl(transport);

    return(rc);
}


extern "C"
NNTI_result_t NNTI_initialized(
        const NNTI_transport_id_t  trans_id,
        int                       *is_init)
{
    NNTI_result_t                rc = NNTI_OK;

    *is_init = nnti::transports::factory::exists(trans_id);
    if (*is_init) {
        nnti::transports::transport *transport = nnti::transports::factory::get_instance(trans_id);
        if (transport->initialized()) {
            *is_init = 1;
        } else {
            *is_init = 0;
        }
    } else {
        return NNTI_ENOENT;
    }

    return NNTI_OK;
}


/**
 * @brief Return the URL field of this transport.
 *
 * Return the URL field of this transport.  After initialization, the transport will
 * have a specific location on the network where peers can contact it.  The
 * transport will convert this location to a string that other instances of the
 * transport will recognize.
 *
 * URL format: "transport://address/memory_descriptor"
 *    - transport - (required) identifies how the URL should parsed
 *    - address   - (required) uniquely identifies a location on the network
 *                - ex. "ptl://nid:pid/", "ib://ip_addr:port"
 *    - memory_descriptor - (optional) transport-specific representation of RMA params
 *
 */
extern "C"
NNTI_result_t NNTI_get_url(
        const NNTI_transport_t  trans_hdl,
        char                   *url,
        const uint64_t          maxlen)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    rc = t->get_url(
            url,
            maxlen);

    return(rc);
}

/**
 * @brief Get the process ID of this process.
 *
 * \param[out] pid   the process ID of this process
 * \return A result code (NNTI_OK or an error)
 *
 */
extern "C"
NNTI_result_t NNTI_get_pid(
        const NNTI_transport_t  trans_hdl,
        NNTI_process_id_t      *pid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    rc = t->pid(
            pid);

    return(rc);
}

/**
 * @brief Get attributes of the transport.
 *
 * \param[out] attrs   the current attributes
 * \return A result code (NNTI_OK or an error)
 *
 */
extern "C"
NNTI_result_t NNTI_get_attrs(
        const NNTI_transport_t  trans_hdl,
        NNTI_attrs_t           *attrs)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    rc = t->attrs(
            attrs);

    return(rc);
}

/**
 * @brief Prepare for communication with the peer identified by <tt>url</tt>.
 *
 * Parse <tt>url</tt> in a transport specific way.  Perform any transport specific
 * actions necessary to begin communication with this peer.
 *
 */
extern "C"
NNTI_result_t NNTI_connect(
        const NNTI_transport_t  trans_hdl,
        const char             *url,
        const int               timeout,
        NNTI_peer_t            *peer_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    rc = t->connect(
            url,
            timeout,
            peer_hdl);

    return(rc);
}


/**
 * @brief Terminate communication with this peer.
 *
 * Perform any transport specific actions necessary to end communication with
 * this peer.
 *
 */
extern "C"
NNTI_result_t NNTI_disconnect(
        const NNTI_transport_t trans_hdl,
        NNTI_peer_t            peer_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    rc = t->disconnect(
            peer_hdl);

    return(rc);
}


/**
 * @brief Create an event queue.
 */
extern "C"
NNTI_result_t NNTI_eq_create(
        const NNTI_transport_t  trans_hdl,
        uint64_t                size,
        NNTI_eq_flags_t         flags,
        NNTI_event_callback_t   cb,
        void                   *cb_context,
        NNTI_event_queue_t     *eq)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    if (cb == nullptr) {
        rc = t->eq_create(
                size,
                flags,
                eq);
    } else {
        nnti::datatype::nnti_event_callback cb_functor(t, cb);

        rc = t->eq_create(
                size,
                flags,
                cb_functor,
                cb_context,
                eq);
    }

    return(rc);
}


/**
 * @brief Destroy an event queue.
 */
extern "C"
NNTI_result_t NNTI_eq_destroy(
        NNTI_event_queue_t eq)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)eq)->transport();

    rc = t->eq_destroy(
            eq);

    return(rc);
}


/**
 * @brief Wait for an event to arrive on an event queue.
 */
extern "C"
NNTI_result_t NNTI_eq_wait(
        NNTI_event_queue_t *eq_list,
        const uint32_t      eq_count,
        const int           timeout,
        uint32_t           *which,
        NNTI_event_t       *event)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nullptr;

    for (uint32_t i=0;i<eq_count;i++) {
        if (eq_list[i]) {
            t = ((nnti::datatype::nnti_datatype *)eq_list[i])->transport();
            break;
        }
    }

    rc = t->eq_wait(
            eq_list,
            eq_count,
            timeout,
            which,
            event);

    return(rc);
}


/**
 * @brief Retrieves the next message from the unexpected list.
 */
extern "C"
NNTI_result_t NNTI_next_unexpected(
        NNTI_buffer_t  dst_hdl,
        uint64_t       dst_offset,
        NNTI_event_t  *result_event)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)dst_hdl)->transport();

    rc = t->next_unexpected(
            dst_hdl,
            dst_offset,
            result_event);

    return(rc);
}


/**
 * @brief Retrieves a specific message from the unexpected list.
 */
extern "C"
NNTI_result_t NNTI_get_unexpected(
        NNTI_event_t  *unexpected_event,
        NNTI_buffer_t  dst_hdl,
        uint64_t       dst_offset,
        NNTI_event_t  *result_event)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(unexpected_event->trans_hdl);

    rc = t->get_unexpected(
            unexpected_event,
            dst_hdl,
            dst_offset,
            result_event);

    return(rc);
}


/**
 * @brief Marks a send operation as complete.
 */
extern "C"
NNTI_result_t NNTI_event_complete(
        NNTI_event_t *event)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(event->trans_hdl);

    rc = t->event_complete(
            event);

    return(rc);
}


/**
 * @brief Allocate a block of memory and prepare it for network operations.
 *
 * Allocate a block of memory in a transport specific way and wrap it in an
 * NNTI_buffer_t.  The transport may take additional actions to prepare the
 * memory for network send/receive.
 *
 */
extern "C"
NNTI_result_t NNTI_alloc(
        const NNTI_transport_t     trans_hdl,
        const uint64_t             size,
        const NNTI_buffer_flags_t  flags,
        NNTI_event_queue_t         eq,
        NNTI_event_callback_t      cb,
        void                      *cb_context,
        char                     **reg_ptr,
        NNTI_buffer_t             *reg_buf)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    nnti::datatype::nnti_event_callback cb_functor(t, cb);

    rc = t->alloc(
            size,
            flags,
            eq,
            cb_functor,
            cb_context,
            reg_ptr,
            reg_buf);

    return(rc);
}


/**
 * @brief Cleanup after network operations are complete and release the block
 * of memory.
 *
 * Destroy an NNTI_buffer_t that was previously created by NNTI_alloc().
 *
 */
extern "C"
NNTI_result_t NNTI_free(
        NNTI_buffer_t reg_buf)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)reg_buf)->transport();

    rc = t->free(
            reg_buf);

    return(rc);
}


/**
 * @brief Prepare a block of memory for network operations.
 *
 * Wrap a user allocated block of memory in an NNTI_buffer_t.  The transport
 * may take additional actions to prepare the memory for network send/receive.
 * If the memory block doesn't meet the transport's requirements for memory
 * regions, then errors or poor performance may result.
 *
 */
extern "C"
NNTI_result_t NNTI_register_memory(
        const NNTI_transport_t    trans_hdl,
        char                      *buffer,
        const uint64_t             size,
        const NNTI_buffer_flags_t  flags,
        NNTI_event_queue_t         eq,
        NNTI_event_callback_t      cb,
        void                      *cb_context,
        NNTI_buffer_t             *reg_buf)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    nnti::datatype::nnti_event_callback cb_functor(t, cb);

    rc = t->register_memory(
            buffer,
            size,
            flags,
            eq,
            cb_functor,
            cb_context,
            reg_buf);

    return(rc);
}


/**
 * @brief Cleanup after network operations are complete.
 *
 * Destroy an NNTI_buffer_t that was previously created by NNTI_regsiter_buffer().
 * It is the user's responsibility to release the the memory region.
 *
 */
extern "C"
NNTI_result_t NNTI_unregister_memory(
        NNTI_buffer_t reg_buf)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)reg_buf)->transport();

    rc = t->unregister_memory(
            reg_buf);

    return(rc);
}


/**
 * @brief Calculate the number of bytes required to store an encoded NNTI datatype.
 *
 */
extern "C"
NNTI_result_t NNTI_dt_sizeof(
        const NNTI_transport_t  trans_hdl,
        void                   *nnti_dt,
        uint64_t               *packed_len)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    t->dt_sizeof(
            nnti_dt,
            packed_len);

    return(rc);
}


/**
 * @brief Encode an NNTI datatype into an array of bytes.
 *
 */
extern "C"
NNTI_result_t NNTI_dt_pack(
        const NNTI_transport_t  trans_hdl,
        void                   *nnti_dt,
        char                   *packed_buf,
        uint64_t                packed_buflen)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    t->dt_pack(
            nnti_dt,
            packed_buf,
            packed_buflen);

    return(rc);
}


/**
 * @brief Decode an array of bytes into an NNTI datatype.
 *
 */
extern "C"
NNTI_result_t NNTI_dt_unpack(
        const NNTI_transport_t  trans_hdl,
        void                   *nnti_dt,
        char                   *packed_buf,
        uint64_t                packed_len)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    t->dt_unpack(
            nnti_dt,
            packed_buf,
            packed_len);

    return(rc);
}


/**
 * @brief Free a variable size NNTI datatype that was unpacked with NNTI_dt_unpack().
 *
 */
extern "C"
NNTI_result_t NNTI_dt_free(
        const NNTI_transport_t  trans_hdl,
        void                   *nnti_dt)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    t->dt_free(
            nnti_dt);

    return(rc);
}


/**
 * @brief Convert an NNTI URL to an NNTI_process_id_t.
 */
extern "C"
NNTI_result_t NNTI_dt_url_to_pid(
        const char        *url,
        NNTI_process_id_t *pid)
{
    return nnti::transports::transport::dt_url_to_pid(url, pid);
}


/**
 * @brief Convert an NNTI_process_id_t to an NNTI URL.
 */
extern "C"
NNTI_result_t NNTI_dt_pid_to_url(
        const NNTI_process_id_t  pid,
        char                    *url,
        const uint64_t           maxlen)
{
    return nnti::transports::transport::dt_pid_to_url(pid, url, maxlen);
}


/**
 * @brief Send a message to a peer.
 *
 * Send a message (<tt>msg_hdl</tt>) to a peer (<tt>peer_hdl</tt>).  It is expected that the
 * message is small, but the exact maximum size is transport dependent.
 *
 */
extern "C"
NNTI_result_t NNTI_send(
        NNTI_work_request_t *wr,
        NNTI_work_id_t      *wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(wr->trans_hdl);

    t->send(
            new nnti::datatype::nnti_work_request(t, *wr),
            wid);

    return(rc);
}


/**
 * @brief Transfer data to a peer.
 *
 * Put the contents of <tt>src_buffer_hdl</tt> into <tt>dest_buffer_hdl</tt>.  It is
 * assumed that the destination is at least <tt>src_length</tt> bytes in size.
 *
 */
extern "C"
NNTI_result_t NNTI_put(
        NNTI_work_request_t *wr,
        NNTI_work_id_t      *wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(wr->trans_hdl);

    t->put(
            new nnti::datatype::nnti_work_request(t, *wr),
            wid);

    return(rc);
}


/**
 * @brief Transfer data from a peer.
 *
 * Get the contents of <tt>src_buffer_hdl</tt> into <tt>dest_buffer_hdl</tt>.  It is
 * assumed that the destination is at least <tt>src_length</tt> bytes in size.
 *
 */
extern "C"
NNTI_result_t NNTI_get(
        NNTI_work_request_t *wr,
        NNTI_work_id_t      *wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(wr->trans_hdl);

    t->get(
            new nnti::datatype::nnti_work_request(t, *wr),
            wid);

    return(rc);
}


extern "C"
NNTI_result_t NNTI_atomic_fop(
        NNTI_work_request_t *wr,
        NNTI_work_id_t      *wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(wr->trans_hdl);

    t->atomic_fop(
            new nnti::datatype::nnti_work_request(t, *wr),
            wid);

    return(rc);
}


extern "C"
NNTI_result_t NNTI_atomic_cswap(
        NNTI_work_request_t *wr,
        NNTI_work_id_t      *wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(wr->trans_hdl);

    t->atomic_cswap(
            new nnti::datatype::nnti_work_request(t, *wr),
            wid);

    return(rc);
}


/**
 * @brief Attempts to cancel an NNTI opertion.
 *
 */
extern "C"
NNTI_result_t NNTI_cancel(
        NNTI_work_id_t wid)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)wid)->transport();

    rc = t->cancel(
            wid);

    return(rc);
}


/**
 * @brief Attempts to cancel a list of NNTI opertions.
 *
 */
extern "C"
NNTI_result_t NNTI_cancelall(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count)
{
    NNTI_result_t       rc=NNTI_OK;
    NNTI_transport_id_t id=NNTI_TRANSPORT_NULL;
    uint32_t            i=0;

    nnti::transports::transport *t=nullptr;

    for (uint32_t i=0;i<wid_count;i++) {
        if (wid_list[i]) {
            t = ((nnti::datatype::nnti_datatype *)wid_list[i])->transport();
            break;
        }
    }

    rc = t->cancelall(
            wid_list,
            wid_count);

    return(rc);
}


/**
 * @brief Interrupts NNTI_wait*()
 *
 */
extern "C"
NNTI_result_t NNTI_interrupt(
        const NNTI_transport_t trans_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = nnti::transports::transport::to_obj(trans_hdl);

    t->interrupt();

    return(rc);
}


/**
 * @brief Wait for <tt>remote_op</tt> on <tt>reg_buf</tt> to complete.
 *
 * Wait for <tt>remote_op</tt> on <tt>reg_buf</tt> to complete or timeout
 * waiting.  This is typically used to wait for a result or a bulk data
 * transfer.  The timeout is specified in milliseconds.  A timeout of <tt>-1</tt>
 * means wait forever.  A timeout of <tt>0</tt> means do not wait.
 *
 */
extern "C"
NNTI_result_t NNTI_wait(
        NNTI_work_id_t  wid,
        const int64_t   timeout,
        NNTI_status_t  *status)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = ((nnti::datatype::nnti_datatype *)wid)->transport();

    t->wait(
            wid,
            timeout,
            status);

    return(rc);
}


/**
 * @brief Wait for <tt>remote_op</tt> on any buffer in <tt>buf_list</tt> to complete.
 *
 * Wait for <tt>remote_op</tt> on any buffer in <tt>buf_list</tt> to complete or timeout
 * waiting.  This is typically used to wait for a result or a bulk data
 * transfer.  The timeout is specified in milliseconds.  A timeout of <tt>-1</tt>
 * means wait forever.  A timeout of <tt>0</tt> means do not wait.
 *
 * Caveats:
 *   1) All buffers in buf_list must be registered with the same transport.
 *   2) You can't wait on the receive queue and RDMA buffers in the same call.  Will probably be fixed in the future.
 *
 */
extern "C"
NNTI_result_t NNTI_waitany(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count,
        const int64_t   timeout,
        uint32_t       *which,
        NNTI_status_t  *status)
{
    NNTI_result_t       rc=NNTI_OK;
    NNTI_transport_id_t id=NNTI_TRANSPORT_NULL;
    uint32_t            i=0;

    nnti::transports::transport *t=nullptr;

    for (uint32_t i=0;i<wid_count;i++) {
        if (wid_list[i]) {
            t = ((nnti::datatype::nnti_datatype *)wid_list[i])->transport();
            break;
        }
    }

    rc = t->waitany(
            wid_list,
            wid_count,
            timeout,
            which,
            status);

    return(rc);
}


/**
 * @brief Wait for <tt>remote_op</tt> on all buffers in <tt>buf_list</tt> to complete.
 *
 * Wait for <tt>remote_op</tt> on all buffers in <tt>buf_list</tt> to complete or timeout
 * waiting.  This is typically used to wait for a result or a bulk data
 * transfer.  The timeout is specified in milliseconds.  A timeout of <tt>-1</tt>
 * means wait forever.  A timeout of <tt>0</tt> means do not wait.
 *
 * Caveats:
 *   1) All buffers in buf_list must be registered with the same transport.
 *   2) You can't wait on the receive queue and RDMA buffers in the same call.  Will probably be fixed in the future.
 *
 */
extern "C"
NNTI_result_t NNTI_waitall(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count,
        const int64_t   timeout,
        NNTI_status_t  *status)
{
    NNTI_result_t rc=NNTI_OK;
    NNTI_transport_id_t id=NNTI_TRANSPORT_NULL;
    uint32_t i=0;

    nnti::transports::transport *t=nullptr;

    for (uint32_t i=0;i<wid_count;i++) {
        if (wid_list[i]) {
            t = ((nnti::datatype::nnti_datatype *)wid_list[i])->transport();
            break;
        }
    }

    rc = t->waitall(
            wid_list,
            wid_count,
            timeout,
            status);

    return(rc);
}


/**
 * @brief Disable this transport.
 *
 * Shutdown the transport.  Any outstanding sends, gets and puts will be
 * canceled.  Any new transport requests will fail.
 *
 */
extern "C"
NNTI_result_t NNTI_fini(
        const NNTI_transport_t trans_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    nnti::transports::transport *t = (nnti::transports::transport *)trans_hdl;

    t->stop();

    return(rc);
}


static void setup_logging(void)
{
    char                logfile[1024];
    sbl::severity_level severity = sbl::severity_level::error;
    bool                include_ffl = false;

    const char *log_filename = getenv("NNTI_LOG_FILENAME");
    const char *log_fileper  = getenv("NNTI_LOG_FILEPER");
    const char *log_level    = getenv("NNTI_LOG_LEVEL");
    const char *log_ffl      = getenv("NNTI_LOG_FFL");

    if (log_level) {
        if (!strcasecmp(log_level, "FATAL") || !strcmp(log_level, "5")) {
            severity = sbl::severity_level::fatal;
        } else if (!strcasecmp(log_level, "ERROR") || !strcmp(log_level, "4")) {
            severity = sbl::severity_level::error;
        } else if (!strcasecmp(log_level, "WARNING") || !strcmp(log_level, "3")) {
            severity = sbl::severity_level::warning;
        } else if (!strcasecmp(log_level, "INFO") || !strcmp(log_level, "2")) {
            severity = sbl::severity_level::info;
        } else if (!strcasecmp(log_level, "DEBUG") || !strcmp(log_level, "1")) {
            severity = sbl::severity_level::debug;
        }
    }

    if (log_ffl) {
        if (!strcasecmp(log_ffl, "TRUE") || !strcmp(log_ffl, "1")) {
            include_ffl = true;
        }
    }

    if (log_filename) {
        if (log_fileper != nullptr && (!strcasecmp(log_fileper, "TRUE") || !strcmp(log_fileper, "1"))) {
            int mypid = getpid();
            sprintf(logfile, "%s.%d.log", log_filename, mypid);
        } else {
            strcpy(logfile, log_filename);
        }
        nnti::core::logger::init(logfile, include_ffl, severity);
    } else {
        nnti::core::logger::init(include_ffl, severity);
    }
}
