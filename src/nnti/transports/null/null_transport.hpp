// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file null_transport.hpp
 *
 * @brief null_transport.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

#ifndef NULL_TRANSPORT_HPP_
#define NULL_TRANSPORT_HPP_

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/base/base_transport.hpp"

#include "nnti/nnti_types.h"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_eq.hpp"
#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_wr.hpp"


namespace nnti  {
namespace transports {

class null_transport
: public base_transport {
private:
    /**
     * @brief Initialize NNTI to use a specific transport.
     *
     * \param[in]  config  configuration data
     * \return A result code (NNTI_OK or an error)
     *
     */
    null_transport(
        faodel::Configuration &config)
        : base_transport(NNTI_TRANSPORT_NULL,
                         config)
    {
        return;
    }

public:
    static null_transport *
    get_instance(
        faodel::Configuration &config)
    {
        static null_transport *instance = new null_transport(config);

        return instance;
    }

    /**
     * @brief Deactivates a specific transport.
     *
     * \return A result code (NNTI_OK or an error)
     */
    ~null_transport() override
    {
        return;
    }

    NNTI_result_t
    start(void) override
    {
        return NNTI_OK;
    }

    NNTI_result_t
    stop(void) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Indicates if a transport has been initialized.
     *
     * \return A result code (NNTI_OK or an error)
     *
     */
    bool
    initialized(void) override
    {
        return true;
    }

    /**
     * @brief Return the URL field of this transport.
     *
     * \param[out] url       A string that describes this process in a transport specific way.
     * \param[in]  maxlen    The length of the 'url' string parameter.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    get_url(
        char           *url,
        const uint64_t  maxlen) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Get the process ID of this process.
     *
     * \param[out] pid   the process ID of this process
     * \return A result code (NNTI_OK or an error)
     *
     */
    NNTI_result_t
    pid(NNTI_process_id_t *pid) override
    {
        *pid = 0;

        return NNTI_OK;
    }

    /**
     * @brief Get attributes of the transport.
     *
     * \param[out] attrs   the current attributes
     * \return A result code (NNTI_OK or an error)
     *
     */
    NNTI_result_t
    attrs(NNTI_attrs_t *attrs) override
    {
        memset(attrs, 0, sizeof(NNTI_attrs_t));

        return NNTI_OK;
    }

    /**
     * @brief Prepare for communication with the peer identified by url.
     *
     * \param[in]  url       A string that describes a peer's location on the network.
     * \param[in]  timeout   The amount of time (in milliseconds) to wait before aborting the connection attempt.
     * \param[out] peer_hdl  A handle to a peer that can be used for network operations.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    connect(
        const char  *url,
        const int    timeout,
        NNTI_peer_t *peer_hdl) override
    {
        *peer_hdl = (NNTI_peer_t)nullptr;

        return NNTI_OK;
    }

    /**
     * @brief Terminate communication with this peer.
     *
     * \param[in] peer_hdl  A handle to a peer.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    disconnect(
        NNTI_peer_t peer_hdl) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Create an event queue.
     *
     * \param[in]  size      The number of events the queue can hold.
     * \param[in]  flags     Control the behavior of the queue.
     * \param[out] eq        The new event queue.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    eq_create(
        uint64_t            size,
        NNTI_eq_flags_t     flags,
        NNTI_event_queue_t *eq) override
    {
        nnti::datatype::nnti_event_queue *new_eq = new nnti::datatype::nnti_event_queue(true, size, this);
        *eq = (NNTI_event_queue_t)new_eq;

        return NNTI_OK;
    }

    NNTI_result_t
    eq_create(
        uint64_t                             size,
        NNTI_eq_flags_t                      flags,
        nnti::datatype::nnti_event_callback  cb,
        void                                *cb_context,
        NNTI_event_queue_t                  *eq) override
    {
        nnti::datatype::nnti_event_queue *new_eq = new nnti::datatype::nnti_event_queue(true, size, cb, cb_context, this);
        *eq = (NNTI_event_queue_t)new_eq;

        return NNTI_OK;
    }

    /**
     * @brief Destroy an event queue.
     *
     * \param[in] eq  The event queue to destroy.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    eq_destroy(
        NNTI_event_queue_t eq) override
    {
        delete (nnti::datatype::nnti_event_queue *)eq;

        return NNTI_OK;
    }

    /**
     * @brief Wait for an event to arrive on an event queue.
     *
     * \param[in]  eq_list   A list of event queues to wait on.
     * \param[in]  eq_count  The number of event queues in the list.
     * \param[in]  timeout   The amount of time (in milliseconds) to wait.
     * \param[out] which     The index of the EQ where the event occurred.
     * \param[out] event     The details of the event.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    eq_wait(
        NNTI_event_queue_t *eq_list,
        const uint32_t      eq_count,
        const int           timeout,
        uint32_t           *which,
        NNTI_event_t       *event) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Retrieves the next message from the unexpected list.
     *
     * \param[in]  dst_hdl        Buffer where the message is delivered.
     * \param[in]  dst_offset     Offset into dst_hdl where the message is delivered.
     * \param[out] result_event   Event describing the message delivered to dst_hdl.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    next_unexpected(
        NNTI_buffer_t  dst_hdl,
        uint64_t       dst_offset,
        NNTI_event_t  *result_event) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Retrieves a specific message from the unexpected list.
     *
     * \param[in]  unexpected_event  Event describing the message to retrieve.
     * \param[in]  dst_hdl           Buffer where the message is delivered.
     * \param[in]  dst_offset        Offset into dst_hdl where the message is delivered.
     * \param[out] result_event      Event describing the message delivered to dst_hdl.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    get_unexpected(
        NNTI_event_t  *unexpected_event,
        NNTI_buffer_t  dst_hdl,
        uint64_t       dst_offset,
        NNTI_event_t  *result_event) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Marks a send operation as complete.
     *
     * \param[in] event  The event to mark complete.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    event_complete(
        NNTI_event_t *event) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Decode an array of bytes into an NNTI datatype.
     *
     * \param[out] nnti_dt        The NNTI data structure cast to void*.
     * \param[in]  packed_buf     A array of bytes containing the encoded data structure.
     * \param[in]  packed_len     The number of encoded bytes.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    dt_unpack(
        void           *nnti_dt,
        char           *packed_buf,
        const uint64_t  packed_len) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Allocate a block of memory and prepare it for network operations.
     *
     * \param[in]  size        The size (in bytes) of the new buffer.
     * \param[in]  flags       Control the behavior of this buffer.
     * \param[in]  eq          Events occurring on the memory region are delivered to this event queue.
     * \param[in]  cb          A callback that gets called for events delivered to eq.
     * \param[in]  cb_context  A blob of data that is passed to each invocation of cb.
     * \param[out] reg_ptr     A pointer to the memory buffer allocated.
     * \param[out] reg_buf     A handle to a memory buffer that can be used for network operations.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    alloc(
        const uint64_t                       size,
        const NNTI_buffer_flags_t            flags,
        NNTI_event_queue_t                   eq,
        nnti::datatype::nnti_event_callback  cb,
        void                                *cb_context,
        char                               **reg_ptr,
        NNTI_buffer_t                       *reg_buf) override
    {
        nnti::datatype::nnti_buffer *b = new nnti::datatype::nnti_buffer(this,
                                                                         size,
                                                                         flags,
                                                                         eq,
                                                                         cb,
                                                                         cb_context);
        *reg_ptr = b->payload();
        *reg_buf = (NNTI_buffer_t)b;

        return NNTI_OK;
    }

    /**
     * @brief Disables network operations on the block of memory and frees it.
     *
     * \param[in]  reg_buf The buffer to cleanup.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    free(
        NNTI_buffer_t reg_buf) override
    {
        nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;
        delete b;

        return NNTI_OK;
    }

    /**
     * @brief Prepare a block of memory for network operations.
     *
     * \param[in]  buffer      Pointer to a memory block.
     * \param[in]  size        The size (in bytes) of buffer.
     * \param[in]  flags       Control the behavior of this buffer.
     * \param[in]  eq          Events occurring on the memory region are delivered to this event queue.
     * \param[in]  cb          A callback that gets called for events delivered to eq.
     * \param[in]  cb_context  A blob of data that is passed to each invocation of cb.
     * \param[out] reg_buf     A handle to a memory buffer that can be used for network operations.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    register_memory(
        char                                *buffer,
        const uint64_t                       size,
        const NNTI_buffer_flags_t            flags,
        NNTI_event_queue_t                   eq,
        nnti::datatype::nnti_event_callback  cb,
        void                                *cb_context,
        NNTI_buffer_t                       *reg_buf) override
    {
        nnti::datatype::nnti_buffer *b = new nnti::datatype::nnti_buffer(this,
                                                                         buffer,
                                                                         size,
                                                                         flags,
                                                                         eq,
                                                                         cb,
                                                                         cb_context);
        *reg_buf = (NNTI_buffer_t)b;

        return NNTI_OK;
    }

    /**
     * @brief Disables network operations on a memory buffer.
     *
     * \param[in]  reg_buf  A handle to a memory buffer to unregister.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    unregister_memory(
        NNTI_buffer_t reg_buf) override
    {
        nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;
        delete b;

        return NNTI_OK;
    }

    /**
     * @brief Convert an NNTI peer to an NNTI_process_id_t.
     *
     * \param[in]   peer_hdl  A handle to a peer that can be used for network operations.
     * \param[out]  pid       Compact binary representation of a process's location on the network.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    dt_peer_to_pid(
        NNTI_peer_t        peer_hdl,
        NNTI_process_id_t *pid) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Convert an NNTI_process_id_t to an NNTI peer.
     *
     * \param[in]   pid       Compact binary representation of a process's location on the network.
     * \param[out]  peer_hdl  A handle to a peer that can be used for network operations.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    dt_pid_to_peer(
        NNTI_process_id_t  pid,
        NNTI_peer_t       *peer_hdl) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Send a message to a peer.
     *
     * \param[in]  wr   A work request that describes the operation
     * \param[out] wid  Identifier used to track this work request
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    send(
        nnti::datatype::nnti_work_request *wr,
        NNTI_work_id_t                    *wid) override
    {
        nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
        *wid = (NNTI_work_id_t)work_id;

        return NNTI_OK;
    }

    /**
     * @brief Transfer data to a peer.
     *
     * \param[in]  wr   A work request that describes the operation
     * \param[out] wid  Identifier used to track this work request
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    put(
        nnti::datatype::nnti_work_request *wr,
        NNTI_work_id_t                    *wid) override
    {
        nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
        *wid = (NNTI_work_id_t)work_id;

        return NNTI_OK;
    }

    /**
     * @brief Transfer data from a peer.
     *
     * \param[in]  wr   A work request that describes the operation
     * \param[out] wid  Identifier used to track this work request
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    get(
        nnti::datatype::nnti_work_request *wr,
        NNTI_work_id_t                    *wid) override
    {
        nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
        *wid = (NNTI_work_id_t)work_id;

        return NNTI_OK;
    }

    /**
     * perform a 64-bit atomic operation with GET semantics
     *
     * \param[in]  wr   A work request that describes the operation
     * \param[out] wid  Identifier used to track this work request
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    atomic_fop(
        nnti::datatype::nnti_work_request *wr,
        NNTI_work_id_t                    *wid) override
    {
        nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
        *wid = (NNTI_work_id_t)work_id;

        return NNTI_OK;
    }

    /**
     * perform a 64-bit compare-and-swap operation
     *
     * \param[in]  wr   A work request that describes the operation
     * \param[out] wid  Identifier used to track this work request
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    atomic_cswap(
        nnti::datatype::nnti_work_request *wr,
        NNTI_work_id_t                    *wid) override
    {
        nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
        *wid = (NNTI_work_id_t)work_id;

        return NNTI_OK;
    }

    /**
     * @brief Attempts to cancel an NNTI operation.
     *
     * \param[in]  wid   A work ID to cancel.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    cancel(
        NNTI_work_id_t wid) override
    {
        return NNTI_OK;
    }


    /**
     * @brief Attempts to cancel a list of NNTI operations.
     *
     * \param[in]  wid_list   A list of work IDs to cancel.
     * \param[in]  wid_count  The number of work IDs in wid_list.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    cancelall(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count) override
    {
        return NNTI_OK;
    }


    /**
     * @brief Sends a signal to interrupt NNTI_wait*().
     *
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    interrupt() override
    {
        return NNTI_OK;
    }


    /**
     * @brief Wait for a specific operation (wid) to complete.
     *
     * \param[in]  wid      The operation to wait for.
     * \param[in]  timeout  The amount of time (in milliseconds) to wait.
     * \param[out] status   The details of the completed (or timed out) operation.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    wait(
        NNTI_work_id_t  wid,
        const int64_t   timeout,
        NNTI_status_t  *status) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Wait for any operation (wid_list) in the list to complete.
     *
     * \param[in]  wid_list   The list of operations to wait for.
     * \param[in]  wid_count  The number of operations in wid_list.
     * \param[in]  timeout    The amount of time (in milliseconds) to wait.
     * \param[out] which      The index of the operation that completed.
     * \param[out] status     The details of the completed (or timed out) operation.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    waitany(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count,
        const int64_t   timeout,
        uint32_t       *which,
        NNTI_status_t  *status) override
    {
        return NNTI_OK;
    }

    /**
     * @brief Waits for all the operations (wid_list) in the list to complete.
     *
     * \param[in]  wid_list   The list of operations to wait for.
     * \param[in]  wid_count  The number of operations in wid_list.
     * \param[in]  timeout    The amount of time (in milliseconds) to wait.
     * \param[out] status     The details of the completed (or timed out) operations (one per operation).
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    waitall(
        NNTI_work_id_t *wid_list,
        const uint32_t  wid_count,
        const int64_t   timeout,
        NNTI_status_t  *status) override
    {
        return NNTI_OK;
    }

};

} /* namespace transports */
} /* namespace nnti */

#endif /* NULL_TRANSPORT_HPP_*/
