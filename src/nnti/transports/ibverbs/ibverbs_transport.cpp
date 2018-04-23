// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file ibverbs_transport.hpp
 *
 * @brief ibverbs_transport.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Aug 26, 2015
 */


#include "nnti/nnti_pch.hpp"

#include <fcntl.h>
#include <sys/poll.h>

#include <atomic>
#include <thread>
#include <map>
#include <vector>

#include "common/Configuration.hh"

#include "nnti/nntiConfig.h"

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

#include "nnti/transports/ibverbs/ibverbs_atomic_op.hpp"
#include "nnti/transports/ibverbs/ibverbs_buffer.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_msg.hpp"
#include "nnti/transports/ibverbs/ibverbs_cmd_op.hpp"
#include "nnti/transports/ibverbs/ibverbs_connection.hpp"
#include "nnti/transports/ibverbs/ibverbs_peer.hpp"
#include "nnti/transports/ibverbs/ibverbs_rdma_op.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"

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
ibverbs_transport::ibverbs_transport(
    faodel::Configuration &config)
    : base_transport(NNTI_TRANSPORT_IBVERBS,
                     config),
      started_(false),
      event_freelist_size_(128),
      cmd_op_freelist_size_(128),
      rdma_op_freelist_size_(128),
      atomic_op_freelist_size_(128)
{
    faodel::rc_t rc = 0;
    uint64_t uint_value = 0;

    nthread_lock_init(&new_connection_lock_);

    me_ = nnti::datatype::ibverbs_peer(this, url_);

    rc = config.GetUInt(&uint_value, "nnti.freelist.size", "128");
    if (rc == 0) {
        event_freelist_size_     = uint_value;
        cmd_op_freelist_size_    = uint_value;
        rdma_op_freelist_size_   = uint_value;
        atomic_op_freelist_size_ = uint_value;
    }
    event_freelist_     = new nnti::core::nnti_freelist<NNTI_event_t*>(event_freelist_size_);
    cmd_op_freelist_    = new nnti::core::nnti_freelist<nnti::core::ibverbs_cmd_op*>(cmd_op_freelist_size_);
    rdma_op_freelist_   = new nnti::core::nnti_freelist<nnti::core::ibverbs_rdma_op*>(rdma_op_freelist_size_);
    atomic_op_freelist_ = new nnti::core::nnti_freelist<nnti::core::ibverbs_atomic_op*>(atomic_op_freelist_size_);

    return;
}

/**
 * @brief Deactivates a specific transport.
 *
 * \return A result code (NNTI_OK or an error)
 */
ibverbs_transport::~ibverbs_transport()
{
    return;
}

NNTI_result_t
ibverbs_transport::start(void)
{
    NNTI_result_t rc     = NNTI_OK;
    int           ibv_rc = 0;

    struct ibv_device_attr dev_attr;
    struct ibv_port_attr   dev_port_attr;

#if NNTI_HAVE_IBV_EXP_QUERY_DEVICE
    struct ibv_exp_device_attr exp_dev_attr;
#endif

    log_debug("ibverbs_transport", "enter");

    log_debug("ibverbs_transport", "initializing InfiniBand");

    struct ibv_device *dev=get_ib_device();

    assert(dev!=nullptr && "The IB transport couldn't find an ibverbs compatible device on this machine.");
    /* open the device */
    ctx_ = ibv_open_device(dev);
    if (!ctx_) {
        log_error("ibverbs_transport", "ibv_open_device failed");
        return NNTI_EIO;
    }

    nic_port_ = 1;

    /* get the lid and verify port state */
    ibv_rc = ibv_query_port(ctx_, nic_port_, &dev_port_attr);
    if (ibv_rc) {
        log_error("ibverbs_transport", "ibv_query_port failed");
        return NNTI_EIO;
    }

    nic_lid_ = dev_port_attr.lid;

    if (dev_port_attr.state != IBV_PORT_ACTIVE) {
        log_error("ibverbs_transport", "port is not active.  cannot continue.");
        return NNTI_EIO;
    }

    active_mtu_bytes_=128;
    for (int i=0;i<dev_port_attr.active_mtu;i++) {
        active_mtu_bytes_ *= 2;
    }
    cmd_msg_size_  = active_mtu_bytes_;
    cmd_msg_count_ = 128;
    log_debug("ibverbs_transport", "dev_port_attr.active_mtu(%d) active_mtu_bytes_(%u) cmd_msg_size_(%u) cmd_msg_count_(%u)",
        dev_port_attr.active_mtu, active_mtu_bytes_, cmd_msg_size_, cmd_msg_count_);

    /* Query the device for device attributes (max QP, max WR, etc) */
    ibv_rc = ibv_query_device(ctx_, &dev_attr);
    if (ibv_rc) {
        log_error("ibverbs_transport", "ibv_query_device failed");
        return NNTI_EIO;
    }

#if NNTI_HAVE_IBV_EXP_QUERY_DEVICE
    exp_dev_attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED - 1;
    ibv_rc = ibv_exp_query_device(ctx_, &exp_dev_attr);
    if (ibv_rc) {
        log_error("ibverbs_transport", "ibv_exp_query_device failed");
        return NNTI_EIO;
    }
#endif
#if NNTI_HAVE_IBV_EXP_ATOMIC_HCA_REPLY_BE
    byte_swap_atomic_result_ = (exp_dev_attr.exp_atomic_cap == IBV_EXP_ATOMIC_HCA_REPLY_BE);
#else
    byte_swap_atomic_result_ = false;
#endif

    log_debug("ibverbs_transport", "max %d completion queue entries", dev_attr.max_cqe);
    cqe_count_ = dev_attr.max_cqe;

    log_debug("ibverbs_transport", "max %d shared receive queue work requests", dev_attr.max_srq_wr);
    srq_count_ = (float)dev_attr.max_srq_wr * 0.8;

    log_debug("ibverbs_transport", "max %d shared receive queue scatter gather elements", dev_attr.max_srq_sge);
    sge_count_ = 1;

    log_debug("ibverbs_transport", "max %d queue pair work requests", dev_attr.max_qp_wr);
    qp_count_ = 1024;

    attrs_.mtu                 = cmd_msg_size_;
    attrs_.max_cmd_header_size = nnti::core::ibverbs_cmd_msg::header_length();
    attrs_.max_eager_size      = attrs_.mtu - attrs_.max_cmd_header_size;
    attrs_.cmd_queue_size      = cmd_msg_count_;
    log_debug("mpi_transport", "attrs_.mtu                =%d", attrs_.mtu);
    log_debug("mpi_transport", "attrs_.max_cmd_header_size=%d", attrs_.max_cmd_header_size);
    log_debug("mpi_transport", "attrs_.max_eager_size     =%d", attrs_.max_eager_size);
    log_debug("mpi_transport", "attrs_.cmd_queue_size     =%d", attrs_.cmd_queue_size);

    /* Allocate a Protection Domain (global) */
    pd_ = ibv_alloc_pd(ctx_);
    if (!pd_) {
        log_error("ibverbs_transport", "ibv_alloc_pd failed");
        return NNTI_EIO;
    }

    rc = setup_command_channel();
    if (rc) {
        log_error("ibverbs_transport", "setup_command_channel failed");
        return NNTI_EIO;
    }
    rc = setup_rdma_channel();
    if (rc) {
        log_error("ibverbs_transport", "setup_rdma_channel failed");
        return NNTI_EIO;
    }
    rc = setup_long_get_channel();
    if (rc) {
        log_error("ibverbs_transport", "setup_long_get_channel failed");
        return NNTI_EIO;
    }

    cmd_buffer_ = new nnti::core::ibverbs_cmd_buffer(this, nullptr, cmd_msg_size_, cmd_msg_count_);

    rc = setup_interrupt_pipe();
    if (rc) {
        log_error("ibverbs_transport", "setup_interrupt_pipe failed");
        return NNTI_EIO;
    }

    rc = setup_freelists();
    if (rc) {
        log_error("ibverbs_transport", "setup_freelists() failed");
        return NNTI_EIO;
    }

    NNTI_STATS_DATA(
    stats_ = new webhook_stats;
    )

    assert(webhook::Server::IsRunning() && "webhook is not running.  Confirm Bootstrap configuration and try again.");

    register_webhook_cb();

    log_debug("ibverbs_transport", "url_=%s", url_.url().c_str());

    start_progress_thread();

    log_debug("ibverbs_transport", "InfiniBand (ibverbs) Initialized");

    started_ = true;

    log_debug("ibverbs_transport", "exit");

    return NNTI_OK;
}

NNTI_result_t
ibverbs_transport::stop(void)
{
    NNTI_result_t rc=NNTI_OK;;

    log_debug("ibverbs_transport", "enter");

    started_ = false;

//    close_all_conn();

    unregister_webhook_cb();

    stop_progress_thread();

    teardown_freelists();

    delete cmd_buffer_;

    ibv_destroy_comp_channel(long_get_comp_channel_);
    ibv_destroy_cq(long_get_cq_);
    ibv_destroy_srq(long_get_srq_);

    ibv_destroy_comp_channel(rdma_comp_channel_);
    ibv_destroy_cq(rdma_cq_);
    ibv_destroy_srq(rdma_srq_);

    ibv_destroy_comp_channel(cmd_comp_channel_);
    ibv_destroy_cq(cmd_cq_);
    ibv_destroy_srq(cmd_srq_);

    ibv_dealloc_pd(pd_);

    ibv_close_device(ctx_);

cleanup:
    log_debug("ibverbs_transport", "exit");

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
ibverbs_transport::initialized(void)
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
ibverbs_transport::get_url(
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
ibverbs_transport::pid(NNTI_process_id_t *pid)
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
ibverbs_transport::attrs(NNTI_attrs_t *attrs)
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
ibverbs_transport::connect(
    const char  *url,
    const int    timeout,
    NNTI_peer_t *peer_hdl)
{
    nnti::core::nnti_url            peer_url(url);
    nnti::datatype::nnti_peer      *peer = new nnti::datatype::ibverbs_peer(this, peer_url);
    nnti::core::ibverbs_connection *conn;

    nthread_lock(&new_connection_lock_);

    // look for an existing connection to reuse
    log_debug("ibverbs_transport", "Looking for connection with pid=%016lx", peer->pid());
    conn = (nnti::core::ibverbs_connection*)conn_map_.get(peer->pid());
    if (conn != nullptr) {
        log_debug("ibverbs_transport", "Found connection with pid=%016lx", peer->pid());
        // reuse an existing connection
        *peer_hdl = (NNTI_peer_t)conn->peer();
        nthread_unlock(&new_connection_lock_);
        return NNTI_OK;
    }
    log_debug("ibverbs_transport", "Couldn't find connection with pid=%016lx", peer->pid());

    conn = new nnti::core::ibverbs_connection(this, cmd_msg_size_, cmd_msg_count_);

    peer->conn(conn);
    conn->peer(peer);

    conn_map_.insert(conn);

    nthread_unlock(&new_connection_lock_);

    std::string  reply;
    std::string  wh_path = build_webhook_connect_path(conn);
    int wh_rc = 0;
    int retries = 5;

    wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);

    while (wh_rc != 0 && --retries) {
        sleep(1);
        wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
    }
    if (wh_rc != 0) {
        log_debug("ibverbs_transport", "connect() timed out");
        return(NNTI_ETIMEDOUT);
    }

    conn->peer_params(reply);

    conn->transition_to_ready();

    *peer_hdl = (NNTI_peer_t)peer;

    return NNTI_OK;
}

/**
 * @brief Terminate communication with this peer.
 *
 * \param[in] peer_hdl  A handle to a peer.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
ibverbs_transport::disconnect(
    NNTI_peer_t peer_hdl)
{
    nnti::datatype::nnti_peer   *peer     = (nnti::datatype::nnti_peer *)peer_hdl;
    nnti::core::nnti_url        &peer_url = peer->url();
    std::string                  reply;
    nnti::core::nnti_connection *conn     = peer->conn();

    if (conn==nullptr) {
        log_warn("ibverbs_transport", "disconnect failed.  peer conn (%016lx) is nullptr", peer->pid());
        return NNTI_EINVAL;
    }

    std::string wh_path = build_webhook_disconnect_path(conn);

    int wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
    if (wh_rc != 0) {
        return(NNTI_ETIMEDOUT);
    }

    conn_map_.remove(conn);

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
ibverbs_transport::eq_create(
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
ibverbs_transport::eq_create(
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
ibverbs_transport::eq_destroy(
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
ibverbs_transport::eq_wait(
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
    log_debug_stream("ibverbs_transport") << event;
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
ibverbs_transport::next_unexpected(
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event)
{
    NNTI_result_t rc = NNTI_OK;
    uint64_t actual_offset;
    nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)dst_hdl;

    log_debug("next_unexpected", "enter");

    if (unexpected_msgs_.size() == 0) {
        log_debug("ibverbs_transport", "next_unexpected - unexpected_msgs_ list is empty");
        return NNTI_ENOENT;
    }

    nnti::core::ibverbs_cmd_msg *unexpected_msg = unexpected_msgs_.front();
    unexpected_msgs_.pop_front();

    if (unexpected_msg->eager()) {
        rc = b->copy_in(dst_offset, unexpected_msg->eager_payload(), unexpected_msg->payload_length(), &actual_offset);
        if (rc != NNTI_OK) {
            log_error("next_unexpected", "copy_in() failed (rc=%d)", rc);
        }
        NNTI_FAST_STAT(stats_->short_recvs++;)
    } else {
        nnti::datatype::ibverbs_peer   *peer     = unexpected_msg->initiator_peer();
        nnti::core::ibverbs_connection *conn     = (nnti::core::ibverbs_connection *)peer->conn();
        nnti::datatype::ibverbs_buffer *init_buf = unexpected_msg->initiator_buffer();
        struct ibv_send_wr sq_wr, *bad_wr;
        struct ibv_sge     sge;

        sq_wr.wr_id      = (uint64_t)unexpected_msg;
        sq_wr.next       = nullptr;
        sq_wr.sg_list    = &sge;
        sq_wr.num_sge    = 1;
        sq_wr.opcode     = IBV_WR_RDMA_READ;
        sq_wr.send_flags = IBV_SEND_SIGNALED;

        sq_wr.wr.rdma.remote_addr = (uint64_t)init_buf->payload() + unexpected_msg->initiator_offset();
        sq_wr.wr.rdma.rkey        = init_buf->rkey();

        sge.addr   = (uint64_t)b->payload() + dst_offset;
        sge.length = unexpected_msg->payload_length();
        sge.lkey   = b->lkey();

        print_send_wr(&sq_wr);
        if (ibv_post_send(conn->long_get_qp(), &sq_wr, &bad_wr)) {
            log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
            rc = NNTI_EIO;
        }

        struct ibv_wc long_get_wc;
        memset(&long_get_wc, 0, sizeof(struct ibv_wc));
        while ((rc = poll_cq(long_get_cq_, &long_get_wc)) == NNTI_ENOENT) {
            log_debug("ibverbs_transport", "long get not done yet");
        }
        if (rc != NNTI_OK) {
            log_error("ibverbs_transport", "long get failed");
        }

        log_debug("poll_cmd_cqs", "sending ACK");
        nnti::core::ibverbs_cmd_op *ack_op  = nullptr;
        rc = create_ack_op(unexpected_msg->src_op_id(), &ack_op);
        rc = execute_ack_op(peer, ack_op);
        log_debug("poll_cmd_cqs", "ACK sent");

        NNTI_FAST_STAT(stats_->long_recvs++;)
    }

    unexpected_msg->post_recv();

    result_event->trans_hdl  = nnti::transports::transport::to_hdl(this);
    result_event->result     = NNTI_OK;
    result_event->op         = NNTI_OP_SEND;
    result_event->peer       = nnti::datatype::nnti_peer::to_hdl(unexpected_msg->initiator_peer());
    result_event->length     = unexpected_msg->payload_length();
    result_event->type       = NNTI_EVENT_SEND;
    result_event->start      = b->payload();
    result_event->offset     = actual_offset;
    result_event->context    = 0;

    log_debug("next_unexpected", "result_event->peer = %p", result_event->peer);

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
ibverbs_transport::get_unexpected(
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
ibverbs_transport::event_complete(
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
ibverbs_transport::dt_unpack(
    void           *nnti_dt,
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_result_t                   rc = NNTI_OK;
    nnti::datatype::ibverbs_buffer *b  = nullptr;
    nnti::datatype::nnti_peer      *p  = nullptr;

    switch (*(NNTI_datatype_t*)packed_buf) {
        case NNTI_dt_buffer:
            log_debug("base_transport", "dt is a buffer");
            b = new nnti::datatype::ibverbs_buffer(this, packed_buf, packed_len);
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
ibverbs_transport::alloc(
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    char                               **reg_ptr,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::ibverbs_buffer(this,
                                                                      size,
                                                                      flags,
                                                                      eq,
                                                                      cb,
                                                                      cb_context);

    buffer_map_.insert(b);

    NNTI_FAST_STAT(stats_->pinned_buffers++;)
    NNTI_SLOW_STAT(stats_->pinned_bytes  += b->size();)

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
ibverbs_transport::free(
    NNTI_buffer_t reg_buf)
{
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;

    buffer_map_.remove(b);

    NNTI_FAST_STAT(stats_->pinned_buffers--;)
    NNTI_SLOW_STAT(stats_->pinned_bytes  -= b->size();)

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
ibverbs_transport::register_memory(
    char                                *buffer,
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::ibverbs_buffer(this,
                                                                      buffer,
                                                                      size,
                                                                      flags,
                                                                      eq,
                                                                      cb,
                                                                      cb_context);

    buffer_map_.insert(b);

    NNTI_FAST_STAT(stats_->pinned_buffers++;)
    NNTI_SLOW_STAT(stats_->pinned_bytes  += b->size();)

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
ibverbs_transport::unregister_memory(
    NNTI_buffer_t reg_buf)
{
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)reg_buf;

    buffer_map_.remove(b);

    NNTI_FAST_STAT(stats_->pinned_buffers--;)
    NNTI_SLOW_STAT(stats_->pinned_bytes  -= b->size();)

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
ibverbs_transport::dt_peer_to_pid(
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
ibverbs_transport::dt_pid_to_peer(
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
ibverbs_transport::send(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ibverbs_cmd_op   *cmd_op  = nullptr;

    log_debug("ibverbs_transport", "send - wr.local_offset=%lu", wr->local_offset());

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
ibverbs_transport::put(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ibverbs_rdma_op  *put_op  = nullptr;

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
ibverbs_transport::get(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ibverbs_rdma_op  *get_op  = nullptr;

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
ibverbs_transport::atomic_fop(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id  *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ibverbs_atomic_op *atomic_op = nullptr;

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
ibverbs_transport::atomic_cswap(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id  *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ibverbs_atomic_op *atomic_op = nullptr;

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
ibverbs_transport::cancel(
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
ibverbs_transport::cancelall(
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
ibverbs_transport::interrupt()
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
ibverbs_transport::wait(
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
ibverbs_transport::waitany(
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
ibverbs_transport::waitall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    NNTI_status_t  *status)
{
    return NNTI_OK;
}

/*************************************************************/

ibverbs_transport *
ibverbs_transport::get_instance(
    faodel::Configuration &config)
{
    static ibverbs_transport *instance = new ibverbs_transport(config);
    return instance;
}

/*************************************************************
 * Accessors for data members specific to this interconnect.
 *************************************************************/

NNTI_result_t
ibverbs_transport::setup_command_channel(void)
{
    int flags;

    cmd_comp_channel_ = ibv_create_comp_channel(ctx_);
    if (!cmd_comp_channel_) {
        log_error("ibverbs_transport", "ibv_create_comp_channel failed");
        return NNTI_EIO;
    }
    cmd_cq_ = ibv_create_cq(
            ctx_,
            cqe_count_,
            NULL,
            cmd_comp_channel_,
            0);
    if (!cmd_cq_) {
        log_error("ibverbs_transport", "ibv_create_cq failed: %s", strerror(errno));
        return NNTI_EIO;
    }

    cmd_srq_count_=0;

    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr)); // initialize to avoid valgrind uninitialized warning
    srq_attr.attr.max_wr = srq_count_;
    srq_attr.attr.max_sge = sge_count_;

    cmd_srq_ = ibv_create_srq(pd_, &srq_attr);
    if (!cmd_srq_)  {
        log_error("ibverbs_transport", "ibv_create_srq failed");
        return NNTI_EIO;
    }

    if (ibv_req_notify_cq(cmd_cq_, 0)) {
        log_error("ibverbs_transport", "ibv_req_notify_cq failed");
        return NNTI_EIO;
    }

    /* use non-blocking IO on the async fd and completion fd */
    flags = fcntl(ctx_->async_fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get async_fd flags");
        return NNTI_EIO;
    }
    if (fcntl(ctx_->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set async_fd to nonblocking");
        return NNTI_EIO;
    }

    flags = fcntl(cmd_comp_channel_->fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get completion fd flags");
        return NNTI_EIO;
    }
    if (fcntl(cmd_comp_channel_->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set completion fd to nonblocking");
        return NNTI_EIO;
    }

    return(NNTI_OK);
}

NNTI_result_t
ibverbs_transport::setup_rdma_channel(void)
{
    int flags;

    rdma_comp_channel_ = ibv_create_comp_channel(ctx_);
    if (!rdma_comp_channel_) {
        log_error("ibverbs_transport", "ibv_create_comp_channel failed");
        return NNTI_EIO;
    }
    rdma_cq_ = ibv_create_cq(
            ctx_,
            cqe_count_,
            NULL,
            rdma_comp_channel_,
            0);
    if (!rdma_cq_) {
        log_error("ibverbs_transport", "ibv_create_cq failed");
        return NNTI_EIO;
    }

    rdma_srq_count_=0;

    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));   // initialize to avoid valgrind warning
    srq_attr.attr.max_wr  = srq_count_;
    srq_attr.attr.max_sge = sge_count_;

    rdma_srq_ = ibv_create_srq(pd_, &srq_attr);
    if (!rdma_srq_)  {
        log_error("ibverbs_transport", "ibv_create_srq failed");
        return NNTI_EIO;
    }

    if (ibv_req_notify_cq(rdma_cq_, 0)) {
        log_error("ibverbs_transport", "ibv_req_notify_cq failed");
        return NNTI_EIO;
    }

    /* use non-blocking IO on the async fd and completion fd */
    flags = fcntl(ctx_->async_fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get async_fd flags");
        return NNTI_EIO;
    }
    if (fcntl(ctx_->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set async_fd to nonblocking");
        return NNTI_EIO;
    }

    flags = fcntl(rdma_comp_channel_->fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get completion fd flags");
        return NNTI_EIO;
    }
    if (fcntl(rdma_comp_channel_->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set completion fd to nonblocking");
        return NNTI_EIO;
    }

    return(NNTI_OK);
}

NNTI_result_t
ibverbs_transport::setup_long_get_channel(void)
{
    int flags;

    long_get_comp_channel_ = ibv_create_comp_channel(ctx_);
    if (!long_get_comp_channel_) {
        log_error("ibverbs_transport", "ibv_create_comp_channel failed");
        return NNTI_EIO;
    }
    long_get_cq_ = ibv_create_cq(
            ctx_,
            cqe_count_,
            NULL,
            long_get_comp_channel_,
            0);
    if (!long_get_cq_) {
        log_error("ibverbs_transport", "ibv_create_cq failed");
        return NNTI_EIO;
    }

    long_get_srq_count_=0;

    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));   // initialize to avoid valgrind warning
    srq_attr.attr.max_wr  = srq_count_;
    srq_attr.attr.max_sge = sge_count_;

    long_get_srq_ = ibv_create_srq(pd_, &srq_attr);
    if (!long_get_srq_)  {
        log_error("ibverbs_transport", "ibv_create_srq failed");
        return NNTI_EIO;
    }

    if (ibv_req_notify_cq(long_get_cq_, 0)) {
        log_error("ibverbs_transport", "ibv_req_notify_cq failed");
        return NNTI_EIO;
    }

    /* use non-blocking IO on the async fd and completion fd */
    flags = fcntl(ctx_->async_fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get async_fd flags");
        return NNTI_EIO;
    }
    if (fcntl(ctx_->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set async_fd to nonblocking");
        return NNTI_EIO;
    }

    flags = fcntl(long_get_comp_channel_->fd, F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get completion fd flags");
        return NNTI_EIO;
    }
    if (fcntl(long_get_comp_channel_->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set completion fd to nonblocking");
        return NNTI_EIO;
    }

    return(NNTI_OK);
}

NNTI_result_t
ibverbs_transport::setup_interrupt_pipe(void)
{
    int rc=0;
    int flags;

    rc=pipe(interrupt_pipe_);
    if (rc < 0) {
        log_error("ibverbs_transport", "pipe() failed: %s", strerror(errno));
        return NNTI_EIO;
    }

    /* use non-blocking IO on the read side of the interrupt pipe */
    flags = fcntl(interrupt_pipe_[0], F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
        return NNTI_EIO;
    }
    if (fcntl(interrupt_pipe_[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
        return NNTI_EIO;
    }

    /* use non-blocking IO on the write side of the interrupt pipe */
    flags = fcntl(interrupt_pipe_[1], F_GETFL);
    if (flags < 0) {
        log_error("ibverbs_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
        return NNTI_EIO;
    }
    if (fcntl(interrupt_pipe_[1], F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("ibverbs_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
        return NNTI_EIO;
    }

    return(NNTI_OK);
}

NNTI_result_t
ibverbs_transport::setup_freelists(void)
{
    for (uint64_t i=0;i<cmd_op_freelist_size_;i++) {
        nnti::core::ibverbs_cmd_op *op = new nnti::core::ibverbs_cmd_op(this, cmd_msg_size_);
        cmd_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<rdma_op_freelist_size_;i++) {
        nnti::core::ibverbs_rdma_op *op = new nnti::core::ibverbs_rdma_op(this);
        rdma_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<atomic_op_freelist_size_;i++) {
        nnti::core::ibverbs_atomic_op *op = new nnti::core::ibverbs_atomic_op(this);
        atomic_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<event_freelist_size_;i++) {
        NNTI_event_t *e = new NNTI_event_t;
        event_freelist_->push(e);
    }

    return(NNTI_OK);
}
NNTI_result_t
ibverbs_transport::teardown_freelists(void)
{
    while(!event_freelist_->empty()) {
        NNTI_event_t *e = nullptr;
        if (event_freelist_->pop(e)) {
            delete e;
        }
    }

    while(!cmd_op_freelist_->empty()) {
        nnti::core::ibverbs_cmd_op *op = nullptr;
        if (cmd_op_freelist_->pop(op)) {
            delete op;
        }
    }

    while(!rdma_op_freelist_->empty()) {
        nnti::core::ibverbs_rdma_op *op = nullptr;
        if (rdma_op_freelist_->pop(op)) {
            delete op;
        }
    }

    while(!atomic_op_freelist_->empty()) {
        nnti::core::ibverbs_atomic_op *op = nullptr;
        if (atomic_op_freelist_->pop(op)) {
            delete op;
        }
    }

    return(NNTI_OK);
}

void
ibverbs_transport::progress(void)
{
    NNTI_result_t rc;

    while (terminate_progress_thread_.load() == false) {
        log_debug("ibverbs_transport::progress", "this is the progress thread");

        rc = poll_fds();

        if (rc == NNTI_ETIMEDOUT) {
            log_debug("progress", "poll_all() timed out");
        } else if (rc != NNTI_OK) {
            log_error("progress", "poll_all() failed (rc=%d)", rc);
        } else {
            log_debug("progress", "poll_all() success");
        }

        do {
            rc = poll_cmd_cq();
        } while (rc == NNTI_OK);
        do {
            rc = poll_rdma_cq();
        } while (rc == NNTI_OK);
    }

    return;
}
void
ibverbs_transport::start_progress_thread(void)
{
    terminate_progress_thread_ = false;
    progress_thread_ = std::thread(&nnti::transports::ibverbs_transport::progress, this);
}
void
ibverbs_transport::stop_progress_thread(void)
{
    terminate_progress_thread_ = true;
    progress_thread_.join();
}


struct ibv_device *
ibverbs_transport::get_ib_device(void)
{
    struct ibv_device *dev=nullptr;
    struct ibv_device **dev_list;
    int dev_count=0;

    dev_list = ibv_get_device_list(&dev_count);
    if (dev_count == 0)
        return nullptr;
    if (dev_count > 1) {
        log_debug("ibverbs_transport", "found %d devices, defaulting the dev_list[0] (%p)", dev_count, dev_list[0]);
    }
    dev = dev_list[0];
    ibv_free_device_list(dev_list);

    return dev;
}

void
ibverbs_transport::connect_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    nnti::core::ibverbs_connection *conn;

    log_debug("ibverbs_transport", "inbound connection from %s", std::string(args.at("hostname")+":"+args.at("port")).c_str());

    nthread_lock(&new_connection_lock_);

    nnti::core::nnti_url peer_url = nnti::core::nnti_url(args.at("hostname"), args.at("port"));

    log_debug("ibverbs_transport", "Looking for connection with pid=%016lx", peer_url.pid());
    conn = (nnti::core::ibverbs_connection*)conn_map_.get(peer_url.pid());
    if (conn != nullptr) {
        log_debug("ibverbs_transport", "Found connection with pid=%016lx", peer_url.pid());
    } else {
        log_debug("ibverbs_transport", "Couldn't find connection with pid=%016lx", peer_url.pid());

        conn = new nnti::core::ibverbs_connection(this, cmd_msg_size_, cmd_msg_count_, args);
        conn_map_.insert(conn);

        conn->transition_to_ready();
    }

    nthread_unlock(&new_connection_lock_);

    results << "hostname=" << url_.hostname() << std::endl;
    results << "addr="     << url_.addr()     << std::endl;
    results << "port="     << url_.port()     << std::endl;
    results << "lid="      << nic_lid_        << std::endl;
    results << conn->reply_string();
}

void
ibverbs_transport::disconnect_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    nnti::core::nnti_connection *conn = nullptr;
    nnti::core::nnti_url         peer_url(args.at("hostname"), args.at("port"));

    log_debug("ibverbs_transport", "%s is disconnecting", peer_url.url().c_str());
    conn = conn_map_.get(peer_url.pid());
    log_debug("ibverbs_transport", "connection map says %s => conn(%p)", peer_url.url().c_str(), conn);

    conn_map_.remove(conn);

    delete conn;

    log_debug("ibverbs_transport", "disconnect_cb - results=%s", results.str().c_str());
}

void
ibverbs_transport::stats_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    html::mkHeader(results, "Transfer Statistics");
    html::mkText(results,"Transfer Statistics",1);

    NNTI_STATS_DATA(
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
    stats.push_back("fadds            = " + std::to_string(stats_->fadds.load()));
    stats.push_back("cswaps           = " + std::to_string(stats_->cswaps.load()));
    html::mkList(results, stats);
    )

    html::mkFooter(results);
}

void
ibverbs_transport::peers_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    html::mkHeader(results, "Connected Peers");
    html::mkText(results,"Connected Peers",1);

    std::vector<std::string> links;
    nnti::core::nnti_connection_map_iter_t iter;
    for (iter = conn_map_.begin() ; iter != conn_map_.end() ; iter++) {
        std::string p((*iter)->peer()->url().url());
        p.replace(0,2,"http");
        links.push_back(html::mkLink(p, p));
    }
    html::mkList(results, links);

    html::mkFooter(results);
}

std::string
ibverbs_transport::build_webhook_path(
    nnti::core::nnti_connection *conn,
    const char                  *service)
{
    std::stringstream wh_url;

    wh_url << "/nnti/ib/" << service;
    wh_url << "&hostname=" << url_.hostname();
    wh_url << "&addr="     << url_.addr();
    wh_url << "&port="     << url_.port();
    wh_url << "&lid="      << nic_lid_;
    wh_url << conn->query_string();

    return wh_url.str();
}
std::string
ibverbs_transport::build_webhook_connect_path(
    nnti::core::nnti_connection *conn)
{
    return build_webhook_path(conn, "connect");
}
std::string
ibverbs_transport::build_webhook_disconnect_path(
    nnti::core::nnti_connection *conn)
{
    return build_webhook_path(conn, "disconnect");
}

void
ibverbs_transport::register_webhook_cb(void)
{
    webhook::Server::registerHook("/nnti/ib/connect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        connect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ib/disconnect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        disconnect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ib/stats", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        stats_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ib/peers", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        peers_cb(args, results);
    });
}

void
ibverbs_transport::unregister_webhook_cb(void)
{
    webhook::Server::deregisterHook("/nnti/ib/connect");
    webhook::Server::deregisterHook("/nnti/ib/disconnect");
    webhook::Server::deregisterHook("/nnti/ib/stats");
    webhook::Server::deregisterHook("/nnti/ib/peers");
}

NNTI_result_t
ibverbs_transport::create_send_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ibverbs_cmd_op   **cmd_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_send_op() - enter");

    if (work_id->wr().flags() & NNTI_OF_ZERO_COPY) {
        *cmd_op = new nnti::core::ibverbs_cmd_op(this, work_id);
    } else {
        if (cmd_op_freelist_->pop(*cmd_op)) {
            (*cmd_op)->set(work_id);
        } else {
            *cmd_op = new nnti::core::ibverbs_cmd_op(this, cmd_msg_size_, work_id);
        }
    }

    (*cmd_op)->index = op_vector_.add(*cmd_op);
    (*cmd_op)->src_op_id((*cmd_op)->index);

    log_debug("ibverbs_transport", "(*cmd_op)->index=%u", (*cmd_op)!=nullptr ? (*cmd_op)->index : UINT_MAX);

    log_debug("ibverbs_transport", "create_send_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::execute_cmd_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::ibverbs_cmd_op   *cmd_op)
{
    NNTI_result_t rc = NNTI_OK;
    struct ibv_send_wr *bad_wr=nullptr;

    log_debug("ibverbs_transport", "execute_cmd_op() - enter");

    log_debug("ibverbs_transport", "looking up connection for peer pid=%016lX", work_id->wr().peer_pid());

    nnti::datatype::nnti_peer      *peer = (nnti::datatype::nnti_peer *)work_id->wr().peer();
    nnti::core::ibverbs_connection *conn = (nnti::core::ibverbs_connection *)peer->conn();

    print_send_wr(cmd_op->sq_wr());

    log_debug("ibverbs_transport", "posting cmd_op(%s)", cmd_op->toString().c_str());
    if (ibv_post_send(conn->cmd_qp(), cmd_op->sq_wr(), &bad_wr)) {
        log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
        rc = NNTI_EIO;
    }

    log_debug("ibverbs_transport", "execute_cmd_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::create_ack_op(
    uint32_t                     src_op_id,
    nnti::core::ibverbs_cmd_op **cmd_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_ack_op() - enter");

    if (!cmd_op_freelist_->pop(*cmd_op)) {
        *cmd_op = new nnti::core::ibverbs_cmd_op(this, nnti::core::ibverbs_cmd_msg::header_length());
    }

    (*cmd_op)->set(src_op_id);

    log_debug("ibverbs_transport", "create_ack_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::execute_ack_op(
    nnti::datatype::nnti_peer  *peer,
    nnti::core::ibverbs_cmd_op *cmd_op)
{
    NNTI_result_t rc = NNTI_OK;
    struct ibv_send_wr *bad_wr=nullptr;

    log_debug("ibverbs_transport", "execute_ack_op() - enter");

    log_debug("ibverbs_transport", "looking up connection for peer pid=%016lX", peer->pid());

    nnti::core::ibverbs_connection *conn = (nnti::core::ibverbs_connection *)peer->conn();

    print_send_wr(cmd_op->sq_wr());

    log_debug("ibverbs_transport", "posting cmd_op(%s)", cmd_op->toString().c_str());
    if (ibv_post_send(conn->cmd_qp(), cmd_op->sq_wr(), &bad_wr)) {
        log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
        rc = NNTI_EIO;
    }

    log_debug("ibverbs_transport", "execute_ack_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::create_get_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ibverbs_rdma_op  **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_get_op() - enter");

    if (rdma_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
    } else {
        *rdma_op = new nnti::core::ibverbs_rdma_op(this, work_id);
    }

    log_debug("ibverbs_transport", "create_get_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::create_put_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ibverbs_rdma_op  **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_put_op() - enter");

    if (rdma_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
    } else {
        *rdma_op = new nnti::core::ibverbs_rdma_op(this, work_id);
    }

    log_debug("ibverbs_transport", "create_put_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::execute_rdma_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::ibverbs_rdma_op  *rdma_op)
{
    NNTI_result_t rc = NNTI_OK;
    struct ibv_send_wr *bad_wr=nullptr;

    log_debug("ibverbs_transport", "execute_rdma_op() - enter");

    nnti::datatype::nnti_peer      *peer = (nnti::datatype::nnti_peer *)work_id->wr().peer();
    nnti::core::ibverbs_connection *conn = (nnti::core::ibverbs_connection *)peer->conn();

    print_send_wr(rdma_op->sq_wr());

    log_debug("ibverbs_transport", "posting rdma_op(%s) to rdma_qp(%p)", rdma_op->toString().c_str(), conn->rdma_qp());
    if (ibv_post_send(conn->rdma_qp(), rdma_op->sq_wr(), &bad_wr)) {
        log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
        rc = NNTI_EIO;
    }

    log_debug("ibverbs_transport", "execute_rdma_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::create_fadd_op(
    nnti::datatype::nnti_work_id   *work_id,
    nnti::core::ibverbs_atomic_op **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_fadd_op() - enter");

    if (atomic_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
    } else {
        *atomic_op = new nnti::core::ibverbs_atomic_op(this, work_id);
    }

    log_debug("ibverbs_transport", "create_fadd_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::create_cswap_op(
    nnti::datatype::nnti_work_id   *work_id,
    nnti::core::ibverbs_atomic_op **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ibverbs_transport", "create_cswap_op() - enter");

    if (atomic_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
    } else {
        *atomic_op = new nnti::core::ibverbs_atomic_op(this, work_id);
    }

    log_debug("ibverbs_transport", "create_cswap_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::execute_atomic_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ibverbs_atomic_op *atomic_op)
{
    NNTI_result_t rc = NNTI_OK;
    struct ibv_send_wr *bad_wr=nullptr;

    log_debug("ibverbs_transport", "execute_atomic_op() - enter");

    nnti::datatype::nnti_peer      *peer = (nnti::datatype::nnti_peer *)work_id->wr().peer();
    nnti::core::ibverbs_connection *conn = (nnti::core::ibverbs_connection *)peer->conn();

    print_send_wr(atomic_op->sq_wr());

    log_debug("ibverbs_transport", "posting atomic_op(%s)", atomic_op->toString().c_str());
    if (ibv_post_send(conn->rdma_qp(), atomic_op->sq_wr(), &bad_wr)) {
        log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
        rc = NNTI_EIO;
    }

    log_debug("ibverbs_transport", "execute_atomic_op() - exit");

    return rc;
}

NNTI_result_t
ibverbs_transport::process_comp_channel_event(
    struct ibv_comp_channel *comp_channel,
    struct ibv_cq           *cq)
{
    NNTI_result_t rc=NNTI_OK;

    int retries_left=3;

    struct ibv_cq *ev_cq;
    void          *ev_ctx;


    log_debug("process_comp_channel_event", "enter");

try_again:
    if (ibv_get_cq_event(comp_channel, &ev_cq, &ev_ctx) == 0) {
        log_debug("process_comp_channel_event", "got event from comp_channel=%p for cq=%p", comp_channel, ev_cq);
        ibv_ack_cq_events(ev_cq, 1);
        log_debug("process_comp_channel_event", "ACKed event on cq=%p", ev_cq);
        rc = NNTI_OK;
    } else {
        if (errno == EAGAIN) {
            if (retries_left > 0) {
                retries_left--;
                goto try_again;
            } else {
                rc = NNTI_EAGAIN;
                goto cleanup;
            }
        }
        log_error("process_comp_channel_event", "ibv_get_cq_event failed (ev_cq==%p): %s",
                ev_cq, strerror(errno));
        rc = NNTI_EIO;
        goto cleanup;
    }

cleanup:
    log_debug("process_comp_channel_event", "exit");

    return(rc);
}

#define CMD_CQ_SOCKET_INDEX     0
#define RDMA_CQ_SOCKET_INDEX    1
#define INTERRUPT_PIPE_INDEX    2
#define FD_COUNT 3

NNTI_result_t
ibverbs_transport::poll_fds(void)
{
    NNTI_result_t rc=NNTI_OK;
    int poll_rc=0;
    struct pollfd my_pollfd[FD_COUNT];

    log_debug("poll_fds", "this is the progress thread");

    my_pollfd[CMD_CQ_SOCKET_INDEX].fd       = cmd_comp_channel_->fd;
    my_pollfd[CMD_CQ_SOCKET_INDEX].events   = POLLIN;
    my_pollfd[CMD_CQ_SOCKET_INDEX].revents  = 0;
    my_pollfd[RDMA_CQ_SOCKET_INDEX].fd      = rdma_comp_channel_->fd;
    my_pollfd[RDMA_CQ_SOCKET_INDEX].events  = POLLIN;
    my_pollfd[RDMA_CQ_SOCKET_INDEX].revents = 0;
    my_pollfd[INTERRUPT_PIPE_INDEX].fd      = interrupt_pipe_[0];
    my_pollfd[INTERRUPT_PIPE_INDEX].events  = POLLIN;
    my_pollfd[INTERRUPT_PIPE_INDEX].revents = 0;

    // Test for errno==EINTR to deal with timing interrupts from HPCToolkit
    do {
        poll_rc = poll(&my_pollfd[0], FD_COUNT, 100);
    } while ((poll_rc < 0) && (errno == EINTR));

    if (poll_rc == 0) {
        log_debug("poll_fds", "poll() timed out: poll_rc=%d", poll_rc);
        rc = NNTI_ETIMEDOUT;
    } else if (poll_rc < 0) {
        if (errno == EINTR) {
            log_error("poll_fds", "poll() interrupted by signal: poll_rc=%d (%s)", poll_rc, strerror(errno));
            rc = NNTI_EINTR;
        } else if (errno == ENOMEM) {
            log_error("poll_fds", "poll() out of memory: poll_rc=%d (%s)", poll_rc, strerror(errno));
            rc = NNTI_ENOMEM;
        } else {
            log_error("poll_fds", "poll() invalid args: poll_rc=%d (%s)", poll_rc, strerror(errno));
            rc = NNTI_EINVAL;
        }
    } else {
        log_debug("poll_fds", "polled on %d file descriptor(s).  events occurred on %d file descriptor(s).", FD_COUNT, poll_rc);
        log_debug("poll_fds", "poll success: poll_rc=%d ; my_pollfd[CMD_CQ_SOCKET_INDEX].revents=%d",
                poll_rc, my_pollfd[CMD_CQ_SOCKET_INDEX].revents);
        log_debug("poll_fds", "poll success: poll_rc=%d ; my_pollfd[RDMA_CQ_SOCKET_INDEX].revents=%d",
                poll_rc, my_pollfd[RDMA_CQ_SOCKET_INDEX].revents);
        log_debug("poll_fds", "poll success: poll_rc=%d ; my_pollfd[INTERRUPT_PIPE_INDEX].revents=%d",
                poll_rc, my_pollfd[INTERRUPT_PIPE_INDEX].revents);

        if (my_pollfd[CMD_CQ_SOCKET_INDEX].revents == POLLIN) {
            process_comp_channel_event(cmd_comp_channel_, cmd_cq_);
        }
        if (my_pollfd[RDMA_CQ_SOCKET_INDEX].revents == POLLIN) {
            process_comp_channel_event(rdma_comp_channel_, rdma_cq_);
        }
        if (my_pollfd[INTERRUPT_PIPE_INDEX].revents == POLLIN) {
            log_debug("poll_fds", "poll() interrupted by NNTI_ib_interrupt");
            // read all bytes from the pipe
            ssize_t bytes_read=0;
            uint32_t dummy=0;
            do {
                bytes_read=read(interrupt_pipe_[0], &dummy, 4);
                if (dummy != 0xAAAAAAAA) {
                    log_warn("poll_fds", "interrupt byte is %X, should be 0xAAAAAAAA", dummy);
                }
                log_debug("poll_fds", "bytes_read==%lu", (uint64_t)bytes_read);
            } while(bytes_read > 0);
            rc = NNTI_EINTR;
        }
    }

    if (ibv_req_notify_cq(cmd_cq_, 0)) {
        log_error("poll_fds", "Couldn't request CQ notification: %s", strerror(errno));
        rc = NNTI_EIO;
    }
    if (ibv_req_notify_cq(rdma_cq_, 0)) {
        log_error("poll_fds", "Couldn't request CQ notification: %s", strerror(errno));
        rc = NNTI_EIO;
    }

    return rc;
}

NNTI_result_t
ibverbs_transport::poll_cq(
    struct ibv_cq *cq,
    struct ibv_wc *wc)
{
    NNTI_result_t nnti_rc = NNTI_OK;
    int           ibv_rc  = 0;

    memset(wc, 0, sizeof(struct ibv_wc));

    log_debug("poll_cq", "polling for 1 work completion on cq=%p", cq);
    ibv_rc = ibv_poll_cq(cq, 1, wc);

    print_wc(wc, false);

    log_debug("poll_cq", "ibv_poll_cq(cq=%p) ibv_rc==%d", cq, ibv_rc);

    if (ibv_rc < 0) {
        // an error occurred
        log_debug("poll_cq", "ibv_poll_cq failed: %d", ibv_rc);
        nnti_rc = NNTI_EIO;
    } else if (ibv_rc == 0) {
        // no work completions in the queue
        nnti_rc = NNTI_ENOENT;
    } else  if (ibv_rc > 0) {
        // got a work completion
        log_debug("poll_cq", "got wc from cq=%p", cq);
        log_debug("poll_cq", "polling status is %s", ibv_wc_status_str(wc->status));

        if (wc->status != IBV_WC_SUCCESS) {
            log_error("poll_cq", "Failed status %s (%d) for wr_id %lx",
                    ibv_wc_status_str(wc->status),
                    wc->status, wc->wr_id);
            nnti_rc = NNTI_EIO;
        }
    }

    return nnti_rc;
}

NNTI_result_t
ibverbs_transport::poll_cmd_cq(void)
{
    NNTI_result_t nnti_rc = NNTI_OK;
    int           ibv_rc  = 0;

    struct ibv_wc wc;

    nnti_rc = poll_cq(cmd_cq_, &wc);
    if (nnti_rc == NNTI_OK) {
        // found a work completion
        if (wc.opcode & IBV_WC_RECV) {
            nnti::core::ibverbs_cmd_msg *cmd_msg = (nnti::core::ibverbs_cmd_msg *)wc.wr_id;
            cmd_msg->unpack();
            if (cmd_msg->ack()) {
                log_debug("poll_cmd_cqs", "ACK received");

                nnti::core::ibverbs_cmd_op *cmd_op = (nnti::core::ibverbs_cmd_op*)op_vector_.at(cmd_msg->src_op_id());

                nnti::datatype::nnti_work_request &wr             = cmd_op->wid()->wr();
                nnti::datatype::nnti_event_queue  *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
                nnti::datatype::nnti_event_queue  *buf_q          = nullptr;
                NNTI_event_t                      *e              = create_event(cmd_op);
                bool                               event_complete = false;
                bool                               release_event  = true;

                log_debug("poll_cmd_cqs", "considering WR callback");
                if (wr.invoke_cb(e) == NNTI_OK) {
                    event_complete = true;
                }
                log_debug("poll_cmd_cqs", "considering alt EQ callback");
                if (!event_complete && alt_q && alt_q->invoke_cb(e) == NNTI_OK) {
                    event_complete = true;
                }
                log_debug("poll_cmd_cqs", "considering buf EQ callback");
                if (!event_complete) {
                    nnti::datatype::nnti_buffer *b = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());

                    buf_q = nnti::datatype::nnti_event_queue::to_obj(b->eq());
                    if (buf_q && buf_q->invoke_cb(e) == NNTI_OK) {
                        event_complete = true;
                    }
                }
                log_debug("poll_cmd_cqs", "considering alt EQ push");
                if (!event_complete && alt_q) {
                    alt_q->push(e);
                    alt_q->notify();
                    event_complete = true;
                    release_event = false;
                }
                log_debug("poll_cmd_cqs", "considering buf EQ push");
                if (!event_complete && buf_q) {
                    buf_q->push(e);
                    buf_q->notify();
                    event_complete = true;
                    release_event = false;
                }
                if (release_event) {
                    event_freelist_->push(e);
                }

                op_vector_.remove(cmd_op->index);
                cmd_op_freelist_->push(cmd_op);

                NNTI_FAST_STAT(stats_->long_sends++;)

                if (wr.remote_hdl() == NNTI_INVALID_HANDLE) {
                    NNTI_FAST_STAT(stats_->unexpected_sends++;)
                }

                cmd_msg->post_recv();

            } else {
                if (cmd_msg->unexpected()) {
                    log_debug("poll_cmd_cqs", "unexpected received");

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
                    log_debug("poll_cmd_cqs", "expected received");

                    nnti::datatype::ibverbs_buffer   *tgt_buf       = cmd_msg->target_buffer();
                    assert(tgt_buf != nullptr);
                    nnti::datatype::nnti_event_queue *q             = nnti::datatype::nnti_event_queue::to_obj(tgt_buf->eq());
                    NNTI_event_t                     *e             = nullptr;
                    bool                              release_event = true;
                    uint64_t                          actual_offset = 0;

                    if (cmd_msg->eager()) {
                        log_debug("poll_cmd_cqs", "expected eager received");

                        nnti_rc = tgt_buf->copy_in(cmd_msg->target_offset(), cmd_msg->eager_payload(), cmd_msg->payload_length(), &actual_offset);

                        if (nnti_rc == NNTI_ENOMEM) {

                        } else if (nnti_rc != NNTI_OK) {

                        } else {

                        }

                        e  = create_event(cmd_msg, actual_offset);
                        if (tgt_buf->invoke_cb(e) != NNTI_OK) {
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
                        NNTI_FAST_STAT(stats_->short_recvs++;)
                    } else {
                        log_debug("poll_cmd_cqs", "expected long received");

                        nnti::datatype::ibverbs_peer   *peer     = cmd_msg->initiator_peer();
                        nnti::core::ibverbs_connection *conn     = (nnti::core::ibverbs_connection *)peer->conn();
                        nnti::datatype::ibverbs_buffer *init_buf = cmd_msg->initiator_buffer();
                        struct ibv_send_wr sq_wr, *bad_wr;
                        struct ibv_sge     sge;

                        sq_wr.wr_id      = (uint64_t)cmd_msg;
                        sq_wr.next       = nullptr;
                        sq_wr.sg_list    = &sge;
                        sq_wr.num_sge    = 1;
                        sq_wr.opcode     = IBV_WR_RDMA_READ;
                        sq_wr.send_flags = IBV_SEND_SIGNALED;

                        sq_wr.wr.rdma.remote_addr = (uint64_t)init_buf->payload() + cmd_msg->initiator_offset();
                        sq_wr.wr.rdma.rkey        = init_buf->rkey();

                        sge.addr   = (uint64_t)tgt_buf->payload() + cmd_msg->target_offset();
                        sge.length = cmd_msg->payload_length();
                        sge.lkey   = tgt_buf->lkey();

                        print_send_wr(&sq_wr);

                        if (ibv_post_send(conn->long_get_qp(), &sq_wr, &bad_wr)) {
                            log_error("ibverbs_transport", "failed to post send: %s", strerror(errno));
                            nnti_rc = NNTI_EIO;
                        }

                        struct ibv_wc long_get_wc;
                        while ((nnti_rc = poll_cq(long_get_cq_, &long_get_wc)) == NNTI_ENOENT) {
                            log_debug("ibverbs_transport", "long get not done yet");
                        }
                        if (nnti_rc != NNTI_OK) {
                            log_error("ibverbs_transport", "long get failed");
                        }

                        log_debug("poll_cmd_cqs", "sending ACK");
                        nnti::core::ibverbs_cmd_op *ack_op  = nullptr;
                        nnti_rc = create_ack_op(cmd_msg->src_op_id(), &ack_op);
                        nnti_rc = execute_ack_op(peer, ack_op);
                        log_debug("poll_cmd_cqs", "ACK sent");

                        e  = create_event(cmd_msg);
                        if (tgt_buf->invoke_cb(e) != NNTI_OK) {
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
                        NNTI_FAST_STAT(stats_->long_recvs++;)
                    }
                }
            }
        }
        if (wc.opcode == IBV_WC_SEND) {
            bool need_event = true;

            // wr_id is the address of the cmd_op that issued the send
            nnti::core::ibverbs_cmd_op *cmd_op = (nnti::core::ibverbs_cmd_op *)wc.wr_id;

            if (cmd_op->ack()) {
                log_debug("poll_cmd_cqs", "ACK send complete");
                cmd_op_freelist_->push(cmd_op);
                NNTI_FAST_STAT(stats_->ack_sends++;)
            } else if (!cmd_op->eager()) {
                // this is a long send.  wait for the ACK.
                log_debug("poll_cmd_cqs", "long send complete");
            } else {
                log_debug("poll_cmd_cqs", "eager send complete");
                nnti::datatype::nnti_work_request &wr             = cmd_op->wid()->wr();
                nnti::datatype::nnti_event_queue  *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
                nnti::datatype::nnti_event_queue  *buf_q          = nullptr;
                NNTI_event_t                      *e              = create_event(cmd_op);
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
                    event_freelist_->push(e);
                }

                if ((wr.flags() & NNTI_OF_ZERO_COPY)) {
                    delete cmd_op;
                } else {
                    cmd_op_freelist_->push(cmd_op);
                }

                NNTI_FAST_STAT(stats_->short_sends++;)

                if (wr.remote_hdl() == NNTI_INVALID_HANDLE) {
                    NNTI_FAST_STAT(stats_->unexpected_sends++;)
                }
            }
        }
        if (wc.opcode == IBV_WC_RDMA_READ) {
            log_debug("poll_cmd_cqs", "long send GET complete");
            // if there is a READ event on the cmd_qp, then this is a long send/recv.

            // wr_id is the address of the cmd_op that issued the get
            nnti::core::ibverbs_cmd_msg *cmd_msg = (nnti::core::ibverbs_cmd_msg *)wc.wr_id;

            nnti::datatype::nnti_buffer      *b             = cmd_msg->target_buffer();
            nnti::datatype::nnti_event_queue *q             = nnti::datatype::nnti_event_queue::to_obj(b->eq());
            NNTI_event_t                     *e             = nullptr;
            bool                              release_event = true;

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
        }
    }

    return nnti_rc;
}

NNTI_result_t
ibverbs_transport::poll_rdma_cq(void)
{
    NNTI_result_t nnti_rc = NNTI_OK;
    int           ibv_rc  = 0;

    struct ibv_wc wc;

    nnti_rc = poll_cq(rdma_cq_, &wc);
    if (nnti_rc == NNTI_OK) {
        // found a work completion
        if (wc.opcode == IBV_WC_RDMA_WRITE) {
            bool need_event = true;

            // wr_id is the address of the rdma_op that issued the put
            nnti::core::ibverbs_rdma_op         *rdma_op        = (nnti::core::ibverbs_rdma_op *)wc.wr_id;
            nnti::datatype::nnti_work_request   &wr             = rdma_op->wid()->wr();
            nnti::datatype::nnti_event_queue    *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
            nnti::datatype::nnti_event_queue    *buf_q          = nullptr;
            NNTI_event_t                        *e              = create_event(rdma_op);
            bool                                 event_complete = false;
            bool                                 release_event  = true;

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
                event_freelist_->push(e);
            }

            rdma_op_freelist_->push(rdma_op);

            NNTI_FAST_STAT(stats_->puts++;)
        }
        if (wc.opcode == IBV_WC_RDMA_READ) {
            bool need_event = true;

            // wr_id is the address of the rdma_op that issued the get
            nnti::core::ibverbs_rdma_op         *rdma_op        = (nnti::core::ibverbs_rdma_op *)wc.wr_id;
            nnti::datatype::nnti_work_request   &wr             = rdma_op->wid()->wr();
            nnti::datatype::nnti_event_queue    *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
            nnti::datatype::nnti_event_queue    *buf_q          = nullptr;
            NNTI_event_t                        *e              = create_event(rdma_op);
            bool                                 event_complete = false;
            bool                                 release_event  = true;

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
                event_freelist_->push(e);
            }

            rdma_op_freelist_->push(rdma_op);

            NNTI_FAST_STAT(stats_->gets++;)
        }
        if (wc.opcode == IBV_WC_FETCH_ADD) {
            bool need_event = true;

            // wr_id is the address of the rdma_op that issued the get
            nnti::core::ibverbs_atomic_op       *atomic_op      = (nnti::core::ibverbs_atomic_op *)wc.wr_id;
            nnti::datatype::nnti_work_request   &wr             = atomic_op->wid()->wr();
            nnti::datatype::nnti_event_queue    *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
            nnti::datatype::nnti_event_queue    *buf_q          = nullptr;
            NNTI_event_t                        *e              = create_event(atomic_op);
            bool                                 event_complete = false;
            bool                                 release_event  = true;

#if NNTI_HAVE_IBV_EXP_ATOMIC_HCA_REPLY_BE
            if (byte_swap_atomic_result_) {
                nnti::datatype::ibverbs_work_request *ibwr = (nnti::datatype::ibverbs_work_request *)&wr;
                uint64_t *result = (uint64_t*)((uintptr_t)ibwr->local_addr() + ibwr->local_offset());
                log_debug("ibverbs_transport", "original result = %ld", *result);
                *result = nnti::util::betoh64(*result);
                log_debug("ibverbs_transport", "swapped result = %ld", *result);
            }
#endif

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
                event_freelist_->push(e);
            }

            atomic_op_freelist_->push(atomic_op);

            NNTI_FAST_STAT(stats_->fadds++;)
        }
        if (wc.opcode == IBV_WC_COMP_SWAP) {
            bool need_event = true;

            // wr_id is the address of the rdma_op that issued the get
            nnti::core::ibverbs_atomic_op       *atomic_op      = (nnti::core::ibverbs_atomic_op *)wc.wr_id;
            nnti::datatype::nnti_work_request   &wr             = atomic_op->wid()->wr();
            nnti::datatype::nnti_event_queue    *alt_q          = nnti::datatype::nnti_event_queue::to_obj(wr.alt_eq());
            nnti::datatype::nnti_event_queue    *buf_q          = nullptr;
            NNTI_event_t                        *e              = create_event(atomic_op);
            bool                                 event_complete = false;
            bool                                 release_event  = true;

#if NNTI_HAVE_IBV_EXP_ATOMIC_HCA_REPLY_BE
            if (byte_swap_atomic_result_) {
                nnti::datatype::ibverbs_work_request *ibwr = (nnti::datatype::ibverbs_work_request *)&wr;
                uint64_t *result = (uint64_t*)((uintptr_t)ibwr->local_addr() + ibwr->local_offset());
                log_debug("ibverbs_transport", "original result = %ld", *result);
                *result = nnti::util::betoh64(*result);
                log_debug("ibverbs_transport", "swapped result = %ld", *result);
            }
#endif

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
                event_freelist_->push(e);
            }

            atomic_op_freelist_->push(atomic_op);

            NNTI_FAST_STAT(stats_->cswaps++;)
        }
    }

    return nnti_rc;
}

NNTI_event_t *
ibverbs_transport::create_event(
    nnti::core::ibverbs_cmd_msg *cmd_msg,
    uint64_t                     offset)
{
    NNTI_event_t *e = nullptr;

    log_debug("ibverbs_transport", "create_event(cmd_msg, offset) - enter");

    if (!event_freelist_->pop(e)) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = NNTI_OP_SEND;
    e->peer       = nnti::datatype::nnti_peer::to_hdl(cmd_msg->initiator_peer());
    log_debug("ibverbs_transport", "e->peer = %p", e->peer);
    e->length     = cmd_msg->payload_length();

    if (cmd_msg->unexpected()) {
        log_debug("ibverbs_transport", "creating unexpected event");
        e->type       = NNTI_EVENT_UNEXPECTED;
        e->start      = nullptr;
        e->offset     = 0;
        e->context    = 0;
    } else {
        log_debug("ibverbs_transport", "creating eager event");
        e->type       = NNTI_EVENT_RECV;
        e->start      = cmd_msg->target_buffer()->payload();
        e->offset     = offset;
        e->context    = 0;
    }

    log_debug("ibverbs_transport", "create_event(cmd_msg, offset) - exit");

    return(e);
}

NNTI_event_t *
ibverbs_transport::create_event(
    nnti::core::ibverbs_cmd_msg *cmd_msg)
{
    NNTI_event_t *e = nullptr;

    log_debug("ibverbs_transport", "create_event(cmd_msg) - enter");

    e = create_event(cmd_msg, cmd_msg->target_offset());

    log_debug("ibverbs_transport", "create_event(cmd_msg) - exit");

    return(e);
}

NNTI_event_t *
ibverbs_transport::create_event(
    nnti::core::ibverbs_cmd_op *cmd_op)
{
    nnti::datatype::nnti_work_id            *wid = cmd_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    NNTI_event_t                            *e   = nullptr;

    log_debug("ibverbs_transport", "create_event(cmd_op) - enter");

    if (!event_freelist_->pop(e)) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = wr.op();
    e->peer       = wr.peer();
    e->length     = wr.length();

    e->type       = NNTI_EVENT_SEND;
    e->start      = nullptr;
    e->offset     = 0;
    e->context    = 0;

    log_debug("ibverbs_transport", "create_event(cmd_op) - exit");

    return(e);
}

NNTI_event_t *
ibverbs_transport::create_event(
    nnti::core::ibverbs_rdma_op *rdma_op)
{
    nnti::datatype::nnti_work_id            *wid = rdma_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    nnti::datatype::nnti_buffer             *b   = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
    NNTI_event_t                            *e   = nullptr;

    log_debug("ibverbs_transport", "create_event(rdma_op) - enter");

    if (!event_freelist_->pop(e)) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = wr.op();
    e->peer       = wr.peer();
    e->length     = wr.length();
    e->start      = b->payload();
    e->offset     = wr.local_offset();
    e->context    = wr.event_context();

    if (wr.op() == NNTI_OP_PUT) {
        e->type = NNTI_EVENT_PUT;
    }
    if (wr.op() == NNTI_OP_GET) {
        e->type = NNTI_EVENT_GET;
    }

    log_debug("ibverbs_transport", "create_event(rdma_op) - exit");

    return(e);
}

NNTI_event_t *
ibverbs_transport::create_event(
    nnti::core::ibverbs_atomic_op *atomic_op)
{
    nnti::datatype::nnti_work_id            *wid = atomic_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    NNTI_event_t                            *e   = nullptr;

    log_debug("ibverbs_transport", "create_event(atomic_op) - enter");

    if (!event_freelist_->pop(e)) {
        e = new NNTI_event_t;
    }

    e->trans_hdl  = nnti::transports::transport::to_hdl(this);
    e->result     = NNTI_OK;
    e->op         = wr.op();
    e->peer       = wr.peer();
    e->length     = wr.length();

    if (wr.op() == NNTI_OP_ATOMIC_FADD) {
        e->type = NNTI_EVENT_ATOMIC;
    }
    if (wr.op() == NNTI_OP_ATOMIC_CSWAP) {
        e->type = NNTI_EVENT_ATOMIC;
    }

    e->start      = nullptr;
    e->offset     = 0;
    e->context    = 0;

    log_debug("ibverbs_transport", "create_event(atomic_op) - exit");

    return(e);
}

nnti::datatype::nnti_buffer *
ibverbs_transport::unpack_buffer(
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_buffer_t                hdl;
    nnti::datatype::nnti_buffer *b = nullptr;

    this->dt_unpack(&hdl, packed_buf, packed_len);
    b = nnti::datatype::nnti_buffer::to_obj(hdl);

    nnti::datatype::nnti_buffer *found = buffer_map_.get(b->payload());
    if ((found == nullptr) || (b->id() != found->id())) {
        log_debug("ibverbs_transport", "unpack_buffer() - buffer not found in buffer_map_ for address=%p", b->payload());
        // if the buffer is not in the map, then use b.
        found = b;
    } else {
        // if the buffer is in the map, then delete b.
        delete b;
    }

    return found;
}

void
ibverbs_transport::print_wc(
    const struct ibv_wc *wc,
    bool force)
{
    if (force) {
        log_debug("print_wc", "wc=%p, wc.opcode=%d, wc.flags=%d, wc.status=%d (%s), wc.wr_id=%lx, wc.vendor_err=%u, wc.byte_len=%u, wc.qp_num=%u, wc.imm_data=%x, wc.src_qp=%u",
            wc,
            wc->opcode,
            wc->wc_flags,
            wc->status,
            ibv_wc_status_str(wc->status),
            wc->wr_id,
            wc->vendor_err,
            wc->byte_len,
            wc->qp_num,
            wc->imm_data,
            wc->src_qp);
    } else if ((wc->status != IBV_WC_SUCCESS) && (wc->status != IBV_WC_RNR_RETRY_EXC_ERR)) {
        log_error("print_wc", "wc=%p, wc.opcode=%d, wc.flags=%d, wc.status=%d (%s), wc.wr_id=%lx, wc.vendor_err=%u, wc.byte_len=%u, wc.qp_num=%u, wc.imm_data=%x, wc.src_qp=%u",
            wc,
            wc->opcode,
            wc->wc_flags,
            wc->status,
            ibv_wc_status_str(wc->status),
            wc->wr_id,
            wc->vendor_err,
            wc->byte_len,
            wc->qp_num,
            wc->imm_data,
            wc->src_qp);
    } else {
        log_debug("print_wc", "wc=%p, wc.opcode=%d, wc.flags=%d, wc.status=%d (%s), wc.wr_id=%lx, wc.vendor_err=%u, wc.byte_len=%u, wc.qp_num=%u, wc.imm_data=%x, wc.src_qp=%u",
            wc,
            wc->opcode,
            wc->wc_flags,
            wc->status,
            ibv_wc_status_str(wc->status),
            wc->wr_id,
            wc->vendor_err,
            wc->byte_len,
            wc->qp_num,
            wc->imm_data,
            wc->src_qp);
    }
}

void
ibverbs_transport::print_send_wr(
    const struct ibv_send_wr *wr)
{
    log_debug("print_wr", "wr=%p, wr.opcode=%d, wr.send_flags=%d, wr.wr_id=%lx, wr.next=%p, wr.num_sge=%d, wr.rdma.remote_addr=%lu, wr.sge.rkey=%X, wr.sge.addr=%lu, wr.sge.length=%u, wr.sge.lkey=%X",
        wr,
        wr->opcode,
        wr->send_flags,
        wr->wr_id,
        wr->next,
        wr->num_sge,
        wr->wr.rdma.remote_addr,
        wr->wr.rdma.rkey,
        wr->sg_list[0].addr,
        wr->sg_list[0].length,
        wr->sg_list[0].lkey);
}

} /* namespace transports */
} /* namespace nnti */
