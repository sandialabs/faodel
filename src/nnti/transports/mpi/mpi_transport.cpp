// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include <mpi.h>

#include <fcntl.h>
#include <sys/poll.h>
//#define _POSIX_C_SOURCE 199309
#include <time.h>

#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <vector>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/base/base_transport.hpp"

#include "nnti/nnti_types.h"

#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_eq.hpp"
#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/nnti_url.hpp"
#include "nnti/nnti_util.hpp"
#include "nnti/nnti_logger.hpp"

#include "nnti/transports/mpi/mpi_buffer.hpp"
#include "nnti/transports/mpi/mpi_cmd_msg.hpp"
#include "nnti/transports/mpi/mpi_cmd_op.hpp"
#include "nnti/transports/mpi/mpi_connection.hpp"
#include "nnti/transports/mpi/mpi_peer.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"

#include "webhook/WebHook.hh"
#include "webhook/Server.hh"


namespace nnti  {
namespace transports {

/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * \param[in]  trans_id  The ID of the transport the client wants to use.
 * \param[in]  my_url    A string that describes the transport parameters.
 * \param[out] trans_hdl A handle to the activated transport.
 * \return A result code (NNTI_OK or an error)
 *
 */
mpi_transport::mpi_transport(
    faodel::Configuration &config)
    : base_transport(NNTI_TRANSPORT_MPI,
                     config),
      started_(false),
      external_mpi_init_(true),
      event_freelist_size_(128),
      cmd_op_freelist_size_(128)
{
    faodel::rc_t rc = 0;
    uint64_t uint_value = 0;

    nthread_lock_init(&new_connection_lock_);

    rc = config.GetUInt(&uint_value, "nnti.freelist.size", "128");
    if (rc == 0) {
        event_freelist_size_     = uint_value;
        cmd_op_freelist_size_    = uint_value;
    }
    event_freelist_     = new nnti::core::nnti_freelist<NNTI_event_t*>(event_freelist_size_);
    cmd_op_freelist_    = new nnti::core::nnti_freelist<nnti::core::mpi_cmd_op*>(cmd_op_freelist_size_);

    return;
}

/**
 * @brief Deactivates a specific transport.
 *
 * \return A result code (NNTI_OK or an error)
 */
mpi_transport::~mpi_transport()
{
    nthread_lock_fini(&new_connection_lock_);

    return;
}

NNTI_result_t
mpi_transport::start(void)
{
    NNTI_result_t rc = NNTI_OK;
    int is_initialized=0;

    log_debug("mpi_transport", "enter");

//    config_init(&config);
//    config_get_from_env(&config);

    log_debug("mpi_transport", "initializing MPI");
    MPI_Initialized(&is_initialized);
    if (!is_initialized) {
        external_mpi_init_ = false;
        int provided;
        MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    }
    nnti_comm_ = MPI_COMM_WORLD;
    MPI_Comm_size(nnti_comm_, &nnti_comm_size_);
    MPI_Comm_rank(nnti_comm_, &nnti_comm_rank_);

    faodel::nodeid_t nodeid = webhook::Server::GetNodeID();
    std::string addr = nodeid.GetIP();
    std::string port = nodeid.GetPort();
    url_ = nnti::core::nnti_url(addr, port);
    me_ = nnti::datatype::mpi_peer(this, url_, nnti_comm_rank_);
    log_debug_stream("mpi_transport") << "me_ = " << me_.url().url();

    cmd_msg_size_  = 2048;
    cmd_msg_count_ = 64;
    log_debug("mpi_transport", "cmd_msg_size_(%u) cmd_msg_count_(%u)", cmd_msg_size_, cmd_msg_count_);

    attrs_.mtu                 = cmd_msg_size_;
    attrs_.max_cmd_header_size = nnti::core::mpi_cmd_msg::header_length();
    attrs_.max_eager_size      = attrs_.mtu - attrs_.max_cmd_header_size;
    attrs_.cmd_queue_size      = cmd_msg_count_;
    log_debug("mpi_transport", "attrs_.mtu                =%d", attrs_.mtu);
    log_debug("mpi_transport", "attrs_.max_cmd_header_size=%d", attrs_.max_cmd_header_size);
    log_debug("mpi_transport", "attrs_.max_eager_size     =%d", attrs_.max_eager_size);
    log_debug("mpi_transport", "attrs_.cmd_queue_size     =%d", attrs_.cmd_queue_size);

    rc = setup_freelists();
    if (rc) {
        log_error("mpi_transport", "setup_freelists() failed");
        return NNTI_EIO;
    }
    rc = setup_command_buffer();
    if (rc) {
        log_error("mpi_transport", "setup_command_buffer() failed");
        return NNTI_EIO;
    }

    stats_ = new struct webhook_stats;

    assert(webhook::Server::IsRunning() && "webhook is not running.  Confirm Bootstrap configuration and try again.");

    register_webhook_cb();

    log_debug("mpi_transport", "url_=%s", url_.url().c_str());

    start_progress_thread();

    log_debug("mpi_transport", "MPI Initialized");

    started_ = true;

    log_debug("mpi_transpoprt", "conn_map_ at startup contains:");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    log_debug("mpi_transport", "exit");

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::stop(void)
{
    NNTI_result_t rc=NNTI_OK;;
    nnti::core::nnti_connection_map_iter_t iter;

    log_debug("mpi_transport", "enter");

    started_ = false;

    // purge any remaining connections from the map
    // FIX: this will leak memory and IB resources - do it better
    nthread_lock(&new_connection_lock_);
    for (iter = conn_map_.begin() ; iter != conn_map_.end() ; ) {
        nnti::core::nnti_connection *conn = *iter;
        ++iter;
        conn_map_.remove(conn);
    }
    nthread_unlock(&new_connection_lock_);

    unregister_webhook_cb();

    stop_progress_thread();

    purge_outstanding_cmd_ops();
    purge_outstanding_cmd_msgs();

    teardown_command_buffer();
    teardown_freelists();

    if (!external_mpi_init_) {
        MPI_Finalize();
    }

cleanup:
    log_debug("mpi_transport", "exit");

    return rc;
}

/**
 * @brief Indicates if a transport has been initialized.
 *
 * \param[in]  trans_id  The ID of the transport to test.
 * \param[out] is_init   1 if the transport is initialized, 0 otherwise.
 * \return A result code (NNTI_OK or an error)
 *
 */
bool
mpi_transport::initialized(void)
{
    return started_ ? true : false;
}

/**
 * @brief Return the URL field of this transport.
 *
 * \param[out] url       A string that describes this process in a transport specific way.
 * \param[in]  maxlen    The length of the 'url' string parameter.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::get_url(
    char           *url,
    const uint64_t  maxlen)
{
    strncpy(url, me_.url().url().c_str(), maxlen);
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
mpi_transport::pid(NNTI_process_id_t *pid)
{
    *pid = me_.pid();
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
mpi_transport::attrs(NNTI_attrs_t *attrs)
{
    *attrs = attrs_;

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
mpi_transport::connect(
    const char  *url,
    const int    timeout,
    NNTI_peer_t *peer_hdl)
{
    nnti::core::nnti_url        peer_url(url);
    nnti::datatype::nnti_peer  *peer = new nnti::datatype::mpi_peer(this, peer_url, -1);
    nnti::core::mpi_connection *conn;

    nthread_lock(&new_connection_lock_);

    log_debug("mpi_transport", "In connect(), before conn_map_.insert():");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    // look for an existing connection to reuse
    log_debug("mpi_transport", "Looking for connection with pid=%016lx", peer->pid());
    conn = (nnti::core::mpi_connection*)conn_map_.get(peer->pid());
    if (conn != nullptr) {
        log_debug("mpi_transport", "Found connection with pid=%016lx", peer->pid());
        // reuse an existing connection
        *peer_hdl = (NNTI_peer_t)conn->peer();
        nthread_unlock(&new_connection_lock_);
        return NNTI_OK;
    }
    log_debug("mpi_transport", "Couldn't find connection with pid=%016lx", peer->pid());

    conn = new nnti::core::mpi_connection(this);

    peer->conn(conn);
    conn->peer(peer);

    conn_map_.insert(conn);

    log_debug("mpi_transport", "In connect(), after conn_map_.insert():");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    nthread_unlock(&new_connection_lock_);

    std::string reply;
    std::string wh_path = build_webhook_connect_path();
    int wh_rc = 0;
    int retries = 5;
    wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
    while (wh_rc != 0 && --retries) {
        sleep(1);
        wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
        log_debug("mpi_transport", "retrieveData() rc=%d", wh_rc);
    }
    if (wh_rc != 0) {
        return(NNTI_ETIMEDOUT);
    }

    log_debug("mpi_transport", "connect - reply=%s", reply.c_str());

    conn->peer_params(reply);

    log_debug("mpi_transport", "After connect() conn_map_ contains:");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    *peer_hdl = (NNTI_peer_t)conn->peer();

    return NNTI_OK;
}

/**
 * @brief Terminate communication with this peer.
 *
 * \param[in] peer_hdl  A handle to a peer.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::disconnect(
    NNTI_peer_t peer_hdl)
{
    nnti::datatype::nnti_peer   *peer     = (nnti::datatype::nnti_peer *)peer_hdl;
    nnti::core::nnti_url        &peer_url = peer->url();
    std::string                  reply;
    nnti::core::nnti_connection *conn     = peer->conn();

    log_debug("mpi_transport", "disconnecting from %s", peer_url.url().c_str());

    nthread_lock(&new_connection_lock_);

    conn = (nnti::core::mpi_connection*)conn_map_.get(peer->pid());
    if (conn == nullptr) {
        log_debug("mpi_transport", "disconnect couldn't find connection to %s. Already disconnected?", peer_url.url().c_str());
        nthread_unlock(&new_connection_lock_);
        return NNTI_EINVAL;
    }

    conn_map_.remove(conn);

    nthread_unlock(&new_connection_lock_);

    if (*peer != me_) {
        std::string wh_path = build_webhook_disconnect_path();
        int wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
        if (wh_rc != 0) {
            return(NNTI_ETIMEDOUT);
        }
    }

    log_debug("mpi_transport", "disconnect from %s (pid=%x) succeeded", peer->url(), peer->pid());

    delete conn;
    delete peer;

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
mpi_transport::eq_create(
    uint64_t            size,
    NNTI_eq_flags_t     flags,
    NNTI_event_queue_t *eq)
{
    nnti::datatype::nnti_event_queue *new_eq = new nnti::datatype::nnti_event_queue(true, size, this);

    if (flags & NNTI_EQF_UNEXPECTED) {
        unexpected_queue_ = new_eq;
    }

    *eq = (NNTI_event_queue_t)new_eq;

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::eq_create(
    uint64_t                             size,
    NNTI_eq_flags_t                      flags,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    NNTI_event_queue_t                  *eq)
{
    nnti::datatype::nnti_event_queue *new_eq = new nnti::datatype::nnti_event_queue(true, size, cb, cb_context, this);

    if (flags & NNTI_EQF_UNEXPECTED) {
        unexpected_queue_ = new_eq;
    }

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
mpi_transport::eq_destroy(
    NNTI_event_queue_t eq)
{
    if (unexpected_queue_ == (nnti::datatype::nnti_event_queue *)eq) {
        unexpected_queue_ = nullptr;
    }
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
mpi_transport::eq_wait(
    NNTI_event_queue_t *eq_list,
    const uint32_t      eq_count,
    const int           timeout,
    uint32_t           *which,
    NNTI_event_t       *event)
{
    bool rc;
    int poll_rc;
    NNTI_result_t nnti_rc;

    std::vector<struct pollfd> poll_fds(eq_count);

    NNTI_event_t *e;

    log_debug("eq_wait", "enter");

    for (int i=0;i<eq_count;i++) {
        nnti::datatype::nnti_event_queue *eq = nnti::datatype::nnti_event_queue::to_obj(eq_list[i]);
        rc = eq->pop(e);
        if (rc) {
            uint32_t dummy=0;
            ssize_t bytes_read=read(eq->read_fd(), &dummy, 4);

            *which = i;
            *event = *e;
            event_freelist_->push(e);
            nnti_rc = NNTI_OK;
            goto cleanup;
        }
    }

    for (int i=0;i<eq_count;i++) {
        nnti::datatype::nnti_event_queue *eq = nnti::datatype::nnti_event_queue::to_obj(eq_list[i]);
        poll_fds[i].fd      = eq->read_fd();
        poll_fds[i].events  = POLLIN;
        poll_fds[i].revents = 0;
    }
    log_debug("eq_wait", "polling with timeout==%d", timeout);

    // Test for errno==EINTR to deal with timing interrupts from HPCToolkit
    do {
        poll_rc = poll(&poll_fds[0], poll_fds.size(), timeout);
    } while ((poll_rc < 0) && (errno == EINTR));

    if (poll_rc == 0) {
        log_debug("eq_wait", "poll() timed out: poll_rc=%d", poll_rc);
        nnti_rc = NNTI_ETIMEDOUT;
        event->result = NNTI_ETIMEDOUT;
        goto cleanup;
    } else if (poll_rc < 0) {
        if (errno == EINTR) {
            log_error("eq_wait", "poll() interrupted by signal: poll_rc=%d (%s)", poll_rc, strerror(errno));
            nnti_rc = NNTI_EINTR;
            event->result = NNTI_EINTR;
        } else if (errno == ENOMEM) {
            log_error("eq_wait", "poll() out of memory: poll_rc=%d (%s)", poll_rc, strerror(errno));
            nnti_rc = NNTI_ENOMEM;
            event->result = NNTI_ENOMEM;
        } else {
            log_error("eq_wait", "poll() invalid args: poll_rc=%d (%s)", poll_rc, strerror(errno));
            nnti_rc = NNTI_EINVAL;
            event->result = NNTI_EINVAL;
        }
        goto cleanup;
    } else {
        log_debug("eq_wait", "polled on %d file descriptor(s).  events occurred on %d file descriptor(s).", poll_fds.size(), poll_rc);
        for (int i=0;i<eq_count;i++) {
            log_debug("eq_wait", "poll success: poll_rc=%d ; poll_fds[%d].revents=%d",
                      poll_rc, i, poll_fds[i].revents);
        }
        for (int i=0;i<eq_count;i++) {
            if (poll_fds[i].revents == POLLIN) {
                log_debug("eq_wait", "poll() events on eq[%d]", i);
                ssize_t bytes_read=0;
                uint32_t dummy=0;
                bytes_read=read(poll_fds[i].fd, &dummy, 4);
                if (dummy != 0xAAAAAAAA) {
                    log_warn("eq_wait", "notification byte is %X, should be 0xAAAAAAAA", dummy);
                }
                log_debug("eq_wait", "bytes_read==%lu", (uint64_t)bytes_read);

                nnti::datatype::nnti_event_queue *eq = nnti::datatype::nnti_event_queue::to_obj(eq_list[i]);
                rc = eq->pop(e);
                if (rc) {
                    *which = i;
                    *event = *e;
                    event_freelist_->push(e);
                    nnti_rc = NNTI_OK;
                    goto cleanup;
                }
            }
        }
    }


cleanup:
    log_debug_stream("mpi_transport") << event;
    log_debug("eq_wait", "exit");

    return nnti_rc;
}

/**
 * @brief Retrieves the next message from the unexpected list.
 *
 * \param[in]  dst_hdl        Buffer where the message is delivered.
 * \param[in]  dst_offset     Offset into dst_hdl where the message is delivered.
 * \param[out] reseult_event  Event describing the message delivered to dst_hdl.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::next_unexpected(
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event)
{
    NNTI_result_t rc = NNTI_OK;
    int mpi_rc;
    uint64_t actual_offset;
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)dst_hdl;

    log_debug("next_unexpected", "enter");

    if (unexpected_msgs_.size() == 0) {
        log_debug("mpi_transport", "next_unexpected - unexpected_msgs_ list is empty");
        return NNTI_ENOENT;
    }

    nnti::core::mpi_cmd_msg *unexpected_msg = unexpected_msgs_.front();
    unexpected_msgs_.pop_front();

    if (unexpected_msg->eager()) {
        rc = b->copy_in(dst_offset, unexpected_msg->eager_payload(), unexpected_msg->payload_length(), &actual_offset);
        if (rc != NNTI_OK) {
            log_error("next_unexpected", "copy_in() failed (rc=%d)", rc);
        }
        NNTI_FAST_STAT(stats_->short_recvs++;)
    } else {
        nnti::datatype::mpi_buffer *initiator_buffer = unexpected_msg->initiator_buffer();
        nnti::datatype::mpi_peer   *peer             = unexpected_msg->initiator_peer();

        MPI_Request req;
        MPI_Status  status;

        log_debug("mpi_transport", "unexpected long send Irecv()");

        std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
        mpi_rc = MPI_Irecv((char*)b->payload() + dst_offset,
                           unexpected_msg->payload_length(),
                           MPI_BYTE,
                           peer->rank(),
                           initiator_buffer->cmd_tag(),
                           MPI_COMM_WORLD,
                           &req);
        mpi_rc = MPI_Wait(&req, &status);
        mpi_lock.unlock();

        log_debug("mpi_transport", "unexpected long send Wait() complete");

        NNTI_FAST_STAT(stats_->long_recvs++;)
    }

    unexpected_msg->post_recv();
    add_outstanding_cmd_msg(unexpected_msg->cmd_request(), unexpected_msg);
    log_debug("mpi_transport", "reposting unexpected_msg (index=%d)", unexpected_msg->index());

    result_event->trans_hdl  = nnti::transports::transport::to_hdl(this);
    result_event->result     = NNTI_OK;
    result_event->op         = NNTI_OP_SEND;
    result_event->peer       = nnti::datatype::nnti_peer::to_hdl(unexpected_msg->initiator_peer());
    result_event->length     = unexpected_msg->payload_length();
    result_event->type       = NNTI_EVENT_SEND;
    result_event->start      = b->payload();
    result_event->offset     = actual_offset;
    result_event->context    = 0;

    log_debug("mpi_transport", "result_event->peer = %p", result_event->peer);

    log_debug("next_unexpected", "exit");

    return rc;
}

/**
 * @brief Retrieves a specific message from the unexpected list.
 *
 * \param[in]  unexpect_event  Event describing the message to retrieve.
 * \param[in]  dst_hdl         Buffer where the message is delivered.
 * \param[in]  dst_offset      Offset into dst_hdl where the message is delivered.
 * \param[out] reseult_event   Event describing the message delivered to dst_hdl.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::get_unexpected(
    NNTI_event_t  *unexpected_event,
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event)
{
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)dst_hdl;

    return NNTI_OK;
}

/**
 * @brief Marks a send operation as complete.
 *
 * \param[in] event  The event to mark complete.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::event_complete(
    NNTI_event_t *event)
{
    nnti::datatype::nnti_buffer *b = nullptr;

    b = buffer_map_.get((char*)event->start);
    b->event_complete(event);

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
mpi_transport::dt_unpack(
    void           *nnti_dt,
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_result_t                rc = NNTI_OK;
    nnti::datatype::mpi_buffer  *b  = nullptr;
    nnti::datatype::nnti_peer   *p  = nullptr;

    switch (*(NNTI_datatype_t*)packed_buf) {
        case NNTI_dt_buffer:
            log_debug("base_transport", "dt is a buffer");
            b = new nnti::datatype::mpi_buffer(this, packed_buf, packed_len);
            *(NNTI_buffer_t*)nnti_dt = nnti::datatype::nnti_buffer::to_hdl(b);
            break;
        case NNTI_dt_peer:
            log_debug("base_transport", "dt is a peer");
            p = new nnti::datatype::nnti_peer(this, packed_buf, packed_len);
            *(NNTI_peer_t*)nnti_dt = nnti::datatype::nnti_peer::to_hdl(p);
            break;
        default:
            // unsupported datatype
            rc = NNTI_EINVAL;
            break;
    }

    return(rc);
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
mpi_transport::alloc(
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    char                               **reg_ptr,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::mpi_buffer(this,
                                                                      size,
                                                                      flags,
                                                                      eq,
                                                                      cb,
                                                                      cb_context);

    buffer_map_.insert(b);

    stats_->pinned_buffers++;
    stats_->pinned_bytes  += b->size();

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
mpi_transport::free(
    NNTI_buffer_t reg_buf)
{
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;

    buffer_map_.remove(b);

    stats_->pinned_buffers--;
    stats_->pinned_bytes  -= b->size();

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
mpi_transport::register_memory(
    char                                *buffer,
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::mpi_buffer(this,
                                                                      buffer,
                                                                      size,
                                                                      flags,
                                                                      eq,
                                                                      cb,
                                                                      cb_context);

    buffer_map_.insert(b);

    stats_->pinned_buffers++;
    stats_->pinned_bytes  += b->size();

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
mpi_transport::unregister_memory(
    NNTI_buffer_t reg_buf)
{
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;

    buffer_map_.remove(b);

    stats_->pinned_buffers--;
    stats_->pinned_bytes  -= b->size();

    delete b;

    return NNTI_OK;
}

/**
 * @brief Convert an NNTI peer to an NNTI_process_id_t.
 *
 * \param[in]   peer  A handle to a peer that can be used for network operations.
 * \param[out]  pid   Compact binary representation of a process's location on the network.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::dt_peer_to_pid(
    NNTI_peer_t        peer_hdl,
    NNTI_process_id_t *pid)
{
    nnti::datatype::nnti_peer *peer = (nnti::datatype::nnti_peer *)peer_hdl;
    *pid = peer->pid();
    return NNTI_OK;
}

/**
 * @brief Convert an NNTI_process_id_t to an NNTI peer.
 *
 * \param[in]   pid   Compact binary representation of a process's location on the network.
 * \param[out]  peer  A handle to a peer that can be used for network operations.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::dt_pid_to_peer(
    NNTI_process_id_t  pid,
    NNTI_peer_t       *peer_hdl)
{
    nnti::core::nnti_connection *conn = conn_map_.get(pid);
    *peer_hdl = (NNTI_peer_t)conn->peer();
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
mpi_transport::send(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::mpi_cmd_op       *cmd_op  = nullptr;

    log_debug("mpi_transport", "send - wr.local_offset=%lu", wr->local_offset());

    rc = create_send_op(work_id, &cmd_op);
    rc = execute_cmd_op(work_id, cmd_op);

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
mpi_transport::put(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::mpi_cmd_op       *put_op  = nullptr;

    rc = create_put_op(work_id, &put_op);
    rc = execute_rdma_op(work_id, put_op);

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
mpi_transport::get(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::mpi_cmd_op       *get_op  = nullptr;

    rc = create_get_op(work_id, &get_op);
    rc = execute_rdma_op(work_id, get_op);

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
mpi_transport::atomic_fop(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::mpi_cmd_op       *atomic_op = nullptr;

    rc = create_fadd_op(work_id, &atomic_op);
    rc = execute_atomic_op(work_id, atomic_op);

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
mpi_transport::atomic_cswap(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::mpi_cmd_op       *atomic_op = nullptr;

    rc = create_cswap_op(work_id, &atomic_op);
    rc = execute_atomic_op(work_id, atomic_op);

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
mpi_transport::cancel(
    NNTI_work_id_t wid)
{
    nnti::datatype::nnti_work_id *work_id = (nnti::datatype::nnti_work_id *)wid;

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
mpi_transport::cancelall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count)
{
    return NNTI_OK;
}


/**
 * @brief Sends a signal to interrupt NNTI_wait*().
 *
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
mpi_transport::interrupt()
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
mpi_transport::wait(
    NNTI_work_id_t  wid,
    const int64_t   timeout,
    NNTI_status_t  *status)
{
    nnti::datatype::nnti_work_id *work_id = (nnti::datatype::nnti_work_id *)wid;

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
mpi_transport::waitany(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    uint32_t       *which,
    NNTI_status_t  *status)
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
mpi_transport::waitall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    NNTI_status_t  *status)
{
    return NNTI_OK;
}

/*************************************************************/

mpi_transport *
mpi_transport::get_instance(
    faodel::Configuration &config)
{
    static mpi_transport *instance = new mpi_transport(config);
    return instance;
}

/*************************************************************
 * Accessors for data members specific to this interconnect.
 *************************************************************/

NNTI_result_t
mpi_transport::setup_interrupt_pipe(void)
{
    int rc=0;
    int flags;

    rc=pipe(interrupt_pipe_);
    if (rc < 0) {
        log_error("mpi_transport", "pipe() failed: %s", strerror(errno));
        return NNTI_EIO;
    }

    /* use non-blocking IO on the read side of the interrupt pipe */
    flags = fcntl(interrupt_pipe_[0], F_GETFL);
    if (flags < 0) {
        log_error("mpi_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
        return NNTI_EIO;
    }
    if (fcntl(interrupt_pipe_[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("mpi_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
        return NNTI_EIO;
    }

    /* use non-blocking IO on the write side of the interrupt pipe */
    flags = fcntl(interrupt_pipe_[1], F_GETFL);
    if (flags < 0) {
        log_error("mpi_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
        return NNTI_EIO;
    }
    if (fcntl(interrupt_pipe_[1], F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("mpi_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
        return NNTI_EIO;
    }

    return(NNTI_OK);
}

NNTI_result_t
mpi_transport::setup_freelists(void)
{
    for (uint64_t i=0;i<cmd_op_freelist_size_;i++) {
        nnti::core::mpi_cmd_op *op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_);
        cmd_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<event_freelist_size_;i++) {
        NNTI_event_t *e = new NNTI_event_t;
        event_freelist_->push(e);
    }

    return(NNTI_OK);
}
NNTI_result_t
mpi_transport::teardown_freelists(void)
{
    while(!event_freelist_->empty()) {
        NNTI_event_t *e = nullptr;
        if (event_freelist_->pop(e)) {
            delete e;
        }
    }

    while(!cmd_op_freelist_->empty()) {
        nnti::core::mpi_cmd_op *op = nullptr;
        if (cmd_op_freelist_->pop(op)) {
            if (op->wid()) {
                delete op->wid();
            }
            delete op;
        }
    }

    return(NNTI_OK);
}

NNTI_result_t
mpi_transport::setup_command_buffer(void)
{
    log_debug("mpi_transport", "setup_command_buffer: enter");

    cmd_buf_ = new nnti::core::mpi_cmd_buffer(this, cmd_msg_size_, cmd_msg_count_);

    for (std::vector<nnti::core::mpi_cmd_msg*>::iterator iter = cmd_buf_->begin() ; iter != cmd_buf_->end() ; ++iter) {
        add_outstanding_cmd_msg((*iter)->cmd_request(), *iter);
    }

    log_debug("mpi_transport", "setup_command_buffer: exit (cmd_buf_=%p)", cmd_buf_);

    return(NNTI_OK);
}
NNTI_result_t
mpi_transport::teardown_command_buffer(void)
{
    log_debug("mpi_transport", "teardown_command_buffer: enter");

    delete cmd_buf_;

    log_debug("mpi_transport", "teardown_command_buffer: exit");

    return(NNTI_OK);
}

void
mpi_transport::progress(void)
{
    NNTI_result_t msg_rc, op_rc;
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = poll_min_nsec;

    while (terminate_progress_thread_.load() == false) {
        log_debug("mpi_transport::progress", "this is the progress thread");

        msg_rc = progress_msg_requests();
        op_rc = progress_op_requests();

        if (msg_rc == NNTI_OK || op_rc == NNTI_OK) {
            ts.tv_nsec = poll_min_nsec;
        } else {
            log_debug("mpi_transport", "sleep(%d) after poll_*_requests()", ts.tv_nsec);
            nanosleep(&ts, NULL);
            ts.tv_nsec *= 2;
            ts.tv_nsec = (ts.tv_nsec > poll_max_nsec) ? poll_max_nsec : ts.tv_nsec;
        }
    }

    log_debug("progress", "progress() is finished");

    return;
}
void
mpi_transport::start_progress_thread(void)
{
    terminate_progress_thread_ = false;
    progress_thread_ = std::thread(&nnti::transports::mpi_transport::progress, this);
}
void
mpi_transport::stop_progress_thread(void)
{
    terminate_progress_thread_ = true;
    progress_thread_.join();
}

void
mpi_transport::connect_cb(
    const std::map<std::string,std::string> &args,
    std::stringstream                       &results)
{
    nnti::core::mpi_connection *conn;

    log_debug("mpi_transport", "inbound connection from %s", std::string(args.at("hostname")+":"+args.at("port")).c_str());

    nthread_lock(&new_connection_lock_);

    log_debug("mpi_transport", "In connect_cb(), before conn_map_.insert():");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    nnti::core::nnti_url peer_url = nnti::core::nnti_url(args.at("hostname"), args.at("port"));

    log_debug("mpi_transport", "Looking for connection with pid=%016lx", peer_url.pid());
    conn = (nnti::core::mpi_connection*)conn_map_.get(peer_url.pid());
    if (conn != nullptr) {
        log_debug("mpi_transport", "Found connection with pid=%016lx", peer_url.pid());
    } else {
        log_debug("mpi_transport", "Couldn't find connection with pid=%016lx", peer_url.pid());

        conn = new nnti::core::mpi_connection(this, args);
        conn_map_.insert(conn);
    }

    log_debug("mpi_transport", "In connect_cb(), after conn_map_.insert():");
    for (auto it = conn_map_.begin() ; it != conn_map_.end() ; ++it) {
        log_debug("mpi_transpoprt", "conn to peer=%p pid=%016lx", (*it)->peer(), (*it)->peer_pid());
    }

    nthread_unlock(&new_connection_lock_);

    results << "hostname=" << url_.hostname() << std::endl;
    results << "addr="     << url_.addr()     << std::endl;
    results << "port="     << url_.port()     << std::endl;
    results << "rank="     << nnti_comm_rank_ << std::endl;

    log_debug("mpi_transport", "connect_cb - results=%s", results.str().c_str());
}

void
mpi_transport::disconnect_cb(
    const std::map<std::string,std::string> &args,
    std::stringstream                       &results)
{
    nnti::core::nnti_connection *conn = nullptr;
    nnti::core::nnti_url         peer_url(args.at("hostname"), args.at("port"));

    nthread_lock(&new_connection_lock_);

    log_debug("mpi_transport", "%s is disconnecting", peer_url.url().c_str());
    conn = conn_map_.get(peer_url.pid());
    log_debug("mpi_transport", "connection map says %s => conn(%p)", peer_url.url().c_str(), conn);

    if (conn != nullptr) {
        conn_map_.remove(conn);
        delete conn;
    }

    nthread_unlock(&new_connection_lock_);

    log_debug("mpi_transport", "disconnect_cb - results=%s", results.str().c_str());
}

void
mpi_transport::stats_cb(
    const std::map<std::string,std::string> &args,
    std::stringstream                       &results)
{
    html::mkHeader(results, "Transfer Statistics");
    html::mkText(results,"Transfer Statistics",1);

    std::vector<std::string> stats;
    stats.push_back("pinned_bytes     = " + std::to_string(stats_->pinned_bytes.load()));
    stats.push_back("pinned_buffers   = " + std::to_string(stats_->pinned_buffers.load()));
    stats.push_back("unexpected_sends = " + std::to_string(stats_->unexpected_sends.load()));
    stats.push_back("unexpected_recvs = " + std::to_string(stats_->unexpected_recvs.load()));
    stats.push_back("short_sends      = " + std::to_string(stats_->short_sends.load()));
    stats.push_back("short_recvs      = " + std::to_string(stats_->short_recvs.load()));
    stats.push_back("long_sends       = " + std::to_string(stats_->long_sends.load()));
    stats.push_back("long_recvs       = " + std::to_string(stats_->long_recvs.load()));
    stats.push_back("gets             = " + std::to_string(stats_->gets.load()));
    stats.push_back("puts             = " + std::to_string(stats_->puts.load()));
    html::mkList(results, stats);

    html::mkFooter(results);
}

void
mpi_transport::peers_cb(
    const std::map<std::string,std::string> &args,
    std::stringstream                       &results)
{
    html::mkHeader(results, "Connected Peers");
    html::mkText(results,"Connected Peers",1);

    std::vector<std::string> links;
    nnti::core::nnti_connection_map_iter_t iter;
    for (iter = conn_map_.begin() ; iter != conn_map_.end() ; iter++) {
        std::string p((*iter)->peer()->url().url());
        links.push_back(html::mkLink(p, p));
    }
    html::mkList(results, links);

    html::mkFooter(results);
}

std::string
mpi_transport::build_webhook_path(
    const char *service)
{
    std::stringstream wh_url;

    wh_url << "/nnti/mpi/" << service;
    wh_url << "&hostname=" << url_.hostname();
    wh_url << "&addr="     << url_.addr();
    wh_url << "&port="     << url_.port();
    wh_url << "&rank="     << nnti_comm_rank_;

    return wh_url.str();
}
std::string
mpi_transport::build_webhook_connect_path(void)
{
    return build_webhook_path("connect");
}
std::string
mpi_transport::build_webhook_disconnect_path(void)
{
    return build_webhook_path("disconnect");
}

void
mpi_transport::register_webhook_cb(void)
{
    webhook::Server::registerHook("/nnti/mpi/connect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        connect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/mpi/disconnect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        disconnect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/mpi/stats", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        stats_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/mpi/peers", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        peers_cb(args, results);
    });
}

void
mpi_transport::unregister_webhook_cb(void)
{
    webhook::Server::deregisterHook("/nnti/mpi/connect");
    webhook::Server::deregisterHook("/nnti/mpi/disconnect");
    webhook::Server::deregisterHook("/nnti/mpi/stats");
    webhook::Server::deregisterHook("/nnti/mpi/peers");
}

void
mpi_transport::add_outstanding_cmd_op(
    std::unique_lock<std::mutex> &req_lock,
    MPI_Request r,
    nnti::core::mpi_cmd_op *op)
{
    outstanding_op_requests_.push_back(r);
    outstanding_ops_.push_back(op);
    op->index(outstanding_ops_.size()-1);
    log_debug("mpi_transport", "added at index %d", op->index());
}
void
mpi_transport::add_outstanding_cmd_op(
    MPI_Request r,
    nnti::core::mpi_cmd_op *op)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    add_outstanding_cmd_op(req_lock, r, op);
    req_lock.unlock();
}
void
mpi_transport::remove_outstanding_cmd_op(
    std::unique_lock<std::mutex> &req_lock,
    int index)
{
    outstanding_op_requests_[index] = MPI_REQUEST_NULL;
    outstanding_ops_[index]         = nullptr;

    log_debug("mpi_transport", "removed at index %d", index);

    int current_size = outstanding_op_requests_.size();
    if (outstanding_op_requests_.size() > cmd_msg_count_) {
        outstanding_op_requests_.erase( std::remove( outstanding_op_requests_.begin(), outstanding_op_requests_.end(), MPI_REQUEST_NULL ), outstanding_op_requests_.end() );
        outstanding_ops_.erase( std::remove( outstanding_ops_.begin(), outstanding_ops_.end(), nullptr ), outstanding_ops_.end() );

        int new_size = outstanding_op_requests_.size();
        log_debug("mpi_transport", "removing %d elements", current_size-new_size);

        outstanding_ops_.resize(new_size);
        outstanding_op_requests_.resize(new_size);

        // reindex
        int i=0;
        auto iter = outstanding_ops_.begin();
        while (iter != outstanding_ops_.end()) {
            (*iter)->index(i);
            ++iter;
            ++i;
        }
    }
}
void
mpi_transport::remove_outstanding_cmd_op(
    int index)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    remove_outstanding_cmd_op(req_lock, index);
    req_lock.unlock();
}
void
mpi_transport::remove_outstanding_cmd_op(
    nnti::core::mpi_cmd_op *op)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    remove_outstanding_cmd_op(req_lock, op->index());
    req_lock.unlock();
}
void
mpi_transport::purge_outstanding_cmd_ops()
{
    outstanding_op_requests_.clear();
    outstanding_ops_.clear();

    log_debug("mpi_transport", "cleared outstanding ops vector");
}

void
mpi_transport::add_outstanding_cmd_msg(
    std::unique_lock<std::mutex> &req_lock,
    MPI_Request r,
    nnti::core::mpi_cmd_msg *msg)
{
    outstanding_msg_requests_.push_back(r);
    outstanding_msgs_.push_back(msg);
    msg->index(outstanding_msgs_.size()-1);
    log_debug("mpi_transport", "added at index %d", msg->index());
}
void
mpi_transport::add_outstanding_cmd_msg(
    MPI_Request r,
    nnti::core::mpi_cmd_msg *msg)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    add_outstanding_cmd_msg(req_lock, r, msg);
    req_lock.unlock();
}

void
mpi_transport::remove_outstanding_cmd_msg(
    std::unique_lock<std::mutex> &req_lock,
    int index)
{
    outstanding_msg_requests_[index] = MPI_REQUEST_NULL;
    outstanding_msgs_[index]         = nullptr;

    log_debug("mpi_transport", "removed at index %d", index);

    int current_size = outstanding_msg_requests_.size();
    if (outstanding_msg_requests_.size() > cmd_msg_count_*2) {
        outstanding_msg_requests_.erase( std::remove( outstanding_msg_requests_.begin(), outstanding_msg_requests_.end(), MPI_REQUEST_NULL ), outstanding_msg_requests_.end() );
        outstanding_msgs_.erase( std::remove( outstanding_msgs_.begin(), outstanding_msgs_.end(), nullptr ), outstanding_msgs_.end() );

        int new_size = outstanding_msg_requests_.size();
        log_debug("mpi_transport", "removing %d elements", current_size-new_size);

        outstanding_msgs_.resize(new_size);
        outstanding_msg_requests_.resize(new_size);

        // reindex
        int i=0;
        auto iter = outstanding_msgs_.begin();
        while (iter != outstanding_msgs_.end()) {
            (*iter)->index(i);
            ++iter;
            ++i;
        }
    }
}
void
mpi_transport::remove_outstanding_cmd_msg(
    int index)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    remove_outstanding_cmd_msg(req_lock, index);
    req_lock.unlock();
}
void
mpi_transport::remove_outstanding_cmd_msg(
    nnti::core::mpi_cmd_msg *msg)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_);
    remove_outstanding_cmd_msg(req_lock, msg->index());
    req_lock.unlock();
}
void
mpi_transport::purge_outstanding_cmd_msgs()
{
    outstanding_msg_requests_.clear();
    outstanding_msgs_.clear();

    log_debug("mpi_transport", "cleared outstanding msgs vector");
}

NNTI_result_t
mpi_transport::create_send_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::mpi_cmd_op   **cmd_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("mpi_transport", "create_send_op() - enter");

    if (work_id->wr().flags() & NNTI_OF_ZERO_COPY) {
        *cmd_op = new nnti::core::mpi_cmd_op(this, work_id);
    } else {
        if (cmd_op_freelist_->pop(*cmd_op)) {
            (*cmd_op)->set(work_id);
        } else {
            *cmd_op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_, work_id);
        }
    }

    log_debug("mpi_transport", "create_send_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::execute_cmd_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::mpi_cmd_op   *cmd_op)
{
    NNTI_result_t rc = NNTI_OK;
    int mpi_rc=0;

    log_debug("mpi_transport", "execute_cmd_op() - enter");

    log_debug("mpi_transport", "looking up connection for peer pid=%016lX", work_id->wr().peer_pid());

    nnti::datatype::mpi_peer   *peer = (nnti::datatype::mpi_peer *)work_id->wr().peer();
    nnti::core::mpi_connection *conn = (nnti::core::mpi_connection *)peer->conn();

    nnti::datatype::mpi_buffer *local_buffer  = (nnti::datatype::mpi_buffer *)work_id->wr().local_hdl();
    uint64_t                    local_offset  = work_id->wr().local_offset();

    if (!cmd_op->eager()) {
        log_debug("mpi_transport",
                  "posting long send Issend(%s) (payload=%p, local_offset=%lu, length=%lu, peer=%d, cmd_tag=%d",
                  cmd_op->toString().c_str(), local_buffer->payload(), local_offset, work_id->wr().length(), peer->rank(), local_buffer->cmd_tag());

        std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
        mpi_rc = MPI_Issend((char*)local_buffer->payload() + local_offset,
                            work_id->wr().length(),
                            MPI_BYTE,
                            peer->rank(),
                            local_buffer->cmd_tag(),
                            MPI_COMM_WORLD,
                            &cmd_op->long_send_request());
        mpi_lock.unlock();
    }

    log_debug("mpi_transport",
              "posting cmd_op(%s) (cmd_msg=%p, cmd_msg_size=%lu, peer=%d, cmd_tag=%d",
              cmd_op->toString().c_str(), cmd_op->cmd_msg(), cmd_op->cmd_msg_size(), peer->rank(), NNTI_MPI_CMD_TAG);

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc=MPI_Issend(cmd_op->cmd_msg(),
                      cmd_op->cmd_msg_size(),
                      MPI_BYTE,
                      peer->rank(),
                      NNTI_MPI_CMD_TAG,
                      MPI_COMM_WORLD,
                      &cmd_op->cmd_request());
    mpi_lock.unlock();

    mpi_transport::add_outstanding_cmd_op(
        cmd_op->cmd_request(),
        cmd_op);

    log_debug("mpi_transport", "execute_cmd_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::create_get_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::mpi_cmd_op       **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("mpi_transport", "create_get_op() - enter");

    if (cmd_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
    } else {
        *rdma_op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_, work_id);
    }

    log_debug("mpi_transport", "create_get_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::create_put_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::mpi_cmd_op       **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("mpi_transport", "create_put_op() - enter");

    if (cmd_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
    } else {
        *rdma_op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_, work_id);
    }

    log_debug("mpi_transport", "create_put_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::execute_rdma_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::mpi_cmd_op       *rdma_op)
{
    NNTI_result_t rc = NNTI_OK;
    int mpi_rc = MPI_SUCCESS;

    log_debug("mpi_transport", "execute_rdma_op() - enter");

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_, std::defer_lock);

    nnti::datatype::mpi_peer   *peer = (nnti::datatype::mpi_peer *)work_id->wr().peer();
    nnti::core::mpi_connection *conn = (nnti::core::mpi_connection *)peer->conn();

    nnti::datatype::mpi_buffer *local_buffer  = (nnti::datatype::mpi_buffer *)work_id->wr().local_hdl();
    uint64_t                    local_offset  = work_id->wr().local_offset();
    nnti::datatype::mpi_buffer *remote_buffer = (nnti::datatype::mpi_buffer *)work_id->wr().remote_hdl();
    uint64_t                    remote_offset = work_id->wr().remote_offset();

    switch (work_id->wr().op()) {
        case NNTI_OP_GET:
            mpi_lock.lock();
            mpi_rc = MPI_Irecv((char*)local_buffer->payload() + local_offset,
                               work_id->wr().length(),
                               MPI_BYTE,
                               peer->rank(),
                               local_buffer->get_tag(),
                               MPI_COMM_WORLD,
                               &rdma_op->rdma_request());
            mpi_lock.unlock();
            break;
        case NNTI_OP_PUT:
            mpi_lock.lock();
            mpi_rc = MPI_Issend((char*)local_buffer->payload() + local_offset,
                                work_id->wr().length(),
                                MPI_BYTE,
                                peer->rank(),
                                remote_buffer->put_tag(),
                                MPI_COMM_WORLD,
                                &rdma_op->rdma_request());
            mpi_lock.unlock();
            break;
        case NNTI_OP_NOOP:
        case NNTI_OP_SEND:
        case NNTI_OP_ATOMIC_FADD:
        case NNTI_OP_ATOMIC_CSWAP:
            log_error("mpi_transport", "Should never get here!!!");
            break;
    }

    log_debug("mpi_transport", "posting rdma_op(%s)", rdma_op->toString().c_str());

    mpi_lock.lock();
    mpi_rc = MPI_Issend(rdma_op->cmd_msg(),
                        rdma_op->cmd_msg_size(),
                        MPI_BYTE,
                        peer->rank(),
                        NNTI_MPI_CMD_TAG,
                        MPI_COMM_WORLD,
                        &rdma_op->cmd_request());
    mpi_lock.unlock();
    mpi_transport::add_outstanding_cmd_op(
        rdma_op->cmd_request(),
        rdma_op);

    log_debug("mpi_transport", "execute_rdma_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::create_fadd_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::mpi_cmd_op       **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("mpi_transport", "create_fadd_op() - enter");

    if (cmd_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
    } else {
        *atomic_op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_, work_id);
    }

    log_debug("mpi_transport", "create_fadd_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::create_cswap_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::mpi_cmd_op       **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("mpi_transport", "create_cswap_op() - enter");

    if (cmd_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
    } else {
        *atomic_op = new nnti::core::mpi_cmd_op(this, cmd_msg_size_, work_id);
    }

    log_debug("mpi_transport", "create_cswap_op() - exit");

    return rc;
}

NNTI_result_t
mpi_transport::execute_atomic_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::mpi_cmd_op       *atomic_op)
{
    NNTI_result_t rc = NNTI_OK;
    int mpi_rc = MPI_SUCCESS;

    log_debug("mpi_transport", "execute_atomic_op() - enter");

    nnti::datatype::mpi_peer   *peer = (nnti::datatype::mpi_peer *)work_id->wr().peer();
    nnti::core::mpi_connection *conn = (nnti::core::mpi_connection *)peer->conn();

    nnti::datatype::mpi_buffer *local_buffer  = (nnti::datatype::mpi_buffer *)work_id->wr().local_hdl();
    uint64_t                    local_offset  = work_id->wr().local_offset();
    nnti::datatype::mpi_buffer *remote_buffer = (nnti::datatype::mpi_buffer *)work_id->wr().remote_hdl();
    uint64_t                    remote_offset = work_id->wr().remote_offset();

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc = MPI_Irecv((char*)local_buffer->payload() + local_offset,
                       sizeof(int64_t),
                       MPI_BYTE,
                       peer->rank(),
                       local_buffer->atomic_tag(),
                       MPI_COMM_WORLD,
                       &atomic_op->rdma_request());
    mpi_lock.unlock();

    log_debug("mpi_transport", "atomic_tag=%d", local_buffer->atomic_tag());

    log_debug("mpi_transport", "posting atomic_op(%s)", atomic_op->toString().c_str());

    mpi_lock.lock();
    mpi_rc = MPI_Issend(atomic_op->cmd_msg(),
                        atomic_op->cmd_msg_size(),
                        MPI_BYTE,
                        peer->rank(),
                        NNTI_MPI_CMD_TAG,
                        MPI_COMM_WORLD,
                        &atomic_op->cmd_request());
    mpi_lock.unlock();
    mpi_transport::add_outstanding_cmd_op(
        atomic_op->cmd_request(),
        atomic_op);

    log_debug("mpi_transport", "execute_atomic_op() - exit");

    return rc;
}


NNTI_result_t
mpi_transport::complete_send_command(nnti::core::mpi_cmd_msg *cmd_msg)
{
    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    log_debug("mpi_transport", "complete_send_command() - enter");

    if (cmd_msg->unexpected()) {
        if (unexpected_queue_ == nullptr) {
            // If there is no unexpected queue, then there is no way
            // to communicate unexpected messages to the app.
            // Drop this message.
            NNTI_FAST_STAT(stats_->dropped_unexpected++;);
        } else {
            unexpected_msgs_.push_back(cmd_msg);
            NNTI_event_t *e = create_event(cmd_msg);
            if (unexpected_queue_->invoke_cb(e) != NNTI_OK) {
                unexpected_queue_->push(e);
                unexpected_queue_->notify();
            } else {
                event_freelist_->push(e);
            }
            NNTI_FAST_STAT(stats_->unexpected_recvs++;)
        }
    } else {
        nnti::datatype::nnti_buffer               *b             = cmd_msg->target_buffer();
        assert(b != nullptr);
        nnti::datatype::nnti_event_queue          *q             = nnti::datatype::nnti_event_queue::to_obj(b->eq());
        NNTI_event_t                              *e             = nullptr;
        bool                                       release_event = true;
        uint64_t                                   actual_offset = 0;

        if (cmd_msg->eager()) {
            nnti_rc = b->copy_in(cmd_msg->target_offset(), cmd_msg->eager_payload(), cmd_msg->payload_length(), &actual_offset);

            if (nnti_rc == NNTI_ENOMEM) {

            } else if (nnti_rc != NNTI_OK) {

            } else {

            }

            e  = create_event(cmd_msg, actual_offset);
            if (b->invoke_cb(e) != NNTI_OK) {
                if (q && q->invoke_cb(e) != NNTI_OK) {
                    q->push(e);
                    q->notify();
                    release_event = false;
                }
            }
            if (release_event) {
                // we're done with the event
                event_freelist_->push(e);
            }
            cmd_msg->post_recv();
            add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

            NNTI_FAST_STAT(stats_->short_recvs++;)
        } else {
            nnti::datatype::mpi_buffer *initiator_buffer = cmd_msg->initiator_buffer();
            nnti::datatype::mpi_buffer *target_buffer    = cmd_msg->target_buffer();
            nnti::datatype::mpi_peer   *peer             = cmd_msg->initiator_peer();

            MPI_Request req;
            MPI_Status  status;

            log_debug("mpi_transport", "long send Irecv()");

            std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
            mpi_rc = MPI_Irecv((char*)target_buffer->payload() + cmd_msg->target_offset(),
                               cmd_msg->payload_length(),
                               MPI_BYTE,
                               peer->rank(),
                               initiator_buffer->cmd_tag(),
                               MPI_COMM_WORLD,
                               &req);
            mpi_rc = MPI_Wait(&req, &status);
            mpi_lock.unlock();
            log_debug("mpi_transport", "long send Wait() complete");

            e  = create_event(cmd_msg, cmd_msg->target_offset());
            if (b->invoke_cb(e) != NNTI_OK) {
                if (q && q->invoke_cb(e) != NNTI_OK) {
                    q->push(e);
                    q->notify();
                    release_event = false;
                }
            }
            if (release_event) {
                // we're done with the event
                event_freelist_->push(e);
            }

            cmd_msg->post_recv();
            add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

            NNTI_FAST_STAT(stats_->long_recvs++;)
        }
    }

    log_debug("mpi_transport", "complete_send_command() - exit");

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::complete_get_command(nnti::core::mpi_cmd_msg *cmd_msg)
{
    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    log_debug("mpi_transport", "complete_get_command() - enter");

    nnti::datatype::mpi_buffer *initiator_buffer = cmd_msg->initiator_buffer();
    nnti::datatype::mpi_buffer *target_buffer    = cmd_msg->target_buffer();
    nnti::datatype::mpi_peer   *peer             = cmd_msg->initiator_peer();

    MPI_Request req;
    MPI_Status  status;

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc = MPI_Issend((char*)target_buffer->payload() + cmd_msg->target_offset(),
                        cmd_msg->payload_length(),
                        MPI_BYTE,
                        peer->rank(),
                        initiator_buffer->get_tag(),
                        MPI_COMM_WORLD,
                        &req);
    mpi_rc = MPI_Wait(&req, &status);
    mpi_lock.unlock();

    cmd_msg->post_recv();
    add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

    log_debug("mpi_transport", "complete_get_command() - exit");

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::complete_put_command(nnti::core::mpi_cmd_msg *cmd_msg)
{
    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    log_debug("mpi_transport", "complete_put_command() - enter");

    nnti::datatype::mpi_buffer *initiator_buffer = cmd_msg->initiator_buffer();
    nnti::datatype::mpi_buffer *target_buffer    = cmd_msg->target_buffer();
    nnti::datatype::mpi_peer   *peer             = cmd_msg->initiator_peer();

    MPI_Request req;
    MPI_Status  status;

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc = MPI_Irecv((char*)target_buffer->payload() + cmd_msg->target_offset(),
                        cmd_msg->payload_length(),
                        MPI_BYTE,
                        peer->rank(),
                        target_buffer->put_tag(),
                        MPI_COMM_WORLD,
                        &req);
    mpi_rc = MPI_Wait(&req, &status);
    mpi_lock.unlock();

    cmd_msg->post_recv();
    add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

    log_debug("mpi_transport", "complete_put_command() - exit");

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::complete_fadd_command(nnti::core::mpi_cmd_msg *cmd_msg)
{
    struct atomic_header {
        int64_t operand1;
        int64_t operand2;
    };
    struct atomic_header *h = (struct atomic_header *)cmd_msg->eager_payload();

    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    log_debug("mpi_transport", "complete_fadd_command() - enter");

    nnti::datatype::mpi_buffer *initiator_buffer = cmd_msg->initiator_buffer();
    nnti::datatype::mpi_buffer *target_buffer    = cmd_msg->target_buffer();
    nnti::datatype::mpi_peer   *peer             = cmd_msg->initiator_peer();

    MPI_Request req;
    MPI_Status  status;

    int64_t *op_addr = (int64_t *)((char*)target_buffer->payload() + cmd_msg->target_offset());
    int64_t  current = *(int64_t*)op_addr;

    log_debug("mpi_transport", "adding");
    *op_addr += h->operand1;

    log_debug("mpi_transport", "sending old value back ; current=%ld ; atomic_tag=%d", current, initiator_buffer->atomic_tag());

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc = MPI_Issend(&current,
                        sizeof(int64_t),
                        MPI_BYTE,
                        peer->rank(),
                        initiator_buffer->atomic_tag(),
                        MPI_COMM_WORLD,
                        &req);
    mpi_rc = MPI_Wait(&req, &status);
    mpi_lock.unlock();

    cmd_msg->post_recv();
    add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

    log_debug("mpi_transport", "fadd result (fetch=%ld ; sum=%ld)", current, *op_addr);

    log_debug("mpi_transport", "complete_fadd_command() - exit");

    return NNTI_OK;
}

NNTI_result_t
mpi_transport::complete_cswap_command(nnti::core::mpi_cmd_msg *cmd_msg)
{
    struct atomic_header {
        int64_t operand1;
        int64_t operand2;
    };
    struct atomic_header *h = (struct atomic_header *)cmd_msg->eager_payload();

    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    log_debug("mpi_transport", "complete_cswap_command() - enter");

    nnti::datatype::mpi_buffer *initiator_buffer = cmd_msg->initiator_buffer();
    nnti::datatype::mpi_buffer *target_buffer    = cmd_msg->target_buffer();
    nnti::datatype::mpi_peer   *peer             = cmd_msg->initiator_peer();

    MPI_Request req;
    MPI_Status  status;

    int64_t *op_addr = (int64_t *)((char*)target_buffer->payload() + cmd_msg->target_offset());
    int64_t  current = *(int64_t*)op_addr;

    if (current == h->operand1) {
        log_debug("mpi_transport", "compare success, swapping");
        *op_addr = h->operand2;
    }

    log_debug("mpi_transport", "sending old value back ; current=%ld ; atomic_tag=%d", current, initiator_buffer->atomic_tag());

    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
    mpi_rc = MPI_Issend(&current,
                        sizeof(int64_t),
                        MPI_BYTE,
                        peer->rank(),
                        initiator_buffer->atomic_tag(),
                        MPI_COMM_WORLD,
                        &req);
    mpi_rc = MPI_Wait(&req, &status);
    mpi_lock.unlock();

    cmd_msg->post_recv();
    add_outstanding_cmd_msg(cmd_msg->cmd_request(), cmd_msg);

    log_debug("mpi_transport", "cswap result (operand1=%ld ; operand2=%ld ; target=%ld)", h->operand1, h->operand2, *op_addr);

    log_debug("mpi_transport", "complete_cswap_command() - exit");

    return NNTI_OK;
}

int
mpi_transport::poll_msg_requests(std::vector<MPI_Request> &reqs, int &index, int &done, MPI_Status &event, nnti::core::mpi_cmd_msg *&cmd_msg)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_, std::defer_lock);
    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_, std::defer_lock);
    std::lock(req_lock, mpi_lock);
    int mpi_rc = MPI_Testany(outstanding_msg_requests_.size(), &outstanding_msg_requests_[0], &index, &done, &event);
    if ((mpi_rc == MPI_SUCCESS) && (index >= 0) && (done == TRUE)) {
        // get the cmd_msg now while we hold the lock
        cmd_msg = outstanding_msgs_[index];
        remove_outstanding_cmd_msg(req_lock, cmd_msg->index());
    }
    return mpi_rc;
}

NNTI_result_t
mpi_transport::progress_msg_requests(void)
{
    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    int        index = -1;
    int        done  = -1;
    MPI_Status event;

    nnti::core::mpi_cmd_msg *cmd_msg = nullptr;

    log_debug("mpi_transport", "poll_msg_requests() - enter");

    mpi_rc = poll_msg_requests(outstanding_msg_requests_, index, done, event, cmd_msg);

    /* MPI_Testany() says no active requests.  this is not fatal. */
    if ((mpi_rc == MPI_SUCCESS) && (index == MPI_UNDEFINED) && (done == TRUE)) {
        log_debug("mpi_transport", "MPI_Testany() says there a no active requests (mpi_rc=%d, index=%d, done=%d)", mpi_rc, index, done);
        nnti_rc = NNTI_ENOENT;
    }
    /* MPI_Testany() says requests have completed */
    else if (mpi_rc == MPI_SUCCESS) {
        /* case 1: success */
        if (done == FALSE) {
            nnti_rc = NNTI_EWOULDBLOCK;
        } else {
            nnti_rc = NNTI_OK;

            log_debug("mpi_transport", "polling status is %d, which_req=%d, done=%d", mpi_rc, index, done);
            log_debug("mpi_transport", "Poll Event= {");
            log_debug("mpi_transport", "\tsource  = %d", event.MPI_SOURCE);
            log_debug("mpi_transport", "\ttag     = %d", event.MPI_TAG);
            log_debug("mpi_transport", "\terror   = %d", event.MPI_ERROR);
            log_debug("mpi_transport", "}");

            cmd_msg->unpack();
            switch (cmd_msg->op()) {
                case NNTI_OP_SEND:
                    complete_send_command(cmd_msg);
                    break;
                case NNTI_OP_GET:
                    complete_get_command(cmd_msg);
                    break;
                case NNTI_OP_PUT:
                    complete_put_command(cmd_msg);
                    break;
                case NNTI_OP_ATOMIC_FADD:
                    complete_fadd_command(cmd_msg);
                    break;
                case NNTI_OP_ATOMIC_CSWAP:
                    complete_cswap_command(cmd_msg);
                    break;
            }
        }
    }
    /* MPI_Testany() failure */
    else {
        log_error("mpi_transport", "MPI_Testany() failed: rc=%d",
                mpi_rc);
        nnti_rc = NNTI_EIO;
    }

    if (nnti_rc == NNTI_ETIMEDOUT) {
        log_debug("progress", "poll_msg_requests() timed out");
    } else if (nnti_rc != NNTI_OK) {
        log_debug("progress", "poll_msg_requests() failed (rc=%d)", nnti_rc);
    } else {
        log_debug("progress", "poll_msg_requests() success");
    }

    log_debug("mpi_transport", "poll_msg_requests() - exit");

    return nnti_rc;
}


int
mpi_transport::poll_op_requests(std::vector<MPI_Request> &reqs, int &index, int &done, MPI_Status &event, nnti::core::mpi_cmd_op *&cmd_op)
{
    std::unique_lock<std::mutex> req_lock(outstanding_requests_mutex_, std::defer_lock);
    std::unique_lock<std::mutex> mpi_lock(mpi_mutex_, std::defer_lock);
    std::lock(req_lock, mpi_lock);
    int mpi_rc = MPI_Testany(outstanding_op_requests_.size(), &outstanding_op_requests_[0], &index, &done, &event);
    if ((mpi_rc == MPI_SUCCESS) && (index >= 0) && (done == TRUE)) {
        // get the cmd_op now while we hold the lock
        cmd_op = outstanding_ops_[index];
        remove_outstanding_cmd_op(req_lock, cmd_op->index());
    }
    return mpi_rc;
}

NNTI_result_t
mpi_transport::progress_op_requests(void)
{
    int mpi_rc = MPI_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    int        index = -1;
    int        done  = -1;
    MPI_Status event;

    nnti::core::mpi_cmd_op *cmd_op = nullptr;

    log_debug("mpi_transport", "poll_op_requests() - enter");

    mpi_rc = poll_op_requests(outstanding_op_requests_, index, done, event, cmd_op);

    /* MPI_Testany() says no active requests.  this is not fatal. */
    if ((mpi_rc == MPI_SUCCESS) && (index == MPI_UNDEFINED) && (done == TRUE)) {
        log_debug("mpi_transport", "MPI_Testany() says there a no active requests (mpi_rc=%d, index=%d, done=%d)", mpi_rc, index, done);
        nnti_rc = NNTI_ENOENT;
    }
    /* MPI_Testany() says requests have completed */
    else if (mpi_rc == MPI_SUCCESS) {
        /* case 1: success */
        if (done == FALSE) {
            nnti_rc = NNTI_EWOULDBLOCK;
        } else {
            nnti_rc = NNTI_OK;

            log_debug("mpi_transport", "polling status is %d, which_req=%d, done=%d", mpi_rc, index, done);
            log_debug("mpi_transport", "Poll Event= {");
            log_debug("mpi_transport", "\tsource  = %d", event.MPI_SOURCE);
            log_debug("mpi_transport", "\ttag     = %d", event.MPI_TAG);
            log_debug("mpi_transport", "\terror   = %d", event.MPI_ERROR);
            log_debug("mpi_transport", "}");

            bool need_event = true;

            std::unique_lock<std::mutex> mpi_lock(mpi_mutex_, std::defer_lock);
            nnti::datatype::nnti_work_request &wr = cmd_op->wid()->wr();
            switch (wr.op()) {
                case NNTI_OP_NOOP:
                    log_error("mpi_transport", "Should never get here!!!");
                    break;
                case NNTI_OP_SEND:
                    if (!cmd_op->eager()) {
                        std::unique_lock<std::mutex> mpi_lock(mpi_mutex_);
                        MPI_Wait(&cmd_op->long_send_request(), &event);
                        mpi_lock.unlock();

                        log_debug("mpi_transport", "Long Send Event= {");
                        log_debug("mpi_transport", "\tsource  = %d", event.MPI_SOURCE);
                        log_debug("mpi_transport", "\ttag     = %d", event.MPI_TAG);
                        log_debug("mpi_transport", "\terror   = %d", event.MPI_ERROR);
                        log_debug("mpi_transport", "}");
                    }
                    break;
                case NNTI_OP_GET:
                case NNTI_OP_PUT:
                case NNTI_OP_ATOMIC_FADD:
                case NNTI_OP_ATOMIC_CSWAP:
                    mpi_lock.lock();
                    MPI_Wait(&cmd_op->rdma_request(), &event);
                    mpi_lock.unlock();
                    log_debug("mpi_transport", "RDMA Event= {");
                    log_debug("mpi_transport", "\tsource  = %d", event.MPI_SOURCE);
                    log_debug("mpi_transport", "\ttag     = %d", event.MPI_TAG);
                    log_debug("mpi_transport", "\terror   = %d", event.MPI_ERROR);
                    log_debug("mpi_transport", "}");
                    break;
            }

            nnti::datatype::nnti_event_queue    *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
            nnti::datatype::nnti_buffer         *b              = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
            nnti::datatype::nnti_event_queue    *buf_q          = nnti::datatype::nnti_event_queue::to_obj(b->eq());
            NNTI_event_t                        *e              = create_event(cmd_op);
            bool                                 event_complete = false;
            bool                                 release_event  = true;

            log_debug("mpi_transport", "poll_op_requests() - buf_q=%p  alt_q=%p", buf_q, alt_q);

            if (wr.invoke_cb(e) == NNTI_OK) {
                log_debug("mpi_transport", "poll_op_requests() - wr.invoke_cb()");
                event_complete = true;
            }
            if (!event_complete && alt_q && alt_q->invoke_cb(e) == NNTI_OK) {
                log_debug("mpi_transport", "poll_op_requests() - alt_q->invoke_cb()");
                event_complete = true;
            }
            if (!event_complete && buf_q && buf_q->invoke_cb(e) == NNTI_OK) {
                log_debug("mpi_transport", "poll_op_requests() - buf_q->invoke_cb()");
                event_complete = true;
            }
            if (!event_complete && alt_q) {
                log_debug("mpi_transport", "poll_op_requests() - pushing on alt_q");
                alt_q->push(e);
                alt_q->notify();
                event_complete = true;
                release_event = false;
            }
            if (!event_complete && buf_q) {
                log_debug("mpi_transport", "poll_op_requests() - pushing on buf_q");
                buf_q->push(e);
                buf_q->notify();
                event_complete = true;
                release_event = false;
            }
            if (release_event) {
                event_freelist_->push(e);
            }

            log_debug("mpi_transport", "poll_op_requests() - event_complete == %d", event_complete ? 1 : 0);

            cmd_op_freelist_->push(cmd_op);

            if (cmd_op->eager()) {
                stats_->short_sends++;
            } else {
                stats_->long_sends++;
            }

            if (wr.remote_hdl() == NNTI_INVALID_HANDLE) {
                stats_->unexpected_sends++;
            }
        }
    }
    /* MPI_Testany() failure */
    else {
        log_error("mpi_transport", "MPI_Testany() failed: rc=%d",
                mpi_rc);
        nnti_rc = NNTI_EIO;
    }

    if (nnti_rc == NNTI_ETIMEDOUT) {
        log_debug("progress", "poll_op_requests() timed out");
    } else if (nnti_rc != NNTI_OK) {
        log_debug("progress", "poll_op_requests() failed (rc=%d)", nnti_rc);
    } else {
        log_debug("progress", "poll_op_requests() success");
    }

    log_debug("mpi_transport", "poll_op_requests() - exit");

    return nnti_rc;
}


NNTI_event_t *
mpi_transport::create_event(
    nnti::core::mpi_cmd_msg *cmd_msg,
    uint64_t                 offset)
{
    NNTI_event_t *e = nullptr;

    log_debug("mpi_transport", "create_event(cmd_msg, offset) - enter");

    if (event_freelist_->pop(e) == false) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = NNTI_OP_SEND;
    e->peer       = nnti::datatype::nnti_peer::to_hdl(cmd_msg->initiator_peer());
    log_debug("mpi_transport", "e->peer = %p", e->peer);
    e->length     = cmd_msg->payload_length();

    if (cmd_msg->unexpected()) {
        log_debug("mpi_transport", "creating unexpected event");
        e->type       = NNTI_EVENT_UNEXPECTED;
        e->start      = nullptr;
        e->offset     = 0;
        e->context    = 0;
    } else {
        log_debug("mpi_transport", "creating eager event");
        e->type       = NNTI_EVENT_RECV;
        e->start      = cmd_msg->target_buffer()->payload();
        e->offset     = offset;
        e->context    = 0;
    }

    log_debug("mpi_transport", "create_event(cmd_msg, offset) - exit");

    return(e);
}

NNTI_event_t *
mpi_transport::create_event(
    nnti::core::mpi_cmd_msg *cmd_msg)
{
    NNTI_event_t *e = nullptr;

    log_debug("mpi_transport", "create_event(cmd_msg) - enter");

    e = create_event(cmd_msg, cmd_msg->target_offset());

    log_debug("mpi_transport", "create_event(cmd_msg) - exit");

    return(e);
}

NNTI_event_t *
mpi_transport::create_event(
    nnti::core::mpi_cmd_op *cmd_op)
{
    nnti::datatype::nnti_work_id            *wid = cmd_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    nnti::datatype::nnti_buffer             *b   = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
    NNTI_event_t                            *e   = nullptr;

    log_debug("mpi_transport", "create_event(cmd_op) - enter");

    if (event_freelist_->pop(e) == false) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = wr.op();
    e->peer       = wr.peer();
    e->length     = wr.length();

    if (wr.op() == NNTI_OP_SEND) {
        e->type = NNTI_EVENT_SEND;
    }
    if (wr.op() == NNTI_OP_PUT) {
        e->type = NNTI_EVENT_PUT;
    }
    if (wr.op() == NNTI_OP_GET) {
        e->type = NNTI_EVENT_GET;
    }
    if ((wr.op() == NNTI_OP_ATOMIC_FADD) || (wr.op() == NNTI_OP_ATOMIC_CSWAP)) {
        e->type = NNTI_EVENT_ATOMIC;
    }
    e->start      = b->payload();
    e->offset     = wr.local_offset();
    e->context    = wr.event_context();

    log_debug("mpi_transport", "create_event(cmd_op) - exit");

    return(e);
}

nnti::datatype::nnti_buffer *
mpi_transport::unpack_buffer(
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_buffer_t                hdl;
    nnti::datatype::nnti_buffer *b = nullptr;

    this->dt_unpack(&hdl, packed_buf, packed_len);
    b = nnti::datatype::nnti_buffer::to_obj(hdl);

    nnti::datatype::nnti_buffer *found = buffer_map_.get(b->payload());
    if (found == nullptr) {
        log_debug("mpi_transport", "unpack_buffer() - buffer not found in buffer_map_ for address=%p", b->payload());
        // if the buffer is not in the map, then use b.
        found = b;
    } else {
        // if the buffer is in the map, then delete b.
        delete b;
    }

    return found;
}

} /* namespace transports */
} /* namespace nnti */
