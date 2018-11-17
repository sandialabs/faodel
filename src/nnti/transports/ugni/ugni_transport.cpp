// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nnti_pch.hpp"

#include <fcntl.h>
#include <limits.h>
#include <sys/poll.h>

#include <atomic>
#include <thread>
#include <map>
#include <vector>

#include "faodel-common/Configuration.hh"

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

#include "nnti/transports/ugni/ugni_connection.hpp"

#include "nnti/transports/ugni/ugni_atomic_op.hpp"
#include "nnti/transports/ugni/ugni_buffer.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/transports/ugni/ugni_cmd_tgt.hpp"
#include "nnti/transports/ugni/ugni_cmd_op.hpp"
#include "nnti/transports/ugni/ugni_peer.hpp"
#include "nnti/transports/ugni/ugni_rdma_op.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"

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
ugni_transport::ugni_transport(
    faodel::Configuration &config)
    : base_transport(NNTI_TRANSPORT_UGNI,
                     config),
      started_(false),
      event_freelist_size_(128),
      cmd_op_freelist_size_(128),
      cmd_tgt_freelist_size_(128),
      rdma_op_freelist_size_(128),
      atomic_op_freelist_size_(128)
{
    faodel::rc_t rc = 0;
    uint64_t uint_value = 0;

    nthread_lock_init(&new_connection_lock_);

    me_ = nnti::datatype::ugni_peer(this, url_);

    rc = config.GetUInt(&uint_value, "nnti.freelist.size", "128");
    if (rc == 0) {
        event_freelist_size_     = uint_value;
        cmd_op_freelist_size_    = uint_value;
        cmd_tgt_freelist_size_   = uint_value;
        rdma_op_freelist_size_   = uint_value;
        atomic_op_freelist_size_ = uint_value;
    }
    event_freelist_     = new nnti::core::nnti_freelist<NNTI_event_t*>(event_freelist_size_);
    cmd_op_freelist_    = new nnti::core::nnti_freelist<nnti::core::ugni_cmd_op*>(cmd_op_freelist_size_);
    cmd_tgt_freelist_   = new nnti::core::nnti_freelist<nnti::core::ugni_cmd_tgt*>(cmd_tgt_freelist_size_);
    rdma_op_freelist_   = new nnti::core::nnti_freelist<nnti::core::ugni_rdma_op*>(rdma_op_freelist_size_);
    atomic_op_freelist_ = new nnti::core::nnti_freelist<nnti::core::ugni_atomic_op*>(atomic_op_freelist_size_);

    return;
}

/**
 * @brief Deactivates a specific transport.
 *
 * \return A result code (NNTI_OK or an error)
 */
ugni_transport::~ugni_transport()
{
    nthread_lock_fini(&new_connection_lock_);

    return;
}

NNTI_result_t
ugni_transport::start(void)
{
    NNTI_result_t rc    = NNTI_OK;
    gni_return_t gni_rc = GNI_RC_SUCCESS;

    uint32_t nic_addr  =0;
    uint32_t gni_cpu_id=0;

    faodel::nodeid_t nodeid;
    std::string addr;
    std::string port;

    log_debug("ugni_transport", "enter");

    nthread_lock_init(&ugni_lock_);

    log_debug("ugni_transport", "initializing libugni");

    get_drc_info(&drc_info_);

    gni_rc = GNI_CdmGetNicAddress (drc_info_.device_id, &nic_addr, &gni_cpu_id);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CdmGetNicAddress() failed: %d", gni_rc);
        if (gni_rc==GNI_RC_NO_MATCH)
            rc=NNTI_EEXIST;
        else
            rc=NNTI_EINVAL;
        goto cleanup;
    }

    instance_=getpid();
    log_debug("ugni_transport", "nic_addr(%llu), gni_cpu_id(%llu)",
            (uint64_t)nic_addr, (uint64_t)gni_cpu_id);

    log_debug("ugni_transport", "global_nic_hdl - host(%s) device_id(%llu) cookie(%llu) ptag(%llu) "
                "apid_(%llu) inst_id(%llu) gni_nic_addr(%llu) gni_cpu_id(%llu)",
                listen_name_,
                (unsigned long long)drc_info_.device_id,
                (unsigned long long)drc_info_.cookie1,
                (unsigned long long)drc_info_.ptag1,
                (unsigned long long)apid_,
                (unsigned long long)instance_,
                (unsigned long long)nic_addr,
                (unsigned long long)gni_cpu_id);

    gni_rc = GNI_CdmCreate(
            instance_,
            drc_info_.ptag1,
            drc_info_.cookie1,
            GNI_CDM_MODE_ERR_NO_KILL | GNI_CDM_MODE_DUAL_EVENTS,
            &cdm_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CdmCreate() failed: %d", gni_rc);
        rc=NNTI_EINVAL;
        goto cleanup;
    }

    gni_rc = GNI_CdmAttach (
            cdm_hdl_,
            drc_info_.device_id,
            &drc_info_.local_addr,
            &nic_hdl_);

    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CdmAttach() failed: %d", gni_rc);
        if (gni_rc == GNI_RC_PERMISSION_ERROR)
            rc=NNTI_EPERM;
        else
            rc=NNTI_EINVAL;
        goto cleanup;
    }

    gni_rc = GNI_CqCreate (
            nic_hdl_,
            8192,
            0,
            GNI_CQ_BLOCKING,
            NULL,
            NULL,
            &smsg_ep_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(smsg_ep_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_CqCreate (
            nic_hdl_,
            8192,
            0,
            GNI_CQ_BLOCKING,
            NULL,
            NULL,
            &smsg_mem_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(smsg_mem_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }

    gni_rc = GNI_CqCreate (nic_hdl_, 8192, 0, GNI_CQ_BLOCKING, NULL, NULL, &rdma_ep_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(rdma_ep_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_CqCreate (nic_hdl_, 8192, 0, GNI_CQ_BLOCKING, NULL, NULL, &rdma_mem_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(rdma_mem_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }

    gni_rc = GNI_CqCreate (nic_hdl_, 8192, 0, GNI_CQ_BLOCKING, NULL, NULL, &long_get_ep_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(long_get_ep_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_CqCreate (nic_hdl_, 8192, 0, GNI_CQ_BLOCKING, NULL, NULL, &long_get_mem_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(long_get_mem_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }

    gni_rc = GNI_CqCreate (nic_hdl_, 20, 0, GNI_CQ_BLOCKING, NULL, NULL, &interrupt_mem_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(interrupt_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_MemRegister (nic_hdl_,
                        (uint64_t)&interrupt_buf_,
                        sizeof(uint32_t),
                        interrupt_mem_cq_hdl_,
                        GNI_MEM_READWRITE,
                        (uint32_t)-1,
                        &interrupt_mem_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "MemRegister(interrupt_mem_hdl_) failed: rc=%d, %s", gni_rc, strerror(errno));
        goto cleanup;
    }
    gni_rc = GNI_CqCreate (nic_hdl_, 2, 0, GNI_CQ_BLOCKING, NULL, NULL, &interrupt_ep_cq_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "CqCreate(interrupt_cq_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_EpCreate (nic_hdl_, interrupt_ep_cq_hdl_, &interrupt_ep_hdl_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "EpCreate(interrupt_ep_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }
    gni_rc = GNI_EpBind (interrupt_ep_hdl_, drc_info_.local_addr, instance_);
    if (gni_rc != GNI_RC_SUCCESS) {
        log_error("ugni_transport", "EpBind(interrupt_ep_hdl_) failed: %d", gni_rc);
        goto cleanup;
    }

    nodeid = webhook::Server::GetNodeID();
    addr = nodeid.GetIP();
    port = nodeid.GetPort();
    url_ = nnti::core::nnti_url(addr, port);
    me_ = nnti::datatype::ugni_peer(this, url_);
    log_debug_stream("ugni_transport") << "me_ = " << me_.url().url();

    cmd_msg_size_  = 2048;
    cmd_msg_count_ = 64;

    attrs_.mtu                 = cmd_msg_size_;
    attrs_.max_cmd_header_size = nnti::core::ugni_cmd_msg::header_length();
    attrs_.max_eager_size      = attrs_.mtu - attrs_.max_cmd_header_size;
    attrs_.cmd_queue_size      = cmd_msg_count_;
    log_debug("ugni_transport", "attrs_.mtu                =%d", attrs_.mtu);
    log_debug("ugni_transport", "attrs_.max_cmd_header_size=%d", attrs_.max_cmd_header_size);
    log_debug("ugni_transport", "attrs_.max_eager_size     =%d", attrs_.max_eager_size);
    log_debug("ugni_transport", "attrs_.cmd_queue_size     =%d", attrs_.cmd_queue_size);

    rc = setup_freelists();
    if (rc) {
        log_error("ugni_transport", "setup_freelists() failed");
        return NNTI_EIO;
    }

    NNTI_STATS_DATA(
    stats_ = new webhook_stats;
    )

    assert(webhook::Server::IsRunning() && "webhook is not running.  Confirm Bootstrap configuration and try again.");

    register_webhook_cb();

    log_debug("ugni_transport", "url_=%s", url_.url().c_str());

    start_progress_thread();

    log_debug("ugni_transport", "Cray Generic Network Interface (ugni) Initialized");

    started_ = true;

cleanup:
    log_debug("ugni_transport", "exit");

    return NNTI_OK;
}

NNTI_result_t
ugni_transport::stop(void)
{
    NNTI_result_t rc=NNTI_OK;;
    nnti::core::nnti_connection_map_iter_t iter;

    log_debug("ugni_transport", "enter");

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

    teardown_freelists();

    GNI_EpUnbind (interrupt_ep_hdl_);
    GNI_CqDestroy (interrupt_ep_cq_hdl_);
    GNI_EpDestroy (interrupt_ep_hdl_);
    GNI_MemDeregister (nic_hdl_, &interrupt_mem_hdl_);
    GNI_CqDestroy (interrupt_mem_cq_hdl_);

    GNI_CqDestroy (smsg_ep_cq_hdl_);
    GNI_CqDestroy (smsg_mem_cq_hdl_);
    GNI_CqDestroy (long_get_ep_cq_hdl_);
    GNI_CqDestroy (long_get_mem_cq_hdl_);
    GNI_CqDestroy (rdma_ep_cq_hdl_);
    GNI_CqDestroy (rdma_mem_cq_hdl_);

    GNI_CdmDestroy (cdm_hdl_);

    nthread_lock_fini(&ugni_lock_);

cleanup:
    log_debug("ugni_transport", "exit");

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
ugni_transport::initialized(void)
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
ugni_transport::get_url(
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
ugni_transport::pid(NNTI_process_id_t *pid)
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
ugni_transport::attrs(NNTI_attrs_t *attrs)
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
ugni_transport::connect(
    const char  *url,
    const int    timeout,
    NNTI_peer_t *peer_hdl)
{
    nnti::core::nnti_url         peer_url(url);
    nnti::datatype::nnti_peer   *peer = new nnti::datatype::ugni_peer(this, peer_url);
    nnti::core::ugni_connection *conn;

    log_debug("connect", "url=%s", url);

    nthread_lock(&new_connection_lock_);

    // look for an existing connection to reuse
    log_debug("ugni_transport", "Looking for connection with pid=%016lx", peer->pid());
    conn = (nnti::core::ugni_connection*)conn_map_.get(peer->pid());
    if (conn != nullptr) {
        log_debug("ugni_transport", "Found connection with pid=%016lx", peer->pid());
        // reuse an existing connection
        *peer_hdl = (NNTI_peer_t)conn->peer();
        nthread_unlock(&new_connection_lock_);
        return NNTI_OK;
    }
    log_debug("ugni_transport", "Couldn't find connection with pid=%016lx", peer->pid());

    conn = new nnti::core::ugni_connection(this, cmd_msg_size_, cmd_msg_count_);

    peer->conn(conn);
    conn->peer(peer);

    conn->index = conn_vector_.add(conn);
    conn_map_.insert(conn);

    nthread_unlock(&new_connection_lock_);

    std::string reply;
    std::string wh_path = build_webhook_connect_path(conn);
    int wh_rc = 0;
    int retries = 5;
    wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
    while (wh_rc != 0 && --retries) {
        sleep(1);
        wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
    }
    if (wh_rc != 0) {
        log_debug("ugni_transport", "connect() timed out");
        return(NNTI_ETIMEDOUT);
    }

    log_debug_stream("connect") << "reply=" << reply;

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
ugni_transport::disconnect(
    NNTI_peer_t peer_hdl)
{
    nnti::datatype::nnti_peer   *peer     = (nnti::datatype::nnti_peer *)peer_hdl;
    nnti::core::nnti_url        &peer_url = peer->url();
    std::string                  reply;
    nnti::core::nnti_connection *conn     = peer->conn();

    log_debug("ugni_transport", "disconnecting from %s", peer_url.url().c_str());

    nthread_lock(&new_connection_lock_);

    conn = (nnti::core::ugni_connection*)conn_map_.get(peer->pid());
    if (conn==nullptr) {
        log_debug("ugni_transport", "disconnect couldn't find connection to %s. Already disconnected?", peer_url.url().c_str());
        nthread_unlock(&new_connection_lock_);
        return NNTI_EINVAL;
    }

    conn_map_.remove(conn);

    nthread_unlock(&new_connection_lock_);

    if (*peer != me_) {
        std::string wh_path = build_webhook_disconnect_path(conn);
        int wh_rc = webhook::retrieveData(peer_url.hostname(), peer_url.port(), wh_path, &reply);
        if (wh_rc != 0) {
            return(NNTI_ETIMEDOUT);
        }
    }

    log_debug("ugni_transport", "disconnect from %s (pid=%x) succeeded", peer->url(), peer->pid());

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
ugni_transport::eq_create(
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
ugni_transport::eq_create(
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
ugni_transport::eq_destroy(
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
ugni_transport::eq_wait(
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
    log_debug_stream("ugni_transport") << event;
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
ugni_transport::next_unexpected(
    NNTI_buffer_t  dst_hdl,
    uint64_t       dst_offset,
    NNTI_event_t  *result_event)
{
    NNTI_result_t rc = NNTI_OK;
    int           gni_rc;
    uint64_t actual_offset;
    nnti::datatype::nnti_buffer *b = (nnti::datatype::nnti_buffer *)dst_hdl;

    log_debug("next_unexpected", "enter");

    if (unexpected_msgs_.size() == 0) {
        log_debug("ugni_transport", "next_unexpected - unexpected_msgs_ list is empty");
        return NNTI_ENOENT;
    }

    nnti::core::ugni_cmd_tgt *unexpected_msg = unexpected_msgs_.front();
    unexpected_msgs_.pop_front();

    unexpected_msg->unexpected_dst_hdl(dst_hdl);
    unexpected_msg->unexpected_dst_offset(dst_offset);
    if (unexpected_msg->update(nullptr) == 1) {
        result_event->trans_hdl  = nnti::transports::transport::to_hdl(this);
        result_event->result     = NNTI_OK;
        result_event->op         = NNTI_OP_SEND;
        result_event->peer       = nnti::datatype::nnti_peer::to_hdl(unexpected_msg->initiator_peer());
        result_event->length     = unexpected_msg->payload_length();
        result_event->type       = NNTI_EVENT_SEND;
        result_event->start      = b->payload();
        result_event->offset     = unexpected_msg->actual_offset();
        result_event->context    = 0;

        log_debug("next_unexpected", "result_event->peer = %p", result_event->peer);

        cmd_tgt_freelist_->push(unexpected_msg);
    } else {
        rc = NNTI_EIO;
    }

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
ugni_transport::get_unexpected(
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
ugni_transport::event_complete(
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
ugni_transport::dt_unpack(
    void           *nnti_dt,
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_result_t                rc = NNTI_OK;
    nnti::datatype::ugni_buffer *b  = nullptr;
    nnti::datatype::nnti_peer   *p  = nullptr;

    switch (*(NNTI_datatype_t*)packed_buf) {
        case NNTI_dt_buffer:
            log_debug("ugni_transport", "dt is a buffer");
            b = new nnti::datatype::ugni_buffer(this, packed_buf, packed_len);
            *(NNTI_buffer_t*)nnti_dt = nnti::datatype::nnti_buffer::to_hdl(b);
            break;
        case NNTI_dt_peer:
            log_debug("ugni_transport", "dt is a peer");
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
ugni_transport::alloc(
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    char                               **reg_ptr,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::ugni_buffer(this,
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
ugni_transport::free(
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
ugni_transport::register_memory(
    char                                *buffer,
    const uint64_t                       size,
    const NNTI_buffer_flags_t            flags,
    NNTI_event_queue_t                   eq,
    nnti::datatype::nnti_event_callback  cb,
    void                                *cb_context,
    NNTI_buffer_t                       *reg_buf)
{
    nnti::datatype::nnti_buffer *b = new nnti::datatype::ugni_buffer(this,
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
ugni_transport::unregister_memory(
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
ugni_transport::dt_peer_to_pid(
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
ugni_transport::dt_pid_to_peer(
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
ugni_transport::send(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ugni_cmd_op      *cmd_op  = nullptr;

    log_debug("ugni_transport", "send - wr.local_offset=%lu", wr->local_offset());

    rc = create_send_op(work_id, &cmd_op);
    if (rc != NNTI_OK) {
        log_error("ugni_transport", "create_send_op() failed");
        goto cleanup;
    }

    log_debug("ugni_transport", "cmd_op(%p) id(%u)", cmd_op, cmd_op->id());

    rc = execute_cmd_op(work_id, cmd_op);
    if (rc != NNTI_OK) {
        log_error("ugni_transport", "execute_cmd_op() failed");
        goto cleanup;
    }

    *wid = (NNTI_work_id_t)work_id;

cleanup:
    return rc;
}

/**
 * @brief Transfer data to a peer.
 *
 * \param[in]  wr   A work request that describes the operation
 * \param[out] wid  Identifier used to track this work request
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
ugni_transport::put(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ugni_rdma_op     *put_op  = nullptr;

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
ugni_transport::get(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ugni_rdma_op     *get_op  = nullptr;

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
ugni_transport::atomic_fop(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ugni_atomic_op   *atomic_op = nullptr;

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
ugni_transport::atomic_cswap(
    nnti::datatype::nnti_work_request *wr,
    NNTI_work_id_t                    *wid)
{
    NNTI_result_t rc;

    nnti::datatype::nnti_work_id *work_id   = new nnti::datatype::nnti_work_id(*wr);
    nnti::core::ugni_atomic_op   *atomic_op = nullptr;

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
ugni_transport::cancel(
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
ugni_transport::cancelall(
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
ugni_transport::interrupt()
{
    NNTI_result_t rc=NNTI_OK;
    int gni_rc=GNI_RC_SUCCESS;

    uint32_t  dummy=0xAAAAAAAA;
    gni_post_descriptor_t  post_desc;
    gni_post_descriptor_t *post_desc_ptr;
    gni_cq_entry_t   ev_data;

    log_debug("ugni_transport", "enter");

    memset(&post_desc, 0, sizeof(gni_post_descriptor_t));
    post_desc.type           =GNI_POST_CQWRITE;
    post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT;
    post_desc.dlvr_mode      =GNI_DLVMODE_IN_ORDER;
    post_desc.remote_mem_hndl=interrupt_mem_hdl_;

    *(uint32_t*)&post_desc.cqwrite_value = dummy;

    log_debug("ugni_transport", "calling PostCqWrite(cqwrite_value=%X)", *(uint32_t*)&post_desc.cqwrite_value);
    nthread_lock(&ugni_lock_);
    gni_rc=GNI_PostCqWrite(interrupt_ep_hdl_, &post_desc);
    nthread_unlock(&ugni_lock_);
    if (gni_rc!=GNI_RC_SUCCESS) {
        log_error("ugni_transport", "PostCqWrite(post_desc) failed: %d", rc);
        rc=NNTI_EIO;
        goto cleanup;
    }

    nthread_lock(&ugni_lock_);
    gni_rc=GNI_CqWaitEvent(interrupt_ep_cq_hdl_, -1, &ev_data);
    gni_rc=GNI_GetCompleted(interrupt_ep_cq_hdl_, ev_data, &post_desc_ptr);
    nthread_unlock(&ugni_lock_);
    if (gni_rc!=GNI_RC_SUCCESS) {
        log_error("ugni_transport", "GetCompleted(interrupt(%p)) failed: %d", post_desc_ptr, gni_rc);
    } else {
        log_debug("ugni_transport", "GetCompleted(interrupt(%p)) success", post_desc_ptr);
    }
    print_post_desc(post_desc_ptr);

cleanup:
    log_debug("ugni_transport", "exit");

    return rc;

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
ugni_transport::wait(
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
ugni_transport::waitany(
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
ugni_transport::waitall(
    NNTI_work_id_t *wid_list,
    const uint32_t  wid_count,
    const int64_t   timeout,
    NNTI_status_t  *status)
{
    return NNTI_OK;
}

/*************************************************************/

ugni_transport *
ugni_transport::get_instance(
    faodel::Configuration &config)
{
    static ugni_transport *instance = new ugni_transport(config);
    return instance;
}

/*************************************************************
 * Accessors for data members specific to this interconnect.
 *************************************************************/

//NNTI_result_t
//ugni_transport::setup_command_channel(void)
//{
//    int flags;
//
//    cmd_comp_channel_ = ibv_create_comp_channel(ctx_);
//    if (!cmd_comp_channel_) {
//        log_error("ugni_transport", "ibv_create_comp_channel failed");
//        return NNTI_EIO;
//    }
//    cmd_cq_ = ibv_create_cq(
//            ctx_,
//            cqe_count_,
//            NULL,
//            cmd_comp_channel_,
//            0);
//    if (!cmd_cq_) {
//        log_error("ugni_transport", "ibv_create_cq failed: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//
//    cmd_srq_count_=0;
//
//    struct ibv_srq_init_attr srq_attr;
//    memset(&srq_attr, 0, sizeof(srq_attr)); // initialize to avoid valgrind uninitialized warning
//    srq_attr.attr.max_wr = srq_count_;
//    srq_attr.attr.max_sge = sge_count_;
//
//    cmd_srq_ = ibv_create_srq(pd_, &srq_attr);
//    if (!cmd_srq_)  {
//        log_error("ugni_transport", "ibv_create_srq failed");
//        return NNTI_EIO;
//    }
//
//    if (ibv_req_notify_cq(cmd_cq_, 0)) {
//        log_error("ugni_transport", "ibv_req_notify_cq failed");
//        return NNTI_EIO;
//    }
//
//    /* use non-blocking IO on the async fd and completion fd */
//    flags = fcntl(ctx_->async_fd, F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get async_fd flags");
//        return NNTI_EIO;
//    }
//    if (fcntl(ctx_->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set async_fd to nonblocking");
//        return NNTI_EIO;
//    }
//
//    flags = fcntl(cmd_comp_channel_->fd, F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get completion fd flags");
//        return NNTI_EIO;
//    }
//    if (fcntl(cmd_comp_channel_->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set completion fd to nonblocking");
//        return NNTI_EIO;
//    }
//
//    return(NNTI_OK);
//}
//
//NNTI_result_t
//ugni_transport::setup_rdma_channel(void)
//{
//    int flags;
//
//    rdma_comp_channel_ = ibv_create_comp_channel(ctx_);
//    if (!rdma_comp_channel_) {
//        log_error("ugni_transport", "ibv_create_comp_channel failed");
//        return NNTI_EIO;
//    }
//    rdma_cq_ = ibv_create_cq(
//            ctx_,
//            cqe_count_,
//            NULL,
//            rdma_comp_channel_,
//            0);
//    if (!rdma_cq_) {
//        log_error("ugni_transport", "ibv_create_cq failed");
//        return NNTI_EIO;
//    }
//
//    rdma_srq_count_=0;
//
//    struct ibv_srq_init_attr srq_attr;
//    memset(&srq_attr, 0, sizeof(srq_attr));   // initialize to avoid valgrind warning
//    srq_attr.attr.max_wr  = srq_count_;
//    srq_attr.attr.max_sge = sge_count_;
//
//    rdma_srq_ = ibv_create_srq(pd_, &srq_attr);
//    if (!rdma_srq_)  {
//        log_error("ugni_transport", "ibv_create_srq failed");
//        return NNTI_EIO;
//    }
//
//    if (ibv_req_notify_cq(rdma_cq_, 0)) {
//        log_error("ugni_transport", "ibv_req_notify_cq failed");
//        return NNTI_EIO;
//    }
//
//    /* use non-blocking IO on the async fd and completion fd */
//    flags = fcntl(ctx_->async_fd, F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get async_fd flags");
//        return NNTI_EIO;
//    }
//    if (fcntl(ctx_->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set async_fd to nonblocking");
//        return NNTI_EIO;
//    }
//
//    flags = fcntl(rdma_comp_channel_->fd, F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get completion fd flags");
//        return NNTI_EIO;
//    }
//    if (fcntl(rdma_comp_channel_->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set completion fd to nonblocking");
//        return NNTI_EIO;
//    }
//
//    return(NNTI_OK);
//}
//
//NNTI_result_t
//ugni_transport::setup_interrupt_pipe(void)
//{
//    int rc=0;
//    int flags;
//
//    rc=pipe(interrupt_pipe_);
//    if (rc < 0) {
//        log_error("ugni_transport", "pipe() failed: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//
//    /* use non-blocking IO on the read side of the interrupt pipe */
//    flags = fcntl(interrupt_pipe_[0], F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//    if (fcntl(interrupt_pipe_[0], F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//
//    /* use non-blocking IO on the write side of the interrupt pipe */
//    flags = fcntl(interrupt_pipe_[1], F_GETFL);
//    if (flags < 0) {
//        log_error("ugni_transport", "failed to get interrupt_pipe flags: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//    if (fcntl(interrupt_pipe_[1], F_SETFL, flags | O_NONBLOCK) < 0) {
//        log_error("ugni_transport", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
//        return NNTI_EIO;
//    }
//
//    return(NNTI_OK);
//}

NNTI_result_t
ugni_transport::setup_freelists(void)
{
    for (uint64_t i=0;i<cmd_op_freelist_size_;i++) {
        nnti::core::ugni_cmd_op *op = new nnti::core::ugni_cmd_op(this, cmd_msg_size_);
        cmd_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<rdma_op_freelist_size_;i++) {
        nnti::core::ugni_rdma_op *op = new nnti::core::ugni_rdma_op(this);
        rdma_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<atomic_op_freelist_size_;i++) {
        nnti::core::ugni_atomic_op *op = new nnti::core::ugni_atomic_op(this);
        atomic_op_freelist_->push(op);
    }

    for (uint64_t i=0;i<cmd_tgt_freelist_size_;i++) {
        nnti::core::ugni_cmd_tgt *tgt = new nnti::core::ugni_cmd_tgt(this, cmd_msg_size_);
        cmd_tgt_freelist_->push(tgt);
    }

    for (uint64_t i=0;i<event_freelist_size_;i++) {
        NNTI_event_t *e = new NNTI_event_t;
        event_freelist_->push(e);
    }

    return(NNTI_OK);
}
NNTI_result_t
ugni_transport::teardown_freelists(void)
{
    while(!event_freelist_->empty()) {
        NNTI_event_t *e = nullptr;
        if (event_freelist_->pop(e)) {
            delete e;
        }
    }

    while(!cmd_op_freelist_->empty()) {
        nnti::core::ugni_cmd_op *op = nullptr;
        if (cmd_op_freelist_->pop(op)) {
            if (op->wid()) {
                delete op->wid();
            }
            delete op;
        }
    }

    while(!rdma_op_freelist_->empty()) {
        nnti::core::ugni_rdma_op *op = nullptr;
        if (rdma_op_freelist_->pop(op)) {
            if (op->wid()) {
                delete op->wid();
            }
            delete op;
        }
    }

    while(!atomic_op_freelist_->empty()) {
        nnti::core::ugni_atomic_op *op = nullptr;
        if (atomic_op_freelist_->pop(op)) {
            if (op->wid()) {
                delete op->wid();
            }
            delete op;
        }
    }

    while(!cmd_tgt_freelist_->empty()) {
        nnti::core::ugni_cmd_tgt *tgt = nullptr;
        if (cmd_tgt_freelist_->pop(tgt)) {
            delete tgt;
        }
    }

    return(NNTI_OK);
}

#define SMSG_MEM_CQ_INDEX     0
#define SMSG_EP_CQ_INDEX      1
#define LONG_GET_MEM_CQ_INDEX 2
#define LONG_GET_EP_CQ_INDEX  3
#define RDMA_MEM_CQ_INDEX     4
#define RDMA_EP_CQ_INDEX      5
#define INTERRUPT_CQ_INDEX    6
#define CQ_COUNT 7
void
ugni_transport::progress(void)
{
    int           rc      = 0;
    gni_return_t  gni_rc  = GNI_RC_SUCCESS;
    NNTI_result_t nnti_rc = NNTI_OK;

    bool cq_empty;

    gni_cq_handle_t cq_list[CQ_COUNT];
    uint32_t        which_cq = 0;
    gni_cq_entry_t  ev_data;

    uint32_t cq_count = CQ_COUNT;

    gni_post_descriptor_t *post_desc_ptr = NULL;

    void *header = NULL;

    bool interrupted = false;

    nnti::core::ugni_connection *conn = NULL;

    uint8_t tag = GNI_SMSG_ANY_TAG;

    cq_list[SMSG_EP_CQ_INDEX]      = smsg_ep_cq_hdl_;
    cq_list[SMSG_MEM_CQ_INDEX]     = smsg_mem_cq_hdl_;
    cq_list[LONG_GET_EP_CQ_INDEX]  = long_get_ep_cq_hdl_;
    cq_list[LONG_GET_MEM_CQ_INDEX] = long_get_mem_cq_hdl_;
    cq_list[RDMA_EP_CQ_INDEX]      = rdma_ep_cq_hdl_;
    cq_list[RDMA_MEM_CQ_INDEX]     = rdma_mem_cq_hdl_;
    cq_list[INTERRUPT_CQ_INDEX]    = interrupt_mem_cq_hdl_;

    while (terminate_progress_thread_.load() == false) {
        log_debug("ugni_transport::progress", "this is the progress thread");

        memset(&ev_data, 0, sizeof(gni_cq_entry_t));
        post_desc_ptr=NULL;

        log_debug("ugni_transport", "checking for events on any CQ (cq_count=%d)", cq_count);
        gni_rc=GNI_CqVectorMonitor(cq_list, cq_count, -1, &which_cq);
        /* case 1: success */
        if (gni_rc == GNI_RC_SUCCESS) {

            switch (which_cq) {

                // message received
                case SMSG_MEM_CQ_INDEX:
                    cq_empty=false;
                    while (!cq_empty) {
                        log_debug("ugni_transport", "CqVectorMonitor(smsg_mem_cq_hdl_) SMSG recv complete event received at receiver");
                        gni_rc = get_event(cq_list[which_cq], &ev_data);
                        if (gni_rc == GNI_RC_SUCCESS) {
                            print_cq_event(&ev_data, false);
                            conn=(nnti::core::ugni_connection *)conn_vector_.at(GNI_CQ_GET_INST_ID(ev_data));
                            assert(conn);

                            tag=GNI_SMSG_ANY_TAG;

                            nthread_lock(&ugni_lock_);
                            gni_rc=GNI_SmsgGetNextWTag(conn->mbox_ep_hdl(), &header, &tag);
                            nthread_unlock(&ugni_lock_);
                            if (gni_rc == GNI_RC_SUCCESS) {
                                log_debug("ugni_transport", "GNI_RC_SUCCESS ; tag=%d", tag);
                                if (tag == NNTI_SMSG_TAG_CREDIT) {
                                    credit_msg_t *credit=(credit_msg_t *)header;
                                    log_debug("ugni_transport", "SmsgGetNextWTag(smsg_ep_hdl) SMSG explicit credit event received at receiver: credit_return_msg.inst_id=%llu", credit->inst_id);
                                    nthread_lock(&ugni_lock_);
                                    log_debug("ugni_transport", "calling SmsgRelease(mbox_ep_hdl)");
                                    gni_rc=GNI_SmsgRelease(conn->mbox_ep_hdl());
                                    log_debug("ugni_transport", "called SmsgRelease(mbox_ep_hdl)");
                                    nthread_unlock(&ugni_lock_);
                                    if (gni_rc!=GNI_RC_SUCCESS) log_error("ugni_transport", "SmsgRelease(request) failed: %d", gni_rc);
                                } else if (tag == NNTI_SMSG_TAG_REQUEST) {
                                    log_debug("ugni_transport", "SmsgGetNextWTag(smsg_ep_hdl) SMSG request received (header=%p)", header);

                                    nnti::core::ugni_cmd_tgt *cmd_tgt;
                                    if (cmd_tgt_freelist_->pop(cmd_tgt)) {
                                        cmd_tgt->set((char*)header, 2048, true);
                                    } else {
                                        cmd_tgt = new nnti::core::ugni_cmd_tgt(this, (char*)header, 2048, false);
                                    }

                                    nthread_lock(&ugni_lock_);
                                    log_debug("ugni_transport", "calling SmsgRelease(mbox_ep_hdl)");
                                    gni_rc=GNI_SmsgRelease(conn->mbox_ep_hdl());
                                    log_debug("ugni_transport", "called SmsgRelease(mbox_ep_hdl)");
                                    nthread_unlock(&ugni_lock_);
                                    if (gni_rc!=GNI_RC_SUCCESS) log_error("ugni_transport", "SmsgRelease(request) failed: %d", gni_rc);

                                    if (cmd_tgt->update(nullptr) == 1) {
                                        cmd_tgt_freelist_->push(cmd_tgt);
                                    }
                                } else if (tag == NNTI_SMSG_TAG_LONG_GET_ACK) {
                                    log_debug("ugni_transport", "SmsgGetNextWTag(smsg_ep_hdl) SMSG long get ack (header=%p)", header);

                                    nnti::core::ugni_cmd_tgt *cmd_tgt = new nnti::core::ugni_cmd_tgt(this, (char*)header, 2048, false);

                                    nnti::core::ugni_cmd_op *cmd_op = (nnti::core::ugni_cmd_op*)op_vector_.at(cmd_tgt->src_op_id());

                                    delete cmd_tgt;

                                    nthread_lock(&ugni_lock_);
                                    log_debug("ugni_transport", "calling SmsgRelease(mbox_ep_hdl)");
                                    gni_rc=GNI_SmsgRelease(conn->mbox_ep_hdl());
                                    log_debug("ugni_transport", "called SmsgRelease(mbox_ep_hdl)");
                                    nthread_unlock(&ugni_lock_);
                                    if (gni_rc!=GNI_RC_SUCCESS) log_error("ugni_transport", "SmsgRelease(request) failed: %d", gni_rc);

                                    if (cmd_op->update(nullptr) == 1) {
                                        op_vector_.remove(cmd_op->index);
                                        cmd_op_freelist_->push(cmd_op);
                                    }
                                } else {
                                    log_debug("ugni_transport", "SmsgGetNextWTag(smsg_ep_hdl) SMSG unknown tag: %d", tag);
                                    abort();
                                }
                            } else if (gni_rc == GNI_RC_NO_MATCH) {
                                log_debug("ugni_transport", "GNI_RC_NO_MATCH - didn't match ANY_TAG??  Aborting...");
                                abort();
                            } else if (gni_rc == GNI_RC_NOT_DONE) {
                                log_debug("ugni_transport", "GNI_RC_NOT_DONE means the mailbox is empty - implicit credit event on CQ??");
                                if (conn->waitlisted()) {
                                    // try to send some messages from this connection's wait list
                                    conn->waitlist_execute();
                                }
//                                cq_empty = true;
                            } else {
                                log_debug("ugni_transport", "SmsgGetNextWTag(smsg_ep_hdl) failed: %d", gni_rc);
                                continue;
                            }
                            log_debug("ugni_transport", "goto another_event");
                        } else if (gni_rc == GNI_RC_NOT_DONE) {
                            log_debug("ugni_transport", "GNI_RC_NOT_DONE means the CQ is empty");
                            cq_empty = true;
                        } else {
                            if (GNI_CQ_OVERRUN(ev_data)) {
                                fprintf(stdout, "GNI_CQ_OVERRUN destination, gni_rc: %s\n", gni_err_str[gni_rc] );
                            } else {
                                fprintf(stdout, "GNI_CqGetEvent destination, gni_rc: %s\n", gni_err_str[gni_rc] );
                            }
                        }
                    }
                    break;

                // message sent
                case SMSG_EP_CQ_INDEX:
                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        log_debug("ugni_transport", "CqVectorMonitor(smsg_ep_cq_hdl_) SMSG send complete event received at sender");
                        print_cq_event(&ev_data, false);
                        assert (GNI_CQ_GET_TYPE(ev_data) == GNI_CQ_EVENT_TYPE_SMSG);

                        if (GNI_CQ_GET_INST_ID(ev_data) < 0xFFFFFF) {
                            nnti::core::ugni_cmd_op *cmd_op = (nnti::core::ugni_cmd_op*)op_vector_.at(GNI_CQ_GET_INST_ID(ev_data));
                            conn = (nnti::core::ugni_connection*)cmd_op->target_peer()->conn();
                            assert(conn);
                            if (cmd_op->update(nullptr) == 1) {
                                op_vector_.remove(cmd_op->index);
                                cmd_op_freelist_->push(cmd_op);
                            }
                            if (conn->waitlisted()) {
                                conn->waitlist_execute();
                            }
                        } else {
                            // ignoring message with index == UINT_MAX
                            log_debug("ugni_transport", "ignoring send event with INST_ID == %u", GNI_CQ_GET_INST_ID(ev_data));
                        }
                    }
                    break;

                case LONG_GET_EP_CQ_INDEX:
                    log_debug("ugni_transport", "CqVectorMonitor() - event received on long_get_ep_cq_hdl_");

                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        print_cq_event(&ev_data, false);

                        nnti::core::ugni_cmd_tgt *cmd_tgt = (nnti::core::ugni_cmd_tgt*)msg_vector_.at(GNI_CQ_GET_INST_ID(ev_data));

                        log_debug("ugni_transport", "calling GetComplete(progress)");
                        nthread_lock(&ugni_lock_);
                        gni_rc=GNI_GetCompleted (cq_list[which_cq], ev_data, &post_desc_ptr);
                        nthread_unlock(&ugni_lock_);
                        if (gni_rc!=GNI_RC_SUCCESS) {
                            log_error("ugni_transport", "GetCompleted(progress post_desc_ptr(%p)) failed: %d", post_desc_ptr, gni_rc);
                        } else {
                            log_debug("ugni_transport", "GetCompleted(progress post_desc_ptr(%p)) success", post_desc_ptr);
                        }
                        print_post_desc(post_desc_ptr);

                        if (cmd_tgt->update(nullptr) == 1) {
                            msg_vector_.remove(cmd_tgt->index);
                            cmd_tgt_freelist_->push(cmd_tgt);
                        }
                    }
                    break;

                case LONG_GET_MEM_CQ_INDEX:
                    log_debug("ugni_transport", "CqVectorMonitor() - event received on long_get_mem_cq_hdl_");

                    // acknowledge but ignore for now
                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        print_cq_event(&ev_data, false);
                    }

                    break;

                case RDMA_EP_CQ_INDEX:
                    log_debug("ugni_transport", "CqVectorMonitor() - event received on rdma_ep_cq_hdl_");

                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        print_cq_event(&ev_data, false);

                        nnti::core::ugni_rdma_op *rdma_op = (nnti::core::ugni_rdma_op*)op_vector_.at(GNI_CQ_GET_INST_ID(ev_data));

                        log_debug("ugni_transport", "calling GetComplete(progress)");
                        nthread_lock(&ugni_lock_);
                        gni_rc=GNI_GetCompleted (cq_list[which_cq], ev_data, &post_desc_ptr);
                        nthread_unlock(&ugni_lock_);
                        if (gni_rc!=GNI_RC_SUCCESS) {
                            log_error("ugni_transport", "GetCompleted(progress post_desc_ptr(%p)) failed: %d", post_desc_ptr, gni_rc);
                        } else {
                            log_debug("ugni_transport", "GetCompleted(progress post_desc_ptr(%p)) success", post_desc_ptr);
                        }
                        print_post_desc(post_desc_ptr);

                        if (rdma_op->update(nullptr) == 1) {
                            op_vector_.remove(rdma_op->index);
                            rdma_op_freelist_->push(rdma_op);
                        }
                    }
                    break;

                case RDMA_MEM_CQ_INDEX:
                    log_debug("ugni_transport", "CqVectorMonitor() - event received on rdma_mem_cq_hdl_");

                    // acknowledge but ignore for now
                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        print_cq_event(&ev_data, false);
                    }

                    break;

                case INTERRUPT_CQ_INDEX:
                    if (get_event(cq_list[which_cq], &ev_data) == GNI_RC_SUCCESS) {
                        log_debug("ugni_transport", "CqVectorMonitor() interrupted by transport::interrupt()");
                        interrupted=true;
                        nnti_rc = NNTI_EINTR;
                    }
                    continue;

                default:
                    break;
            }

            nnti_rc = NNTI_OK;
        } else {
            char errstr[1024];
            uint32_t recoverable=0;
            errstr[0]='\0';
            GNI_CqErrorStr(ev_data, errstr, 1023);
            GNI_CqErrorRecoverable(ev_data, &recoverable);

            log_error("ugni_transport", "CqVectorMonitor failed (gni_rc=%d) (recoverable=%llu) : %s",
                    gni_rc, (uint64_t)recoverable, errstr);
            print_cq_event(&ev_data, false);

            nnti_rc = NNTI_EIO;
        }
    }

cleanup:
    log_debug("ugni_transport", "exit");

    return;
}

void
ugni_transport::start_progress_thread(void)
{
    terminate_progress_thread_ = false;
    progress_thread_ = std::thread(&nnti::transports::ugni_transport::progress, this);
}
void
ugni_transport::stop_progress_thread(void)
{
    log_debug("ugni_transport", "stop_progress_thread() - enter");
    terminate_progress_thread_ = true;
    interrupt();
    progress_thread_.join();
    log_debug("ugni_transport", "stop_progress_thread() - exit");
}


void
ugni_transport::connect_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    nnti::core::ugni_connection *conn;

    log_debug("ugni_transport", "inbound connection from %s", std::string(args.at("hostname")+":"+args.at("port")).c_str());

    nthread_lock(&new_connection_lock_);

    nnti::core::nnti_url peer_url = nnti::core::nnti_url(args.at("hostname"), args.at("port"));

    log_debug("ugni_transport", "Looking for connection with pid=%016lx", peer_url.pid());
    conn = (nnti::core::ugni_connection*)conn_map_.get(peer_url.pid());
    if (conn != nullptr) {
        log_debug("ugni_transport", "Found connection with pid=%016lx", peer_url.pid());
    } else {
        log_debug("ugni_transport", "Couldn't find connection with pid=%016lx", peer_url.pid());

        conn = new nnti::core::ugni_connection(this, cmd_msg_size_, cmd_msg_count_, args);
        conn->index = conn_vector_.add(conn);
        conn_map_.insert(conn);

        conn->transition_to_ready();
    }

    nthread_unlock(&new_connection_lock_);

    results << "hostname="   << url_.hostname()      << std::endl;
    results << "addr="       << url_.addr()          << std::endl;
    results << "port="       << url_.port()          << std::endl;
    results << "local_addr=" << drc_info_.local_addr << std::endl;
    results << "instance="   << instance_            << std::endl;
    results << conn->reply_string();

    log_debug("ugni_transport", "connect_cb() - exit");
}

void
ugni_transport::disconnect_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
{
    log_debug("ugni_transport", "disconnect_cb() - enter");

    nnti::core::nnti_connection *conn = nullptr;
    nnti::core::nnti_url         peer_url(args.at("hostname"), args.at("port"));

    nthread_lock(&new_connection_lock_);

    log_debug("ugni_transport", "%s is disconnecting", peer_url.url().c_str());
    conn = conn_map_.get(peer_url.pid());
    log_debug("ugni_transport", "connection map says %s => conn(%p)", peer_url.url().c_str(), conn);

    if (conn != nullptr) {
        conn_map_.remove(conn);
        delete conn;
    }

    nthread_unlock(&new_connection_lock_);

    log_debug("ugni_transport", "disconnect_cb() - exit");
}

void
ugni_transport::stats_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
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
    html::mkList(results, stats);
    )

    html::mkFooter(results);
}

void
ugni_transport::peers_cb(const std::map<std::string,std::string> &args, std::stringstream &results)
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
ugni_transport::build_webhook_path(
    const char                  *service)
{
    std::stringstream wh_url;

    wh_url << "/nnti/ugni/"  << service;
    wh_url << "&hostname="   << url_.hostname();
    wh_url << "&addr="       << url_.addr();
    wh_url << "&port="       << url_.port();
    wh_url << "&local_addr=" << drc_info_.local_addr;
    wh_url << "&instance="   << instance_;

    return wh_url.str();
}
std::string
ugni_transport::build_webhook_path(
    nnti::core::nnti_connection *conn,
    const char                  *service)
{
    std::stringstream wh_url;

    wh_url << build_webhook_path(service);
    wh_url << conn->query_string();

    return wh_url.str();
}
std::string
ugni_transport::build_webhook_connect_path(void)
{
    return build_webhook_path("connect");
}
std::string
ugni_transport::build_webhook_connect_path(
    nnti::core::nnti_connection *conn)
{
    return build_webhook_path(conn, "connect");
}
std::string
ugni_transport::build_webhook_disconnect_path(void)
{
    return build_webhook_path("disconnect");
}
std::string
ugni_transport::build_webhook_disconnect_path(
    nnti::core::nnti_connection *conn)
{
    return build_webhook_path(conn, "disconnect");
}

void
ugni_transport::register_webhook_cb(void)
{
    webhook::Server::registerHook("/nnti/ugni/connect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        connect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ugni/disconnect", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        disconnect_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ugni/stats", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        stats_cb(args, results);
    });
    webhook::Server::registerHook("/nnti/ugni/peers", [this] (const std::map<std::string,std::string> &args, std::stringstream &results){
        peers_cb(args, results);
    });
}

void
ugni_transport::unregister_webhook_cb(void)
{
    log_debug("ugni_transport", "unregister_webhook_cb() - enter");
    webhook::Server::deregisterHook("/nnti/ugni/connect");
    webhook::Server::deregisterHook("/nnti/ugni/disconnect");
    webhook::Server::deregisterHook("/nnti/ugni/stats");
    webhook::Server::deregisterHook("/nnti/ugni/peers");
    log_debug("ugni_transport", "unregister_webhook_cb() - exit");
}

NNTI_result_t
ugni_transport::create_send_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ugni_cmd_op      **cmd_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "create_send_op() - enter");

    if (work_id->wr().flags() & NNTI_OF_ZERO_COPY) {
        *cmd_op = new nnti::core::ugni_cmd_op(this, work_id);
        (*cmd_op)->index = op_vector_.add(*cmd_op);
    } else {
        if (cmd_op_freelist_->pop(*cmd_op)) {
            (*cmd_op)->set(work_id);
            (*cmd_op)->index = op_vector_.add(*cmd_op);
        } else {
            *cmd_op = new nnti::core::ugni_cmd_op(this, work_id);
            (*cmd_op)->index = op_vector_.add(*cmd_op);
        }
    }
    log_debug("ugni_transport", "(*cmd_op)->index=%u", (*cmd_op)!=nullptr ? (*cmd_op)->index : UINT_MAX);

    (*cmd_op)->src_op_id((*cmd_op)->index);

    log_debug("ugni_transport", "create_send_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::execute_cmd_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::ugni_cmd_op      *cmd_op)
{
    NNTI_result_t rc = NNTI_OK;
    nnti::datatype::nnti_peer   *peer = (nnti::datatype::nnti_peer *)work_id->wr().peer();
    nnti::core::ugni_connection *conn = (nnti::core::ugni_connection *)peer->conn();

    log_debug("ugni_transport", "execute_cmd_op(cmd_op->index=%u) - enter", cmd_op->index);

    if (conn->waitlisted() || cmd_op->update(NULL) == 2) {
        // no smsg credits available
        conn->waitlist_add(cmd_op);
    }

    if (conn->waitlisted()) {
        conn->waitlist_execute();
    }

    log_debug("ugni_transport", "execute_cmd_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::create_get_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ugni_rdma_op     **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "create_get_op() - enter");

    if (rdma_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
        (*rdma_op)->index = op_vector_.add(*rdma_op);
    } else {
        *rdma_op = new nnti::core::ugni_rdma_op(this, work_id);
    }

    log_debug("ugni_transport", "create_get_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::create_put_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ugni_rdma_op     **rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "create_put_op() - enter");

    if (rdma_op_freelist_->pop(*rdma_op)) {
        (*rdma_op)->set(work_id);
        (*rdma_op)->index = op_vector_.add(*rdma_op);
    } else {
        *rdma_op = new nnti::core::ugni_rdma_op(this, work_id);
        (*rdma_op)->index = op_vector_.add(*rdma_op);
    }

    log_debug("ugni_transport", "create_put_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::execute_rdma_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::ugni_rdma_op     *rdma_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "execute_rdma_op() - enter");

    rdma_op->update(nullptr);

    log_debug("ugni_transport", "execute_rdma_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::create_fadd_op(
    nnti::datatype::nnti_work_id  *work_id,
    nnti::core::ugni_atomic_op   **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "create_fadd_op() - enter");

    if (atomic_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
        (*atomic_op)->index = op_vector_.add(*atomic_op);
    } else {
        *atomic_op = new nnti::core::ugni_atomic_op(this, work_id);
        (*atomic_op)->index = op_vector_.add(*atomic_op);
    }

    log_debug("ugni_transport", "create_fadd_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::create_cswap_op(
    nnti::datatype::nnti_work_id   *work_id,
    nnti::core::ugni_atomic_op    **atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "create_cswap_op() - enter");

    if (atomic_op_freelist_->pop(*atomic_op)) {
        (*atomic_op)->set(work_id);
        (*atomic_op)->index = op_vector_.add(*atomic_op);
    } else {
        *atomic_op = new nnti::core::ugni_atomic_op(this, work_id);
        (*atomic_op)->index = op_vector_.add(*atomic_op);
    }

    log_debug("ugni_transport", "create_cswap_op() - exit");

    return rc;
}

NNTI_result_t
ugni_transport::execute_atomic_op(
    nnti::datatype::nnti_work_id *work_id,
    nnti::core::ugni_atomic_op   *atomic_op)
{
    NNTI_result_t rc = NNTI_OK;

    log_debug("ugni_transport", "execute_atomic_op() - enter");

    atomic_op->update(nullptr);

    log_debug("ugni_transport", "execute_atomic_op() - exit");

    return rc;
}

NNTI_event_t *
ugni_transport::create_event(
    nnti::core::ugni_rdma_op *rdma_op)
{
    nnti::datatype::nnti_work_id            *wid = rdma_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    nnti::datatype::nnti_buffer             *b   = nnti::datatype::nnti_buffer::to_obj(wr.local_hdl());
    NNTI_event_t                            *e   = nullptr;

    log_debug("ugni_transport", "create_event(rdma_op) - enter");

    if (event_freelist_->pop(e) == false) {
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

    log_debug("ugni_transport", "create_event(rdma_op) - exit");

    return(e);
}

NNTI_event_t *
ugni_transport::create_event(
    nnti::core::ugni_atomic_op *atomic_op)
{
    nnti::datatype::nnti_work_id            *wid = atomic_op->wid();
    const nnti::datatype::nnti_work_request &wr  = wid->wr();
    NNTI_event_t                            *e   = nullptr;

    log_debug("ugni_transport", "create_event(atomic_op) - enter");

    if (event_freelist_->pop(e) == false) {
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

    log_debug("ugni_transport", "create_event(atomic_op) - exit");

    return(e);
}

nnti::datatype::nnti_buffer *
ugni_transport::unpack_buffer(
    char           *packed_buf,
    const uint64_t  packed_len)
{
    NNTI_buffer_t                hdl;
    nnti::datatype::nnti_buffer *b = nullptr;

    this->dt_unpack(&hdl, packed_buf, packed_len);
    b = nnti::datatype::nnti_buffer::to_obj(hdl);

    nnti::datatype::nnti_buffer *found = buffer_map_.get(b->payload());
    if (found == nullptr) {
        log_debug("ugni_transport", "unpack_buffer() - buffer not found in buffer_map_ for address=%p", b->payload());
        // if the buffer is not in the map, then use b.
        found = b;
    } else {
        // if the buffer is in the map, then delete b.
        delete b;
    }

    return found;
}

NNTI_result_t
ugni_transport::get_drc_info(
    drc_info_t *drc_info)
{
    int          rc;
    gni_return_t grc;
    int          flags = 0;

    char *ptr;

    if (NULL == (ptr = getenv("PMI_GNI_DEV_ID"))) {
        assert(NULL == ptr && "PMI_GNI_DEV_ID is not defined.  Something is wrong.");
        return NNTI_EINVAL;
    }
    errno = 0;
    drc_info->device_id = (uint8_t)strtoul (ptr, (char **)NULL, 10);
    if (0 != errno) {
        return NNTI_EINVAL;
    }

    int64_t tmp_cred=0;
    rc = config_.GetInt(&tmp_cred, "nnti.transport.credential_id", "0");
    if (rc == 0) {
        drc_info->credential_id = (uint32_t)tmp_cred;

        rc = drc_access(drc_info->credential_id, flags, &drc_info->drc_info_hdl);
        if (rc != DRC_SUCCESS) {
            // Something is wrong. Either the credential doesn't exist or you don't have permission to access it
            log_error("ugni_transport", "drc_access() failed %d", rc);
            goto error;
        }

        drc_info->cookie1 = drc_get_first_cookie(drc_info->drc_info_hdl);
        drc_info->ptag1 = GNI_FIND_ALLOC_PTAG;
        grc = GNI_GetPtag(drc_info->device_id, drc_info->cookie1, &drc_info->ptag1);
        if (grc != GNI_RC_SUCCESS) {
            // Something is wrong. This should not happen if drc_access returned DRC_SUCCESS
            log_error("ugni_transport", "GNI_GetPtag() failed %d", grc);
            goto error;
        }

        return NNTI_OK;

error:
        rc = drc_release(drc_info->credential_id, flags);
        if (rc != DRC_SUCCESS) {
            /* failed to release credential */
            log_error("ugni_transport", "drc_release() failed %d", rc);
        }

        return NNTI_EINVAL;

    } else {
        if (NULL == (ptr = getenv("PMI_GNI_PTAG"))) {
            assert(NULL == ptr && "PMI_GNI_PTAG is not defined.  Something is wrong.");
            return NNTI_EINVAL;
        }
        errno = 0;
        drc_info->ptag1 = (uint8_t)strtoul (ptr, (char **)NULL, 10);
        if (0 != errno) {
            return NNTI_EINVAL;
        }

        if (NULL == (ptr = getenv("PMI_GNI_COOKIE"))) {
            assert(NULL == ptr && "PMI_GNI_COOKIE is not defined.  Something is wrong.");
            return NNTI_EINVAL;
        }
        errno = 0;
        drc_info->cookie1 = (uint32_t) strtoul (ptr, NULL, 10);
        if (0 != errno) {
            return NNTI_EINVAL;
        }

        return NNTI_OK;
    }

}

//void
//ugni_transport::print_wc(
//    const nnti_gni_work_completion_t *wc)
//{
//    if (!logging_debug("ugni_transport")) {
//        return;
//    }
//
//    log_debug("ugni_transport", "wc=%p, wc.op=%d, wc.inst_id=%llu, wc.byte_len=%llu, wc.byte_offset=%llu, wc.src_index=%llu, wc.src_offset=%llu, wc.dest_index=%llu, wc.dest_offset=%llu",
//            wc,
//            wc->op,
//            wc->inst_id,
//            wc->byte_len,
//            wc->byte_offset,
//            wc->src_index,
//            wc->src_offset,
//            wc->dest_index,
//            wc->dest_offset);
//}

void
ugni_transport::print_cq_event(
    const gni_cq_entry_t *event,
    const bool            force)
{
    if (force) {
        log_debug("ugni_transport", "event=%p, event.data=%llu, event.source=%llu, event.status=%llu, "
                "event.info=%llu, event.overrun=%llu, event.inst_id=%llu, event.rem_inst_id=%llu, "
                "event.tid=%llu, event.msg_id=%llu, event.type=%llu",
                event,
                (uint64_t)gni_cq_get_data(*event),
                (uint64_t)gni_cq_get_source(*event),
                (uint64_t)gni_cq_get_status(*event),
                (uint64_t)gni_cq_get_info(*event),
                (uint64_t)gni_cq_overrun(*event),
                (uint64_t)gni_cq_get_inst_id(*event),
                (uint64_t)gni_cq_get_rem_inst_id(*event),
                (uint64_t)gni_cq_get_tid(*event),
                (uint64_t)GNI_CQ_GET_MSG_ID(*event),
                (uint64_t)gni_cq_get_type(*event));
    } else if (gni_cq_get_status(*event) != 0) {
        log_error("ugni_transport", "event=%p, event.data=%llu, event.source=%llu, event.status=%llu, "
                "event.info=%llu, event.overrun=%llu, event.inst_id=%llu, event.rem_inst_id=%llu, "
                "event.tid=%llu, event.msg_id=%llu, event.type=%llu",
                event,
                (uint64_t)gni_cq_get_data(*event),
                (uint64_t)gni_cq_get_source(*event),
                (uint64_t)gni_cq_get_status(*event),
                (uint64_t)gni_cq_get_info(*event),
                (uint64_t)gni_cq_overrun(*event),
                (uint64_t)gni_cq_get_inst_id(*event),
                (uint64_t)gni_cq_get_rem_inst_id(*event),
                (uint64_t)gni_cq_get_tid(*event),
                (uint64_t)GNI_CQ_GET_MSG_ID(*event),
                (uint64_t)gni_cq_get_type(*event));
    } else {
        log_debug("ugni_transport", "event=%p, event.data=%llu, event.source=%llu, event.status=%llu, "
                "event.info=%llu, event.overrun=%llu, event.inst_id=%llu, event.rem_inst_id=%llu, "
                "event.tid=%llu, event.msg_id=%llu, event.type=%llu",
                event,
                (uint64_t)gni_cq_get_data(*event),
                (uint64_t)gni_cq_get_source(*event),
                (uint64_t)gni_cq_get_status(*event),
                (uint64_t)gni_cq_get_info(*event),
                (uint64_t)gni_cq_overrun(*event),
                (uint64_t)gni_cq_get_inst_id(*event),
                (uint64_t)gni_cq_get_rem_inst_id(*event),
                (uint64_t)gni_cq_get_tid(*event),
                (uint64_t)GNI_CQ_GET_MSG_ID(*event),
                (uint64_t)gni_cq_get_type(*event));
    }
}

void ugni_transport::print_post_desc(
    const gni_post_descriptor_t *post_desc_ptr)
{
//    if (!logging_debug("ugni_transport")) {
//        return;
//    }

    if (post_desc_ptr != NULL) {
        log_debug("ugni_transport", "post_desc_ptr                  ==%p", (uint64_t)post_desc_ptr);
        log_debug("ugni_transport", "post_desc_ptr->next_descr      ==%p", (uint64_t)post_desc_ptr->next_descr);
        log_debug("ugni_transport", "post_desc_ptr->prev_descr      ==%p", (uint64_t)post_desc_ptr->prev_descr);

        log_debug("ugni_transport", "post_desc_ptr->post_id         ==%llu", (uint64_t)post_desc_ptr->post_id);
        log_debug("ugni_transport", "post_desc_ptr->status          ==%llu", (uint64_t)post_desc_ptr->status);
        log_debug("ugni_transport", "post_desc_ptr->cq_mode_complete==%llu", (uint64_t)post_desc_ptr->cq_mode_complete);

        log_debug("ugni_transport", "post_desc_ptr->type            ==%llu", (uint64_t)post_desc_ptr->type);
        log_debug("ugni_transport", "post_desc_ptr->cq_mode         ==%llu", (uint64_t)post_desc_ptr->cq_mode);
        log_debug("ugni_transport", "post_desc_ptr->dlvr_mode       ==%llu", (uint64_t)post_desc_ptr->dlvr_mode);
        log_debug("ugni_transport", "post_desc_ptr->local_addr      ==%llu or %p", (uint64_t)post_desc_ptr->local_addr, post_desc_ptr->local_addr);
        log_debug("ugni_transport", "post_desc_ptr->remote_addr     ==%llu or %p", (uint64_t)post_desc_ptr->remote_addr, post_desc_ptr->remote_addr);
        log_debug("ugni_transport", "post_desc_ptr->length          ==%llu", (uint64_t)post_desc_ptr->length);
        log_debug("ugni_transport", "post_desc_ptr->rdma_mode       ==%llu", (uint64_t)post_desc_ptr->rdma_mode);
        log_debug("ugni_transport", "post_desc_ptr->src_cq_hndl     ==%llu", (uint64_t)post_desc_ptr->src_cq_hndl);
        log_debug("ugni_transport", "post_desc_ptr->sync_flag_value ==%llu", (uint64_t)post_desc_ptr->sync_flag_value);
        log_debug("ugni_transport", "post_desc_ptr->sync_flag_addr  ==%llu", (uint64_t)post_desc_ptr->sync_flag_addr);
        log_debug("ugni_transport", "post_desc_ptr->amo_cmd         ==%llu", (uint64_t)post_desc_ptr->amo_cmd);
        log_debug("ugni_transport", "post_desc_ptr->first_operand   ==%llu", (uint64_t)post_desc_ptr->first_operand);
        log_debug("ugni_transport", "post_desc_ptr->second_operand  ==%llu", (uint64_t)post_desc_ptr->second_operand);
        log_debug("ugni_transport", "post_desc_ptr->cqwrite_value   ==%llu", (uint64_t)post_desc_ptr->cqwrite_value);
    } else {
        log_debug("ugni_transport", "post_desc_ptr == NULL");
    }
}

//void
//ugni_transport::print_gni_conn(
//    nnti_gni_connection_t *c)
//{
//    log_level "ugni_transport"="ugni_transport";
//
//    if (!logging_debug("ugni_transport")) {
//        return;
//    }
//
//    log_debug("ugni_transport", "c->peer_name       =%s", c->peer_name);
//    log_debug("ugni_transport", "c->peer_addr       =%u", c->peer_addr);
//    log_debug("ugni_transport", "c->peer_port       =%u", (uint32_t)c->peer_port);
//    log_debug("ugni_transport", "c->peer_instance   =%llu", (uint64_t)c->peer_instance);
//
//    log_debug("ugni_transport", "c->state           =%d", c->state);
//}



} /* namespace transports */
} /* namespace nnti */
