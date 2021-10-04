// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_TRANSPORT_HPP_
#define UGNI_TRANSPORT_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <rdmacred.h>
#ifdef __cplusplus
}
#endif
#include <gni_pub.h>

#include <atomic>
#include <deque>
#include <list>
#include <thread>

#include "faodel-common/Configuration.hh"

#include "nnti/nntiConfig.h"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/base/base_transport.hpp"

#include "nnti/nnti_types.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_freelist.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wr.hpp"


namespace nnti  {

namespace datatype {
    // forward declaration of friend
    class ugni_buffer;
    class nnti_event_queue;
    class nnti_event_callback;
    class nnti_work_id;
}

namespace core {
    // forward declaration of friend
    class ugni_cmd_msg;
    class ugni_cmd_tgt;

    class ugni_connection;

    class ugni_atomic_op;
    class ugni_cmd_buffer;
    class ugni_cmd_op;
    class ugni_mailbox;
    class ugni_rdma_op;
}

namespace transports {

#define NNTI_SMSG_TAG_REQUEST       1
#define NNTI_SMSG_TAG_CREDIT        2
#define NNTI_SMSG_TAG_LONG_GET_ACK  3

// UGNI requires alignment for some RDMA ops
#define NNTI_UGNI_RDMA_ALIGNMENT 4

class ugni_transport
: public base_transport {

    friend class nnti::core::ugni_connection;

    friend class nnti::core::ugni_cmd_msg;
    friend class nnti::core::ugni_cmd_tgt;

    friend class nnti::core::ugni_atomic_op;
    friend class nnti::core::ugni_cmd_buffer;
    friend class nnti::core::ugni_cmd_op;
    friend class nnti::core::ugni_mailbox;
    friend class nnti::core::ugni_rdma_op;

    friend class nnti::datatype::ugni_buffer;

private:
    NNTI_STATS_DATA(
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
        std::atomic<uint64_t> fadds;
        std::atomic<uint64_t> cswaps;

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
            fadds.store(0);
            cswaps.store(0);
        }
    };

    struct whookie_stats *stats_;
    )

    struct drc_info_t {
        drc_info_handle_t drc_info_hdl;
        uint32_t          device_id;
        uint32_t          credential_id;
        uint32_t          cookie1;
        uint8_t           ptag1;
        uint32_t          local_addr;
    };

    struct credit_msg_t {
        uint64_t inst_id;
    };

    nthread_lock_t ugni_lock_;

    NNTI_attrs_t attrs_;

    bool started_;

    uint64_t   apid_;
    drc_info_t drc_info_;

    gni_nic_device_t dev_type_;

    gni_cdm_handle_t cdm_hdl_;
    gni_nic_handle_t nic_hdl_;
    NNTI_instance_id instance_;

    int      listen_sock_;
    char     listen_name_[NNTI_HOSTNAME_LEN];
    uint32_t listen_addr_;  /* in NBO */
    uint16_t listen_port_;  /* in NBO */

    gni_cq_handle_t smsg_ep_cq_hdl_;   /* events are delivered here when a request has been delivered to a remote mailbox */
    gni_cq_handle_t smsg_mem_cq_hdl_;  /* events are delivered here when an unsolicited credit message is received */

    gni_cq_handle_t long_get_ep_cq_hdl_;
    gni_cq_handle_t long_get_mem_cq_hdl_;

    gni_cq_handle_t unexpected_long_get_ep_cq_hdl_;
    gni_cq_handle_t unexpected_long_get_mem_cq_hdl_;
    
    gni_cq_handle_t rdma_ep_cq_hdl_;
    gni_cq_handle_t rdma_mem_cq_hdl_;

    char             interrupt_buf_;
    gni_mem_handle_t interrupt_mem_hdl_;
    gni_cq_handle_t  interrupt_mem_cq_hdl_;
    gni_ep_handle_t  interrupt_ep_hdl_;
    gni_cq_handle_t  interrupt_ep_cq_hdl_;

    uint32_t active_mtu_bytes_;

    uint32_t cmd_msg_size_;
    uint32_t cmd_msg_count_;

    int interrupt_pipe_[2];

    std::atomic<bool> terminate_progress_thread_;
    std::thread       progress_thread_;

    nthread_lock_t new_connection_lock_;
    nnti::core::nnti_connection_map conn_map_;
    nnti::datatype::nnti_buffer_map buffer_map_;

    nnti::core::nnti_connection_vector conn_vector_;
    nnti::core::nnti_op_vector         op_vector_;

    nnti::core::nnti_vector<nnti::core::ugni_cmd_tgt *> msg_vector_;

    nnti::datatype::nnti_event_queue       *unexpected_queue_;
    std::deque<nnti::core::ugni_cmd_tgt *>  unexpected_msgs_;

    uint64_t                                                event_freelist_size_;
    nnti::core::nnti_freelist<NNTI_event_t*>               *event_freelist_;
    uint64_t                                                cmd_op_freelist_size_;
    nnti::core::nnti_freelist<nnti::core::ugni_cmd_op*>    *cmd_op_freelist_;
    uint64_t                                                rdma_op_freelist_size_;
    nnti::core::nnti_freelist<nnti::core::ugni_rdma_op*>   *rdma_op_freelist_;
    uint64_t                                                atomic_op_freelist_size_;
    nnti::core::nnti_freelist<nnti::core::ugni_atomic_op*> *atomic_op_freelist_;
    uint64_t                                                cmd_tgt_freelist_size_;
    nnti::core::nnti_freelist<nnti::core::ugni_cmd_tgt*>   *cmd_tgt_freelist_;

private:
    /**
     * @brief Initialize NNTI to use a specific transport.
     *
     * \return A result code (NNTI_OK or an error)
     *
     */
    ugni_transport(
        faodel::Configuration &config);

public:
    /**
     * @brief Deactivates a specific transport.
     *
     * \return A result code (NNTI_OK or an error)
     */
    ~ugni_transport() override;

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
    static ugni_transport*
    get_instance(
        faodel::Configuration &config);

private:
//    NNTI_result_t
//    setup_command_channel(void);
//    NNTI_result_t
//    setup_rdma_channel(void);
//    NNTI_result_t
//    setup_interrupt_pipe(void);

    NNTI_result_t
    setup_freelists(void);
    NNTI_result_t
    teardown_freelists(void);

    void
    progress(void);
    void
    start_progress_thread(void);
    void
    stop_progress_thread(void);

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
        const char *service);
    std::string
    build_whookie_path(
        nnti::core::nnti_connection *conn,
        const char                  *service);
    std::string
    build_whookie_connect_path(void);
    std::string
    build_whookie_connect_path(
        nnti::core::nnti_connection *conn);
    std::string
    build_whookie_disconnect_path(void);
    std::string
    build_whookie_disconnect_path(
        nnti::core::nnti_connection *conn);
    void
    register_whookie_cb(void);
    void
    unregister_whookie_cb(void);

    NNTI_result_t
    create_send_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::ugni_cmd_op   **cmd_op);
    NNTI_result_t
    create_get_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::ugni_rdma_op  **get_op);
    NNTI_result_t
    create_put_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::ugni_rdma_op  **put_op);
    NNTI_result_t
    create_fadd_op(
        nnti::datatype::nnti_work_id   *work_id,
        nnti::core::ugni_atomic_op **atomic_op);
    NNTI_result_t
    create_cswap_op(
        nnti::datatype::nnti_work_id   *work_id,
        nnti::core::ugni_atomic_op **atomic_op);

    NNTI_result_t
    execute_cmd_op(
        nnti::datatype::nnti_work_id *work_id,
        nnti::core::ugni_cmd_op   *cmd_op);
    NNTI_result_t
    execute_rdma_op(
        nnti::datatype::nnti_work_id *work_id,
        nnti::core::ugni_rdma_op  *rdma_op);
    NNTI_result_t
    execute_atomic_op(
        nnti::datatype::nnti_work_id  *work_id,
        nnti::core::ugni_atomic_op *atomic_op);

    NNTI_result_t
    process_comp_channel_event(
        struct ibv_comp_channel *comp_channel,
        struct ibv_cq           *cq);

    NNTI_result_t
    poll_fds(void);
    NNTI_result_t
    poll_cq(
        struct ibv_cq *cq,
        struct ibv_wc *wc);
    NNTI_result_t
    poll_cmd_cq(void);
    NNTI_result_t
    poll_rdma_cq(void);

    NNTI_event_t *
    create_event(
        nnti::core::ugni_cmd_tgt *cmd_tgt,
        uint64_t                  offset);
    NNTI_event_t *
    create_event(
        nnti::core::ugni_cmd_tgt *cmd_tgt);
    NNTI_event_t *
    create_event(
        nnti::core::ugni_cmd_op *cmd_op);
    NNTI_event_t *
    create_event(
        nnti::core::ugni_rdma_op *rdma_op);
    NNTI_event_t *
    create_event(
        nnti::core::ugni_atomic_op *atomic_op);

    inline gni_return_t get_event(gni_cq_handle_t cq, gni_cq_entry_t *ev)
    {
        gni_return_t gni_rc;
        nthread_lock(&ugni_lock_);
        gni_rc=GNI_CqGetEvent(cq, ev);
        nthread_unlock(&ugni_lock_);
        if ((gni_rc != GNI_RC_SUCCESS) && (gni_rc != GNI_RC_NOT_DONE)) {
            log_error("ugni_transport", "CqGetEvent() on cq(%p) failed: %d", cq, gni_rc);
        }
        return gni_rc;
    }


    nnti::datatype::nnti_buffer *
    unpack_buffer(
        char           *packed_buf,
        const uint64_t  packed_len);

    NNTI_result_t
    get_drc_info(
        drc_info_t *drc_info);
    NNTI_result_t
    free_drc_info(
        drc_info_t *drc_info);
//    void
//    print_wc(
//        const nnti_gni_work_completion_t *wc);
    void
    print_cq_event(
        const gni_cq_entry_t *event,
        const bool            force);
    void
    print_post_desc(
        const gni_post_descriptor_t *post_desc_ptr);
//    void
//    print_gni_conn(
//        nnti_gni_connection_t *c);
};

} /* namespace transports */
} /* namespace nnti */

#endif /* UGNI_TRANSPORT_HPP_*/
