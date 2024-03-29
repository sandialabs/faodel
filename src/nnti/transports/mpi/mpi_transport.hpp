// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef MPI_TRANSPORT_HPP_
#define MPI_TRANSPORT_HPP_

#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <mpi.h>

#include <atomic>
#include <deque>
#include <list>
#include <thread>
#include <mutex>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/base/base_transport.hpp"

#include "nnti/nnti_types.h"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_freelist.hpp"
#include "nnti/nnti_wr.hpp"


namespace nnti  {

namespace datatype {
    // forward declaration of friend
    class mpi_buffer;
    class nnti_event_queue;
    class nnti_event_callback;
    class nnti_work_id;
}

namespace core {
    // forward declaration of friend
    class mpi_cmd_msg;

    class mpi_connection;

    class mpi_cmd_buffer;
    class mpi_cmd_op;
}

namespace transports {

class mpi_transport
: public base_transport {

    friend class nnti::core::mpi_connection;

    friend class nnti::core::mpi_cmd_msg;

    friend class nnti::core::mpi_cmd_buffer;
    friend class nnti::core::mpi_cmd_op;

    friend class nnti::datatype::mpi_buffer;

private:
    struct whookie_stats {
        std::atomic<uint64_t> pinned_bytes;
        std::atomic<uint64_t> pinned_buffers;
        std::atomic<uint64_t> unexpected_sends;
        std::atomic<uint64_t> unexpected_recvs;
        std::atomic<uint64_t> dropped_unexpected;
        std::atomic<uint64_t> short_sends;
        std::atomic<uint64_t> short_recvs;
        std::atomic<uint64_t> long_sends;
        std::atomic<uint64_t> long_recvs;
        std::atomic<uint64_t> gets;
        std::atomic<uint64_t> puts;

        whookie_stats()
        {
            pinned_bytes.store(0);
            pinned_buffers.store(0);
            unexpected_sends.store(0);
            unexpected_recvs.store(0);
            dropped_unexpected.store(0);
            short_sends.store(0);
            short_recvs.store(0);
            long_sends.store(0);
            long_recvs.store(0);
            gets.store(0);
            puts.store(0);
        }
    };

    const static int NNTI_MPI_CMD_TAG      = 1;
    const static int NNTI_MPI_GET_DATA_TAG = 2;
    const static int NNTI_MPI_PUT_DATA_TAG = 3;

    const static int poll_min_nsec = 1000;
    const static int poll_max_nsec = 100000;

    MPI_Comm nnti_comm_;
    int      nnti_comm_size_;
    int      nnti_comm_rank_;

    bool                            started_;
    bool                            external_mpi_init_;  // MPI_Init() called before transport::start()

    uint32_t                        cmd_msg_size_;
    uint32_t                        cmd_msg_count_;
    nnti::core::mpi_cmd_buffer     *cmd_buf_;

    int                             interrupt_pipe_[2];

    std::atomic<bool>               terminate_progress_thread_;
    std::thread                     progress_thread_;

    nthread_lock_t new_connection_lock_;
    nnti::core::nnti_connection_map conn_map_;
    nnti::datatype::nnti_buffer_map buffer_map_;

    std::vector<MPI_Request>               outstanding_op_requests_;
    std::vector<nnti::core::mpi_cmd_op *>  outstanding_ops_;
    std::vector<MPI_Request>               outstanding_msg_requests_;
    std::vector<nnti::core::mpi_cmd_msg *> outstanding_msgs_;
    std::mutex                             outstanding_requests_mutex_;

    std::mutex                             mpi_mutex_;

    nnti::datatype::nnti_event_queue       *unexpected_queue_;
    std::deque<nnti::core::mpi_cmd_msg *>   unexpected_msgs_;   //

    uint64_t                                               event_freelist_size_;
    nnti::core::nnti_freelist<NNTI_event_t*>              *event_freelist_;
    uint64_t                                               cmd_op_freelist_size_;
    nnti::core::nnti_freelist<nnti::core::mpi_cmd_op*>    *cmd_op_freelist_;

    struct whookie_stats *stats_;

    NNTI_attrs_t attrs_;

private:
    /**
     * @brief Initialize NNTI to use a specific transport.
     *
     * \param[in]  config    A Configuration object that NNTI should use to configure itself.
     * \return A result code (NNTI_OK or an error)
     *
     */
    mpi_transport(
        faodel::Configuration &config);

public:
    /**
     * @brief Deactivates a specific transport.
     *
     * \return A result code (NNTI_OK or an error)
     */
    ~mpi_transport();

    NNTI_result_t
    start(void) override;

    NNTI_result_t
    stop(void) override;

    /**
     * @brief Indicates if a transport has been initialized.
     *
     * \return A result code (NNTI_OK or an error)
     *
     */
    bool
    initialized(void) override;

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
        const uint64_t  maxlen) override;

    /**
     * @brief Get the process ID of this process.
     *
     * \param[out] pid   the process ID of this process
     * \return A result code (NNTI_OK or an error)
     *
     */
    NNTI_result_t
    pid(NNTI_process_id_t *pid) override;

    /**
     * @brief Get attributes of the transport.
     *
     * \param[out] attrs   the current attributes
     * \return A result code (NNTI_OK or an error)
     *
     */
    NNTI_result_t
    attrs(NNTI_attrs_t *attrs) override;

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
        NNTI_peer_t *peer_hdl) override;

    /**
     * @brief Terminate communication with this peer.
     *
     * \param[in] peer_hdl  A handle to a peer.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    disconnect(
        NNTI_peer_t peer_hdl) override;

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
        NNTI_event_queue_t *eq) override;

    NNTI_result_t
    eq_create(
        uint64_t                             size,
        NNTI_eq_flags_t                      flags,
        nnti::datatype::nnti_event_callback  cb,
        void                                *cb_context,
        NNTI_event_queue_t                  *eq) override;

    /**
     * @brief Destroy an event queue.
     *
     * \param[in] eq  The event queue to destroy.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    eq_destroy(
        NNTI_event_queue_t eq) override;

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
        NNTI_event_t       *event) override;

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
        NNTI_event_t  *result_event) override;

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
        NNTI_event_t  *result_event) override;

    /**
     * @brief Marks a send operation as complete.
     *
     * \param[in] event  The event to mark complete.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    event_complete(
        NNTI_event_t *event) override;

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
        const uint64_t  packed_len) override;

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
        NNTI_buffer_t                       *reg_buf) override;

    /**
     * @brief Disables network operations on the block of memory and frees it.
     *
     * \param[in]  reg_buf The buffer to cleanup.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    free(
        NNTI_buffer_t reg_buf) override;

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
        NNTI_buffer_t                       *reg_buf) override;

    /**
     * @brief Disables network operations on a memory buffer.
     *
     * \param[in]  reg_buf  A handle to a memory buffer to unregister.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    unregister_memory(
        NNTI_buffer_t reg_buf) override;

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
        NNTI_process_id_t *pid) override;

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
        NNTI_peer_t       *peer_hdl) override;

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
        NNTI_work_id_t                    *wid) override;

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
        NNTI_work_id_t                    *wid) override;

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
        NNTI_work_id_t                    *wid) override;

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
        NNTI_work_id_t                    *wid) override;

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
        NNTI_work_id_t                    *wid) override;

    /**
     * @brief Attempts to cancel an NNTI operation.
     *
     * \param[in]  wid   A work ID to cancel.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    cancel(
        NNTI_work_id_t wid) override;


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
        const uint32_t  wid_count) override;


    /**
     * @brief Sends a signal to interrupt NNTI_wait*().
     *
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    interrupt() override;


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
        NNTI_status_t  *status) override;

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
        NNTI_status_t  *status) override;

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
        NNTI_status_t  *status) override;

public:
    static mpi_transport*
    get_instance(
        faodel::Configuration &config);

private:

    void
    add_outstanding_cmd_op(
        std::unique_lock<std::mutex> &req_lock,
        MPI_Request r,
        nnti::core::mpi_cmd_op *op);
    void
    add_outstanding_cmd_op(
        MPI_Request r,
        nnti::core::mpi_cmd_op *op);
    void
    remove_outstanding_cmd_op(
        std::unique_lock<std::mutex> &req_lock,
        int index);
    void
    remove_outstanding_cmd_op(
        int index);
    void
    remove_outstanding_cmd_op(
        nnti::core::mpi_cmd_op *op);
    void
    purge_outstanding_cmd_ops();

    void
    add_outstanding_cmd_msg(
        std::unique_lock<std::mutex> &req_lock,
        MPI_Request r,
        nnti::core::mpi_cmd_msg *msg);
    void
    add_outstanding_cmd_msg(
        MPI_Request r,
        nnti::core::mpi_cmd_msg *msg);
    void
    remove_outstanding_cmd_msg(
        std::unique_lock<std::mutex> &req_lock,
        int index);
    void
    remove_outstanding_cmd_msg(
        int index);
    void
    remove_outstanding_cmd_msg(
        nnti::core::mpi_cmd_msg *msg);
    void
    purge_outstanding_cmd_msgs();

    NNTI_result_t
    setup_interrupt_pipe(void);

    NNTI_result_t
    setup_freelists(void);
    NNTI_result_t
    teardown_freelists(void);

    NNTI_result_t
    setup_command_buffer(void);
    NNTI_result_t
    teardown_command_buffer(void);

    void
    progress(void);
    void
    start_progress_thread(void);
    void
    stop_progress_thread(void);

    struct ibv_device *
    get_ib_device(void);

    void
    connect_cb(
        const std::map<std::string,std::string> &args,
        std::stringstream &results);
    void
    disconnect_cb(
        const std::map<std::string,std::string> &args,
        std::stringstream &results);
    void
    stats_cb(
        const std::map<std::string,std::string> &args,
        std::stringstream &results);
    void
    peers_cb(
        const std::map<std::string,std::string> &args,
        std::stringstream &results);
    std::string
    build_whookie_path(
        const char                  *service);
    std::string
    build_whookie_connect_path(void);
    std::string
    build_whookie_disconnect_path(void);
    void
    register_whookie_cb(void);
    void
    unregister_whookie_cb(void);

    NNTI_result_t
    create_send_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::mpi_cmd_op   **cmd_op);
    NNTI_result_t
    create_get_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::mpi_cmd_op       **get_op);
    NNTI_result_t
    create_put_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::mpi_cmd_op       **put_op);
    NNTI_result_t
    create_fadd_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::mpi_cmd_op       **atomic_op);
    NNTI_result_t
    create_cswap_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::mpi_cmd_op       **atomic_op);

    NNTI_result_t
    execute_cmd_op(
        nnti::datatype::nnti_work_id *work_id,
        nnti::core::mpi_cmd_op       *cmd_op);
    NNTI_result_t
    execute_rdma_op(
        nnti::datatype::nnti_work_id *work_id,
        nnti::core::mpi_cmd_op       *rdma_op);
    NNTI_result_t
    execute_atomic_op(
        nnti::datatype::nnti_work_id *work_id,
        nnti::core::mpi_cmd_op       *atomic_op);

    NNTI_result_t
    complete_send_command(nnti::core::mpi_cmd_msg *cmd_msg);
    NNTI_result_t
    complete_get_command(nnti::core::mpi_cmd_msg *cmd_msg);
    NNTI_result_t
    complete_put_command(nnti::core::mpi_cmd_msg *cmd_msg);
    NNTI_result_t
    complete_fadd_command(nnti::core::mpi_cmd_msg *cmd_msg);
    NNTI_result_t
    complete_cswap_command(nnti::core::mpi_cmd_msg *cmd_msg);

    int
    poll_msg_requests(std::vector<MPI_Request> &reqs, int &index, int &done, MPI_Status &event, nnti::core::mpi_cmd_msg *&cmd_msg);
    NNTI_result_t
    progress_msg_requests(void);
    int
    poll_op_requests(std::vector<MPI_Request> &reqs, int &index, int &done, MPI_Status &event, nnti::core::mpi_cmd_op *&cmd_op);
    NNTI_result_t
    progress_op_requests(void);

    NNTI_event_t *
    create_event(
        nnti::core::mpi_cmd_msg *cmd_msg,
        uint64_t                 offset);
    NNTI_event_t *
    create_event(
        nnti::core::mpi_cmd_msg *cmd_msg);
    NNTI_event_t *
    create_event(
        nnti::core::mpi_cmd_op *cmd_op);

    nnti::datatype::nnti_buffer *
    unpack_buffer(
        char           *packed_buf,
        const uint64_t  packed_len);

    void
    print_wc(
        const struct ibv_wc *wc,
        bool force);
};

} /* namespace transports */
} /* namespace nnti */

#endif /* MPI_TRANSPORT_HPP_*/
