// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti.h
 *
 * @brief nnti.h
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

#ifndef NNTI_H_
#define NNTI_H_


#include <nnti/nnti_packable.h>
#include <nnti/nnti_types.h>


#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup nnti_impl
 *  @{
 */

/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * \param[in]  trans_id  The ID of the transport the client wants to use.
 * \param[in]  my_url    A string that describes the transport parameters.
 * \param[out] trans_hdl A handle to the activated transport.
 * \return A result code (NNTI_OK or an error)
 *
 */
NNTI_result_t NNTI_init(
    const NNTI_transport_id_t  trans_id,
    const char                *my_url,
    NNTI_transport_t          *trans_hdl);

/**
 * @brief Indicates if a transport has been initialized.
 *
 * \param[in]  trans_id  The ID of the transport to test.
 * \param[out] is_init   1 if the transport is initialized, 0 otherwise.
 * \return A result code (NNTI_OK or an error)
 *
 */
NNTI_result_t NNTI_initialized(
    const NNTI_transport_id_t  trans_id,
    int                       *is_init);

/**
 * @brief Return the URL field of this transport.
 *
 * \param[in]  trans_hdl A handle to the activated transport.
 * \param[out] url       A string that describes this process in a transport specific way.
 * \param[in]  maxlen    The length of the 'url' string parameter.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_get_url(
    const NNTI_transport_t  trans_hdl,
    char                   *url,
    const uint64_t          maxlen);

/**
 * @brief Get the process ID of this process.
 *
 * \param[out] pid   the process ID of this process
 * \return A result code (NNTI_OK or an error)
 *
 */
NNTI_result_t NNTI_get_pid(
        const NNTI_transport_t  trans_hdl,
        NNTI_process_id_t      *pid);

/**
 * @brief Get attributes of the transport.
 *
 * \param[out] attrs   the current attributes
 * \return A result code (NNTI_OK or an error)
 *
 */
NNTI_result_t NNTI_get_attrs(
    const NNTI_transport_t  trans_hdl,
    NNTI_attrs_t           *attrs);

/**
 * @brief Prepare for communication with the peer identified by url.
 *
 * \param[in]  trans_hdl A handle to an activated transport.
 * \param[in]  url       A string that describes a peer's location on the network.
 * \param[in]  timeout   The amount of time (in milliseconds) to wait before aborting the connection attempt.
 * \param[out] peer_hdl  A handle to a peer that can be used for network operations.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_connect(
    const NNTI_transport_t  trans_hdl,
    const char             *url,
    const int               timeout,
    NNTI_peer_t            *peer_hdl);

/**
 * @brief Terminate communication with this peer.
 *
 * \param[in] trans_hdl A handle to an activated transport.
 * \param[in] peer_hdl  A handle to a peer.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_disconnect(
    const NNTI_transport_t  trans_hdl,
    NNTI_peer_t             peer_hdl);

/**
 * @brief Create an event queue.
 *
 * \param[in]  trans_hdl A handle to an activated transport.
 * \param[in]  size      The number of events the queue can hold.
 * \param[in]  flags     Control the behavior of the queue.
 * \param[out] eq        The new event queue.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_eq_create(
    const NNTI_transport_t  trans_hdl,
    uint64_t                size,
    NNTI_eq_flags_t         flags,
    NNTI_event_callback_t   cb,
    void                   *cb_context,
    NNTI_event_queue_t     *eq);

/**
 * @brief Destroy an event queue.
 *
 * \param[in] eq  The event queue to destroy.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_eq_destroy(
    NNTI_event_queue_t eq);

/**
 * @brief Wait for an event to arrive on an event queue.
 *
 * \param[in]  eq_list   A list of event queues to wait on.
 * \param[in]  eq_count  The number of event queues in the list.
 * \param[in]  timeout   The amount of time (in milliseconds) to wait.
 * \param[out] event     The details of the event.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_eq_wait(
    NNTI_event_queue_t *eq_list,
    const uint32_t      eq_count,
    const int           timeout,
    uint32_t           *which,
    NNTI_event_t       *event);

/**
 * @brief Retrieves the next message from the unexpected list.
 *
 * \param[in]  dst_hdl        Buffer where the message is delivered.
 * \param[in]  dst_offset     Offset into dst_hdl where the message is delivered.
 * \param[out] reseult_event  Event describing the message delivered to dst_hdl.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_next_unexpected(
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event);

/**
 * @brief Retrieves a specific message from the unexpected list.
 *
 * \param[in]  unexpect_event  Event describing the message to retrieve.
 * \param[in]  dst_hdl         Buffer where the message is delivered.
 * \param[in]  dst_offset      Offset into dst_hdl where the message is delivered.
 * \param[out] reseult_event   Event describing the message delivered to dst_hdl.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_get_unexpected(
    NNTI_event_t  *unexpected_event,
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event);

/**
 * @brief Marks a send operation as complete.
 *
 * \param[in] event  The event to mark complete.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_event_complete(
    NNTI_event_t *event);

/**
 * @brief Allocate a block of memory and prepare it for network operations.
 *
 * \param[in]  trans_hdl   A handle to an activated transport.
 * \param[in]  size        The size (in bytes) of the new buffer.
 * \param[in]  flags       Control the behavior of this buffer.
 * \param[in]  eq          Events occurring on the memory region are delivered to this event queue.
 * \param[in]  cb          A callback that gets called for events delivered to eq.
 * \param[in]  cb_context  A blob of data that is passed to each invocation of cb.
 * \param[out] reg_ptr     A pointer to the memory block allocated.
 * \param[out] reg_buf     A handle to a memory buffer that can be used for network operations.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_alloc(
    const NNTI_transport_t     trans_hdl,
    const uint64_t             size,
    const NNTI_buffer_flags_t  flags,
    NNTI_event_queue_t         eq,
    NNTI_event_callback_t      cb,
    void                      *cb_context,
    char                     **reg_ptr,
    NNTI_buffer_t             *reg_buf);

/**
 * @brief Disables network operations on the block of memory and frees it.
 *
 * \param[in]  reg_buf The buffer to cleanup.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_free(
    NNTI_buffer_t reg_buf);

/**
 * @brief Prepare a block of memory for network operations.
 *
 * \param[in]  trans_hdl   A handle to an activated transport.
 * \param[in]  buffer      Pointer to a memory block.
 * \param[in]  size        The size (in bytes) of buffer.
 * \param[in]  flags       Control the behavior of this buffer.
 * \param[in]  eq          Events occurring on the memory region are delivered to this event queue.
 * \param[in]  cb          A callback that gets called for events delivered to eq.
 * \param[in]  cb_context  A blob of data that is passed to each invocation of cb.
 * \param[out] reg_buf     A handle to a memory buffer that can be used for network operations.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_register_memory(
    const NNTI_transport_t     trans_hdl,
    char                      *buffer,
    const uint64_t             size,
    const NNTI_buffer_flags_t  flags,
    NNTI_event_queue_t         eq,
    NNTI_event_callback_t      cb,
    void                      *cb_context,
    NNTI_buffer_t             *reg_buf);

/**
 * @brief Disables network operations on a memory buffer.
 *
 * \param[in]  reg_buf  A handle to a memory buffer to unregister.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_unregister_memory(
    NNTI_buffer_t reg_buf);

/**
 * @brief Calculate the number of bytes required to store an encoded NNTI data structure.
 *
 * \param[in]  trans_hdl   A handle to the activated transport.
 * \param[in]  nnti_dt     The NNTI data structure cast to void*.
 * \param[out] packed_len  The number of bytes required to store the encoded data structure.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_dt_sizeof(
    const NNTI_transport_t  trans_hdl,
    void                   *nnti_dt,
    uint64_t               *packed_len);

/**
 * @brief Encode an NNTI data structure into an array of bytes.
 *
 * \param[in]  trans_hdl      A handle to the activated transport.
 * \param[in]  nnti_dt        The NNTI data structure cast to void*.
 * \param[out] packed_buf     A array of bytes to store the encoded data structure.
 * \param[in]  packed_buflen  The length of packed_buf.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_dt_pack(
    const NNTI_transport_t  trans_hdl,
    void                   *nnti_dt,
    char                   *packed_buf,
    const uint64_t          packed_buflen);

/**
 * @brief Decode an array of bytes into an NNTI datatype.
 *
 * \param[in]  trans_hdl      A handle to the activated transport.
 * \param[out] nnti_dt        The NNTI data structure cast to void*.
 * \param[in]  packed_buf     A array of bytes containing the encoded data structure.
 * \param[in]  packed_len     The number of encoded bytes.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_dt_unpack(
    const NNTI_transport_t  trans_hdl,
    void                   *nnti_dt,
    char                   *packed_buf,
    const uint64_t          packed_len);

/**
 * @brief Free a variable size NNTI datatype that was unpacked with NNTI_dt_unpack().
 *
 * \param[in]  trans_hdl      A handle to the activated transport.
 * \param[in]  nnti_dt        The NNTI data structure cast to void*.
 */
NNTI_result_t NNTI_dt_free(
    const NNTI_transport_t  trans_hdl,
    void                   *nnti_dt);

/**
 * @brief Convert an NNTI URL to an NNTI_process_id_t.
 *
 * \param[in]   url  A string that describes this process's location on the network.
 * \param[out]  pid  Compact binary representation of a process's location on the network.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_dt_url_to_pid(
    const char        *url,
    NNTI_process_id_t *pid);

/**
 * @brief Convert an NNTI_process_id_t to an NNTI URL.
 *
 * \param[in]   pid     Compact binary representation of a process's location on the network.
 * \param[out]  url     A string that describes this process's location on the network.
 * \param[in]   maxlen  The maximum length of the URL that can be written into url.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_dt_pid_to_url(
    const NNTI_process_id_t  pid,
    char                    *url,
    const uint64_t           maxlen);

/**
 * @brief Send a message to a peer.
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_send(
    NNTI_work_request_t *wr,
    NNTI_work_id_t      *wid);

/**
 * @brief Transfer data to a peer.
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_put(
    NNTI_work_request_t *wr,
    NNTI_work_id_t      *wid);

/**
 * @brief Transfer data from a peer.
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_get(
    NNTI_work_request_t *wr,
    NNTI_work_id_t      *wid);

/**
 * perform a 64-bit atomic operation with GET semantics
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_atomic_fop(
    NNTI_work_request_t *wr,
    NNTI_work_id_t      *wid);

/**
 * perform a 64-bit compare-and-swap operation
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_atomic_cswap(
    NNTI_work_request_t *wr,
    NNTI_work_id_t      *wid);

/**
 * @brief Attempts to cancel an NNTI operation.
 *
 * \param[in]  wid   A work ID to cancel.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_cancel(
    NNTI_work_id_t wid);


/**
 * @brief Attempts to cancel a list of NNTI operations.
 *
 * \param[in]  wid_list   A list of work IDs to cancel.
 * \param[in]  wid_count  The number of work IDs in wid_list.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_cancelall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count);


/**
 * @brief Sends a signal to interrupt NNTI_wait*().
 *
 * \param[in]  trans_hdl      A handle to the activated transport.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_interrupt(
    const NNTI_transport_t  trans_hdl);


/**
 * @brief Wait for a specific operation (wid) to complete.
 *
 * \param[in]  wid      The operation to wait for.
 * \param[in]  timeout  The amount of time (in milliseconds) to wait.
 * \param[out] status   The details of the completed (or timed out) operation.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_wait(
    NNTI_work_id_t  wid,
    const int64_t   timeout,
    NNTI_status_t  *status);

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
NNTI_result_t NNTI_waitany(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    uint32_t       *which,
    NNTI_status_t  *status);

/**
 * @brief Waits for all the operations (wid_list) in the list to complete.
 *
 * \param[in]  wid_list   The list of operations to wait for.
 * \param[in]  wid_count  The number of operations in wid_list.
 * \param[in]  timeout    The amount of time (in milliseconds) to wait.
 * \param[out] status     The details of the completed (or timed out) operations (one per operation).
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t NNTI_waitall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    NNTI_status_t  *status);

/**
 * @brief Deactivates a specific transport.
 *
 * \param[in] trans_hdl A handle to an activated transport.
 * \return A result code (NNTI_OK or an error)
*/
NNTI_result_t NNTI_fini(
    const NNTI_transport_t trans_hdl);

/** @} */


#ifdef __cplusplus
}
#endif

#endif /* NNTI_H_*/
