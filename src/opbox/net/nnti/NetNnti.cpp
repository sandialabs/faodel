// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>

#include <sys/unistd.h> //gethostname
#include <cassert>

#include <boost/bimap.hpp>

#include "opbox/net/net.hh"
#include "opbox/net/net_internal.hh"
#include "opbox/net/peer.hh"

#include "nnti/nnti.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/transport_factory.hpp"

#include "faodel-common/Configuration.hh"
#include "faodel-common/NodeID.hh"

#include "webhook/Server.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "opbox/common/Types.hh"
#include "opbox/OpBox.hh"


using namespace std;
using namespace net;
using namespace opbox;
using namespace lunasa;


namespace opbox {
namespace net {

typedef struct peer_t {
  NNTI_peer_t p;

  peer_t() {} //Empty when given nothing
  peer_t(const NNTI_peer_t &p_) : p(p_) {}
  peer_t(const peer_t &p_) : p(p_.p) {}
  peer_t & operator=(const peer_t &rhs) { p = rhs.p; return *this; }
  peer_t & operator=(const NNTI_peer_t &rhs) { p = rhs; return *this; }
  operator const NNTI_peer_t &() const {return p; }
  operator NNTI_peer_t &() { return p; }
  bool operator== (const peer_t &other) const { return (p == other.p); }
  bool operator!= (const peer_t &other) const { return (p != other.p); }
  bool operator<  (const peer_t &other) const { return (p <  other.p); }

} peer_t;

namespace NetNnti
{

    typedef boost::bimap< faodel::nodeid_t, opbox::net::peer_t * > nodeid_peer_bimap;
    typedef nodeid_peer_bimap::value_type                    nodeid_peer_value;
    nodeid_peer_bimap peer_bimap;

    NNTI_attrs_t nnti_attrs_;

    faodel::nodeid_t myid_;

    bool use_zero_copy_;

    class user_invoking_callback
    {
    private:
        std::function< WaitingType(OpArgs *args) > user_cb_;
        void *context_;

        UpdateType event_to_update_type(NNTI_event_t *event) {
            switch(event->type) {
                case NNTI_EVENT_SEND:
                    return UpdateType::send_success;
                    break;
                case NNTI_EVENT_PUT:
                    return UpdateType::put_success;
                    break;
                case NNTI_EVENT_GET:
                    return UpdateType::get_success;
                    break;
                case NNTI_EVENT_ATOMIC:
                    return UpdateType::atomic_success;
                    break;
                default:
                    abort();
                    break;
            }
        }
    public:
        user_invoking_callback()
        {
            return;
        }
        user_invoking_callback(lambda_net_update_t user_cb, void *context)
        {
            user_cb_ = user_cb;
            context_ = context;
            return;
        }

        NNTI_result_t operator() (NNTI_event_t *event, void *context)
        {
            NNTI_result_t rc = NNTI_OK;

            log_debug("NetNnti", "user_invoking_callback->operator()");

            //peer_t    sender(event->peer);
            //OpArgs    args;
            //results_t results;
            //args.type                 = event_to_update_type(event);
            //args.data.msg.sender      = &sender;
            //args.data.msg.ptr = (message_t*)((char*)event->start + event->offset);

            //WaitingType cb_rc = user_cb_(args, &results);



            //CDU: TODO- Do we need to pass in sender, and msg?
            OpArgs *args = new OpArgs( event_to_update_type(event) );
            WaitingType cb_rc = user_cb_(args);
            //CDU: end


            // if the LDO was used to send a message, then we own it so release it.
            // an LDO used for RDMAs or atomics is  owned by the app, so it should release it.
            if (event->op == NNTI_OP_SEND) {
                DataObject *ldo = (DataObject*)context_;
//                opbox::net::ReleaseMessage(ldo);
                delete ldo;
            }

            return rc;
        }
    };

    class default_callback
    {
    public:
        NNTI_result_t operator() (NNTI_event_t *event, void *context)
        {
            DataObject *ldo = (DataObject*)context;
//            opbox::net::ReleaseMessage(ldo);
            delete ldo;
            return NNTI_OK;
        }
    };

    bool initialized_;
    bool started_;

    faodel::Configuration config_;

    nnti::transports::transport *t_;

    NNTI_event_queue_t  unexpected_eq_;
    NNTI_event_queue_t  send_eq_;
    NNTI_event_queue_t  recv_eq_;
    NNTI_event_queue_t  rdma_eq_;

    default_callback                    *dcb_;
    user_invoking_callback              *uicb_;
    nnti::datatype::nnti_event_callback *default_send_cb_;

    struct NntiRecvBuffer {
        DataObject ldo_;

        NntiRecvBuffer(
            DataObject msg)
        {
            ldo_ = msg;
        }
    };
    std::deque<struct NntiRecvBuffer*> recv_buffers_;

    void postRecvBuffer(DataObject ldo)
    {
        recv_buffers_.push_back(new NntiRecvBuffer(ldo));
    }
    void repostRecvBuffer(NntiRecvBuffer *nrb)
    {
        recv_buffers_.push_back(nrb);
    }
    void setupRecvQueue()
    {
        opbox::net::Attrs attrs;
        opbox::net::GetAttrs(&attrs);

        for (int i=0;i<10;i++) {
            DataObject ldo = NewMessage(attrs.max_eager_size);
            postRecvBuffer(ldo);
        }
    }
    void teardownRecvQueue()
    {
        while(!recv_buffers_.empty()) {
            NntiRecvBuffer *nrb = recv_buffers_.front();
            recv_buffers_.pop_front();
//            opbox::net::ReleaseMessage(nrb->ldo_);
            delete nrb;
        }
    }

    void translateConfig(faodel::Configuration &config)
    {
        faodel::rc_t rc;
        std::vector<std::string> keys;
        bool b;

        /*
         * NNTI doesn't support toggling of individual logger severities.
         * Instead, find the most verbose severity that is toggled on and
         * use that as the NNTI severity.
         */
        keys = {
            "debug",
            "info",
            "warn",
            "error",
            "fatal",
        };
        for  (auto key : keys ) {
            rc = config.GetBool(&b, std::string("net.log."+key), "false");
            if (rc==0 && b) {
                config.Set("nnti.logger.severity", key);
                break;
            }
        }

        /*
         * Directly map net.log.filename to nnti.logger.filename.
         */
        std::string logfile;
        rc = config.GetString(&logfile, "net.log.filename");
        if (rc==0) {
            config.Set("nnti.logger.filename", logfile);
        }

        /*
         * Directly map net.transport.name to nnti.transport.name.  If
         * net.transport.name is not set, nnti.transport.name will not
         * be set.
         */
        std::string transport_name;
        rc = config.GetString(&transport_name, "net.transport.name");
        if (rc==0) {
            if (transport_name == "gni") {
                // translate the libfabric name to the NNTI name
                transport_name = "ugni";
            } else if (transport_name == "verbs") {
                // translate the libfabric name to the NNTI name
                transport_name = "ibverbs";
            } else if (transport_name == "sockets") {
                // NNTI doesn't have a sockets transport.  warn and try MPI.
                // TODO: the logger isn't iniitialized yet.  use cerr until this is fixed.
                std::cerr << "NetNnti -> net.transport.name has unsupported value 'sockets'.  Failing back to 'mpi'." << std::endl;
                transport_name = "mpi";
            }
            config.Set("nnti.transport.name", transport_name);
        }

        /*
         * In Opbox, NNTI specific keys start with "net.nnti." so we
         * strip off the "net." before sending the config to NNTI.
         */
        size_t index;
        std::vector<std::pair<std::string,std::string>> all_kv;
        config.GetAllSettings(&all_kv);
        for ( auto kv : all_kv ) {
            std::string k = kv.first;
            index = k.find("net.nnti.", 0);
            // only do the replace if "net.nnti." is at the beginning of the key name
            if (index == 0) {
                k.replace(index, 9, "nnti.");
                config.Set(k, kv.second);
            }
        }
    }
};

namespace NetNnti
{
    std::function<void(opbox::net::peer_ptr_t, opbox::message_t*)> recv_cb_;

    // This is the Nnti implementation of the remote network buffer
    struct NntiBufferRemote {
        uint32_t offset;
        uint32_t length;
        char     packed[];
    };

    // This is the Nnti implementation of the local network buffer.
    struct NntiBufferLocal : NetBufferLocal {
        NNTI_buffer_t nnti_buffer;
        uint64_t      base_addr;
        uint32_t      length;

        void makeRemoteBuffer (
            size_t           remote_offset,   // in
            size_t           remote_length,   // in
            NetBufferRemote *remote_buffer) override;  // out
    };

    void NntiBufferLocal::makeRemoteBuffer(
        size_t           remote_offset,        // in
        size_t           remote_length,        // in
        NetBufferRemote *remote_buffer) { // out

        NntiBufferRemote *rb = (NntiBufferRemote*)remote_buffer;

        rb->offset           = remote_offset;
        rb->length           = remote_length;

        t_->dt_pack((void*)nnti_buffer, &rb->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);
        log_debug("NetNnti", "offsetof(nbr.packed) is %lu", offsetof(NntiBufferRemote, packed));
    }

    void RegisterMemory(void *base_addr, size_t length, void *&pinned)
    {
        NntiBufferLocal *nbl = new NntiBufferLocal();
        nnti::datatype::nnti_event_callback *cb = new nnti::datatype::nnti_event_callback(t_, *dcb_);
        t_->register_memory(
            (char*)base_addr,
            length,
            static_cast<NNTI_buffer_flags_t>(NNTI_BF_LOCAL_READ|NNTI_BF_LOCAL_WRITE|NNTI_BF_REMOTE_READ|NNTI_BF_REMOTE_WRITE),
            NNTI_INVALID_HANDLE,
            *cb,
            nullptr,
            &nbl->nnti_buffer);
        nbl->base_addr = (uint64_t)base_addr;
        nbl->length    = length;
        pinned = (void*)nbl;
    }
    void UnregisterMemory(void *&pinned)
    {
        NntiBufferLocal *nbl = (NntiBufferLocal *)pinned;
        t_->unregister_memory(nbl->nnti_buffer);
        delete nbl;
        pinned = nullptr;
    }

    class unexpected_callback
    {
    public:
        NNTI_result_t operator() (NNTI_event_t *event, void *context)
        {
            log_debug("NetNnti", "unexpected_callback->operator()");

            if (!started_) {
                return NNTI_EIO;
            }

            if (event->length <= nnti_attrs_.max_eager_size) {
                log_debug("NetNnti", "using short message path");
                log_debug_stream("NetNnti") << event;

                while(recv_buffers_.empty()) {
                    std::this_thread::yield();
                }
                NntiRecvBuffer *nrb = recv_buffers_.front();
                recv_buffers_.pop_front();

                NntiBufferLocal *msg_bl;
                uint32_t         msg_bl_offset;
                nrb->ldo_.GetDataRdmaHandle((void **)&msg_bl, msg_bl_offset);

//                memset(nrb->ldo_->metaPtr(), 5, nrb->ldo_->metaSize());
//                memset(nrb->ldo_->dataPtr(), 6, nrb->ldo_->dataSize());

//                stringstream ss;
//                nrb->ldo_->sstr(ss,0,0);
//                log_debug_stream("NetNnti") << ss.str();

                NNTI_event_t e;
                NNTI_result_t rc = t_->next_unexpected(msg_bl->nnti_buffer, msg_bl_offset, &e);
                if (rc != NNTI_OK) {
                    abort();
                }

                log_debug_stream("NetNnti") << e;

                peer_t        *sender = new peer_t(e.peer);
                message_t *msg = (message_t*)nrb->ldo_.GetDataPtr();
                recv_cb_(sender, msg);

                repostRecvBuffer(nrb);
            } else {
                log_debug("NetNnti", "using long message path");
                log_debug_stream("NetNnti") << event;

                uint32_t meta_size = nnti_attrs_.mtu - nnti_attrs_.max_eager_size;
                DataObject long_msg(meta_size, event->length, DataObject::AllocatorType::eager);

                NntiBufferLocal *msg_bl;
                uint32_t         msg_bl_offset;
                long_msg.GetDataRdmaHandle((void **)&msg_bl, msg_bl_offset);

//                memset(long_msg.metaPtr(), 5, long_msg.metaSize());
//                memset(long_msg.dataPtr(), 6, long_msg.dataSize());

//                stringstream ss;
//                long_msg.sstr(ss,0,0);
//                log_debug_stream("NetNnti") << ss.str();

                NNTI_event_t e;
                NNTI_result_t rc = t_->next_unexpected(msg_bl->nnti_buffer, msg_bl_offset, &e);
                if (rc != NNTI_OK) {
                    abort();
                }

                log_debug_stream("NetNnti") << e;

                peer_t        *sender = new peer_t(e.peer);
                message_t *msg = (message_t*)long_msg.GetDataPtr();
                recv_cb_(sender, msg);
            }


            return NNTI_OK;
        }
    };
};


using namespace NetNnti;


/**
 * @brief Initialize the network module using @c config.
 */
void Init(
    const faodel::Configuration &config)
{

  config_ = config;  // make a copy

  translateConfig(config_);

  dcb_             = new default_callback();
  uicb_            = new user_invoking_callback();
  default_send_cb_ = new nnti::datatype::nnti_event_callback(t_, *dcb_);

  use_zero_copy_=false;

  initialized_=true;
}

/**
 * @brief Start the network module.
 */
void Start()
{
    NNTI_result_t rc;

    //Webhook should be started by this point. We can get its info now.
    assert(webhook::Server::IsRunning() && "Webhook not started before NetNnti started");
    myid_ = webhook::Server::GetNodeID();

    t_ = nnti::transports::factory::get_instance(config_);

    t_->start();

    t_->attrs(&nnti_attrs_);

    nnti::datatype::nnti_event_callback unexpected_cb(t_, unexpected_callback());

    rc = t_->eq_create(128, NNTI_EQF_UNEXPECTED, unexpected_cb, nullptr, &unexpected_eq_);
    assert(rc==NNTI_OK && "couldn't create unexpected EQ");

    rc = t_->eq_create(128, NNTI_EQF_UNSET, &send_eq_);
    assert(rc==NNTI_OK && "couldn't create unexpected EQ");

    rc = t_->eq_create(128, NNTI_EQF_UNSET, &recv_eq_);
    assert(rc==NNTI_OK && "couldn't create unexpected EQ");

    rc = t_->eq_create(128, NNTI_EQF_UNSET, &rdma_eq_);
    assert(rc==NNTI_OK && "couldn't create unexpected EQ");

    lunasa::RegisterPinUnpin(RegisterMemory, UnregisterMemory);

    setupRecvQueue();

    std::stringstream ss;
    config_.sstr(ss, 0, 0);
    log_debug_stream("test_setup") << ss.str() << std::endl;

    started_=true;
}

/**
 * @brief Shutdown the network module.
 */
void Finish()
{
    assert(initialized_ && "NetNnti not initialized");
    assert(started_ && "NetNnti not started");

    started_=false;

    auto it = peer_bimap.begin();
    while (it != peer_bimap.end()) {
        log_debug("NetNnti", "Disconnecting nodeid %s (%x)", it->left.GetHex().c_str(), it->right);
        Disconnect(it->right);
        it = peer_bimap.begin();
    }

    teardownRecvQueue();
    t_->stop();
    delete dcb_;
    delete uicb_;
}

/**
 * @brief Register a callback that is invoked for each message received.
 */
void RegisterRecvCallback(
    std::function<void(opbox::net::peer_ptr_t, opbox::message_t*)> recv_cb)
{
    recv_cb_ = recv_cb;
}

/**
 * @brief Get the node id of this process.
 *
 * Deprecated since Opbox now has this functionality (@c opbox::GetMyID()).
 */
faodel::nodeid_t GetMyID()
{
    assert(initialized_ && "NetNnti not initialized");

    return myid_;
}

/**
 * @brief Convert a node id to a peer using the network module's map.
 *
 * Deprecated since Opbox is responsible for maintaining it's own map if desired.
 */
faodel::nodeid_t ConvertPeerToNodeID(
    opbox::net::peer_t *peer)
{
    faodel::nodeid_t nodeid;
    try {
        nodeid = peer_bimap.right.at(peer);
    }
    catch ( std::out_of_range & e ) {
        log_debug("NetNnti", "Couldn't find %p", (void*)peer);
    }

    return nodeid;
}

/**
 * @brief Convert a peer to a node id using the network module's map.
 *
 * Deprecated since Opbox is responsible for maintaining it's own map if desired.
 */
opbox::net::peer_t *ConvertNodeIDToPeer(
    faodel::nodeid_t nodeid)
{
    opbox::net::peer_t *peer = nullptr;
    try {
        peer = peer_bimap.left.at(nodeid);
    }
    catch ( std::out_of_range & e ) {
        log_debug("NetNnti", "Couldn't find %s", std::string(nodeid.GetIP()+":"+nodeid.GetPort()).c_str());
    }

    return peer;
}

/**
 * @brief Get the name of the active network module.
 */
string GetDriverName() {
  return "nnti3";
}

/**
 * @brief Get the attributes of the network module.
 * @param[out] attrs Where attributes are copied to
 */
void GetAttrs(
        Attrs *attrs) {
    attrs->mtu            = nnti_attrs_.mtu;
    attrs->max_eager_size = nnti_attrs_.max_eager_size;

    char url_c[NNTI_URL_LEN];
    t_->get_url(url_c, NNTI_URL_LEN);
    std::string url(url_c);
    uint32_t first  = url.find("://");
    first += 3;
    uint32_t second = url.find(":", first);
    second += 1;
    uint32_t third  = url.find("/", second);
    std::string hostname = url.substr(first, (second-1)-first);
    std::string port     = url.substr(second, third-second);

    strcpy(attrs->bind_hostname, hostname.c_str());
    strcpy(attrs->listen_port, port.c_str());

    log_debug("NetNnti", "attrs.mtu            = %ld", attrs->mtu);
    log_debug("NetNnti", "attrs.max_eager_size = %ld", attrs->max_eager_size);
    log_debug("NetNnti", "attrs.bind_hostname  = %s",  attrs->bind_hostname);
    log_debug("NetNnti", "attrs.listen_port    = %s",  attrs->listen_port);

}

/**
 * @brief Prepare for communication with the peer identified by peer_addr and peer_port.
 *
 * @param[out] peer       A handle to a peer that can be used for network operations.
 * @param[in]  peer_addr  The hostname or IP address the peer is bound to.
 * @param[in]  peer_port  The port the peer is listening on.
 * @return A result code
 */
int Connect(
    peer_ptr_t *peer,
    const char *peer_addr,
    const char *peer_port)
{
    faodel::nodeid_t nodeid = faodel::nodeid_t(peer_addr, peer_port);
    return Connect(peer, nodeid);
}

/**
 * @brief Prepare for communication with the peer identified by node ID.
 *
 * @param[out] peer         A handle to a peer that can be used for network operations.
 * @param[in]  peer_nodeid  The node ID of the peer.
 * @return A result code
 */
int Connect(
    peer_ptr_t        *peer,
    faodel::nodeid_t  peer_nodeid)
{
    NNTI_result_t rc = NNTI_OK;

    *peer = ConvertNodeIDToPeer(peer_nodeid);

    if (nullptr == *peer) {
        std::stringstream url;
        NNTI_peer_t p;

        url << "http://" << peer_nodeid.GetIP() << ":" << peer_nodeid.GetPort() << "/";

        log_debug("NetNnti", "Connecting to %s", url.str().c_str());

        rc = t_->connect(url.str().c_str(), 1000, &p);

        log_debug("NetNnti", "Connected to %p", (void*)p);

        *peer = new peer_t(p);

        peer_bimap.insert(nodeid_peer_value(peer_nodeid,*peer));
    }

    return rc;
}

int Disconnect(
    peer_ptr_t peer)
{
    log_debug("NetNnti", "Disconnecting from %p", (void*)peer);
    peer_bimap.right.erase(peer);
    int rc = t_->disconnect(peer->p);
    delete peer;
    return rc;
}

int Disconnect(
    faodel::nodeid_t peer_nodeid)
{
    log_debug("NetNnti", "Disconnecting from %s", std::string(peer_nodeid.GetIP()+":"+peer_nodeid.GetPort()).c_str());
    peer_ptr_t peer = ConvertNodeIDToPeer(peer_nodeid);
    if (peer == nullptr) {
        log_warn("NetNnti", "%s is not connected", std::string(peer_nodeid.GetIP()+":"+peer_nodeid.GetPort()).c_str());
        return -1;
    }
    return Disconnect(peer);
}


/*
 * @brief Create a DataObject that can be used for zero copy sends.
 *
 * @param[in]  size  The size in bytes of the data section of the LDO.
 * @return A pointer to the new LDO.
 */
DataObject NewMessage(
    uint64_t size)
{
    uint32_t meta_size = nnti_attrs_.mtu - nnti_attrs_.max_eager_size;
    DataObject ldo(meta_size, size, DataObject::AllocatorType::eager);
    return std::move(ldo);
}

/*
 * @brief Explicitly release a message DataObject after use.
 *
 * @param[in]  msg  The message to release.
 */
void ReleaseMessage(
    DataObject msg)
{
//    delete msg;
}


namespace internal {

/*
 * @brief This method returns the offset of the NetBufferRemote.
 *
 * @param[in]  nbr    - the nbr to query
 * @return the offset of the NetBufferRemote
 */
uint32_t GetOffset(
    opbox::net::NetBufferRemote *nbr) // in
{
    NntiBufferRemote *b = (NntiBufferRemote *)nbr;
    return b->offset;
}

/*
 * @brief This method returns the length of the NetBufferRemote.
 *
 * @param[in]  nbr    - the nbr to query
 * @return the length of the NetBufferRemote
 */
uint32_t GetLength(
    opbox::net::NetBufferRemote *nbr) // in
{
    NntiBufferRemote *b = (NntiBufferRemote *)nbr;
    return b->length;
}

/*
 * @brief This method increases the offset of the NetBufferRemote.
 *
 * @param[in]  nbr    - the nbr to modify
 * @param[in]  addend - the offset adjustment
 * @return 0 if success, otherwise nonzero
 *
 * This method increases the offset of the NetBufferRemote.  As a
 * side effect, the length will be reduced by the same amount so
 * that the window doesn't slide.  addend must be greater than or
 * equal to zero and less than the current length.
 */
int IncreaseOffset(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              addend)      // in
{
    NntiBufferRemote *b = (NntiBufferRemote *)nbr;
    if (addend < b->length) {
        b->offset += addend;
        b->length -= addend;
    } else {
        return -1;
    }
    return 0;
}

/*
 * @brief This method decreases the length of the NetBufferRemote.
 *
 * @param[in]  nbr        - the nbr to modify
 * @param[in]  subtrahend - the length adjustment
 * @return 0 if success, otherwise nonzero
 *
 * This method decreases the length of the NetBufferRemote.  The
 * offset is not adjusted.  subtrahend must be greater than or
 * equal to zero and less than the current length.
 */
int DecreaseLength(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              subtrahend)  // in
{
    NntiBufferRemote *b = (NntiBufferRemote *)nbr;
    if (subtrahend < b->length) {
        b->length -= subtrahend;
    } else {
        return -1;
    }
    return 0;
}

/*
 * @brief This method sets the length of the NetBufferRemote.
 *
 * @param[in]  nbr     the nbr to modify
 * @param[in]  length  the new length
 * @return 0 if success, otherwise nonzero
 *
 * This method sets the length of the NetBufferRemote.  The offset
 * is not adjusted.  subtrahend must be greater than zero and less
 * than the current length.
 *
 */
int TrimToLength(
    opbox::net::NetBufferRemote *nbr,      // in
    uint32_t              length)   // in
{
    NntiBufferRemote *b = (NntiBufferRemote *)nbr;
    if (length < b->length) {
        b->length = length;
    } else {
        return -1;
    }
    return 0;
}

}

/**
 * @brief Send a message to the peer identified by @c peer.
 *
 * @param[in] peer  A handle to the tagret peer.
 * @param[in] msg   The LDO to send.
 *
 * Send the entire msg to peer.  After the send completes
 * (successfully or not), release msg.
 * opbox gets no feedback about this operations (fire and forget).
 */
void SendMsg(
    peer_t      *peer,
    DataObject   msg)
{
    volatile int rc;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NNTI_peer_t peer_hdl = peer->p;

    NntiBufferLocal *msg_bl        = nullptr;
    uint32_t         msg_bl_offset = 0;
    if (use_zero_copy_ &&
        ((msg.GetMetaSize() + msg.GetDataSize()) <= nnti_attrs_.mtu)) {
        log_debug("NetNnti", "using zero-copy");
        rc = msg.GetMetaRdmaHandle( (void **)&msg_bl, msg_bl_offset);
        if (rc != 0) {
            log_error("NetNnti", "msg->GetMetaRdmaHandle() failed: %d", rc);
            abort();
        }

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = (NNTI_op_flags_t)(NNTI_OF_LOCAL_EVENT | NNTI_OF_ZERO_COPY);
        base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = msg_bl->nnti_buffer;
        base_wr.local_offset  = msg_bl_offset;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg.GetMetaSize() + msg.GetDataSize();
    } else {
        rc = msg.GetDataRdmaHandle( (void **)&msg_bl, msg_bl_offset);
        if (rc != 0) {
            log_error("NetNnti", "msg->GetDataRdmaHandle() failed: %d", rc);
            abort();
        }

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = msg_bl->nnti_buffer;
        base_wr.local_offset  = msg_bl_offset;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg.GetDataSize();
    }

    base_wr.cb_context = new DataObject(msg);
    nnti::datatype::nnti_work_request wr(t_, base_wr, *default_send_cb_);

    t_->send(&wr, &wid);
}

/**
 * @brief Send a message to the peer identified by @c peer.
 *
 * @param[in] peer      A handle to the target peer.
 * @param[in] msg       The LDO to send.
 * @param[in] user_cb   The callback to invoke when events are generated from this send.
 *
 * Send the entire msg to peer.  user_cb is invoked after the send
 * completes.  msg is not automatically released.
 */
void SendMsg(
    peer_t     *peer,
    DataObject  msg,
    lambda_net_update_t user_cb)
{
    volatile int rc;

    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NNTI_peer_t peer_hdl = peer->p;

    NntiBufferLocal *msg_bl        = nullptr;
    uint32_t         msg_bl_offset = 0;

    if (use_zero_copy_ &&
        ((msg.GetMetaSize() + msg.GetDataSize()) <= nnti_attrs_.mtu)) {
        log_debug("NetNnti", "using zero-copy");
        rc = msg.GetMetaRdmaHandle( (void **)&msg_bl, msg_bl_offset);
        if (rc != 0) {
            log_error("NetNnti", "msg->GetMetaRdmaHandle() failed: %d", rc);
            abort();
        }

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = (NNTI_op_flags_t)(NNTI_OF_LOCAL_EVENT | NNTI_OF_ZERO_COPY);
        base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = msg_bl->nnti_buffer;
        base_wr.local_offset  = msg_bl_offset;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg.GetMetaSize() + msg.GetDataSize();
    } else {
        rc = msg.GetDataRdmaHandle( (void **)&msg_bl, msg_bl_offset);
        if (rc != 0) {
            log_error("NetNnti", "msg->GetDataRdmaHandle() failed: %d", rc);
            abort();
        }

        base_wr.op            = NNTI_OP_SEND;
        base_wr.flags         = NNTI_OF_LOCAL_EVENT;
        base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
        base_wr.peer          = peer_hdl;
        base_wr.local_hdl     = msg_bl->nnti_buffer;
        base_wr.local_offset  = msg_bl_offset;
        base_wr.remote_hdl    = NNTI_INVALID_HANDLE;
        base_wr.remote_offset = 0;
        base_wr.length        = msg.GetDataSize();
    }

    user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(msg));
    nnti::datatype::nnti_event_callback *send_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);

    nnti::datatype::nnti_work_request wr(t_, base_wr, *send_cb);

    t_->send(&wr, &wid);
}

/**
 * @brief Read an entire LDO from @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] remote_buffer  The source of the GET.
 * @param[in] local_ldo      The destination of the GET.
 * @param[in] user_cb        The callback to invoke when events are generated from this GET.
 *
 * Execute a one-sided read from remote_buffer on peer to local_ldo.
 * Both remote and local offsets are zero.  The length of the read
 * is the smaller of remote_buffer and local_ldo.  user_cb is invoked after
 * the read completes.
 */
void Get(
    peer_t          *peer,
    NetBufferRemote *remote_buffer,
    DataObject       local_ldo,
    lambda_net_update_t user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    uint64_t         local_size = local_ldo.GetHeaderSize() + local_ldo.GetMetaSize() + local_ldo.GetDataSize();
    local_ldo.GetHeaderRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_GET;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset;
    base_wr.length        = std::min(local_size, (uint64_t) nbr->length); //TODO: widths?

    nnti::datatype::nnti_event_callback *get_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        get_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        get_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *get_cb);

    t_->get(&wr, &wid);
}

/*
 * @brief Read a subset of an LDO from @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] remote_buffer  The source of the GET.
 * @param[in] remote_offset  The offset into the source buffer.
 * @param[in] local_ldo      The destination of the GET.
 * @param[in] local_offset   The offset into the destination buffer.
 * @param[in] length         The length of the GET.
 * @param[in] user_cb        The callback to invoke when events are generated from this GET.
 *
 * Execute a one-sided read from remote_buffer on peer to local_ldo.
 * user_cb is invoked after the read completes.
 */
void Get(
    peer_t          *peer,
    NetBufferRemote *remote_buffer,
    uint64_t         remote_offset,
    DataObject       local_ldo,
    uint64_t         local_offset,
    uint64_t         length,
    lambda_net_update_t user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    local_ldo.GetHeaderRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_GET;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset + local_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset + remote_offset;
    base_wr.length        = length;

    nnti::datatype::nnti_event_callback *get_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        get_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        get_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *get_cb);

    t_->get(&wr, &wid);
}

/*
 * @brief Write an entire LDO to @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] local_ldo      The source of the PUT.
 * @param[in] remote_buffer  The destination of the PUT.
 * @param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided write to remote_buffer on peer from local_ldo.
 * Both remote and local offsets are zero.  The length of the write
 * is the smaller of remote_buffer and local_ldo.  user_cb is invoked after
 * the write completes.
 */
void Put(
    peer_t          *peer,
    DataObject       local_ldo,
    NetBufferRemote *remote_buffer,
    lambda_net_update_t user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    uint64_t         local_size = local_ldo.GetHeaderSize() + local_ldo.GetMetaSize() + local_ldo.GetDataSize();
    local_ldo.GetHeaderRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_PUT;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset;
    base_wr.length        = std::min(local_size, (uint64_t)nbr->length);

    nnti::datatype::nnti_event_callback *put_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        put_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        put_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *put_cb);

    t_->put(&wr, &wid);
}

/*
 * @brief Write a subset of an LDO to @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] local_ldo      The source of the PUT.
 * @param[in] local_offset   The offset into the source buffer.
 * @param[in] remote_buffer  The destination of the PUT.
 * @param[in] remote_offset  The offset into the destination buffer.
 * @param[in] length         The length of the PUT.
 * @param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided write to remote_buffer on peer from local_ldo.
 * user_cb is invoked after the write completes.
 */
void Put(
    peer_t          *peer,
    DataObject       local_ldo,
    uint64_t         local_offset,
    NetBufferRemote *remote_buffer,
    uint64_t         remote_offset,
    uint64_t         length,
    lambda_net_update_t user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    local_ldo.GetHeaderRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_PUT;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset + local_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset + remote_offset;
    base_wr.length        = length;

    nnti::datatype::nnti_event_callback *put_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        put_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        put_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *put_cb);

    t_->put(&wr, &wid);
}

/*
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] local_ldo      The result of the atomic buffer.
 * @param[in] local_offset   The offset into the result buffer.
 * @param[in] remote_buffer  The target of the atomic operation.
 * @param[in] remote_offset  The offset into the target buffer.
 * @param[in] length         The length of the atomic operation.
 * @param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation on peer at remote_buffer.
 * length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject           local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    lambda_net_update_t  user_cb)
{
    return;
}

/*
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] local_ldo      The result of the atomic buffer.
 * @param[in] local_offset   The offset into the result buffer.
 * @param[in] remote_buffer  The target of the atomic operation.
 * @param[in] remote_offset  The offset into the target buffer.
 * @param[in] length         The length of the atomic operation.
 * @param[in] operand        The operand of the atomic operation.
 * @param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation with one operand on peer at
 * remote_buffer.  length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject           local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    int64_t              operand,
    lambda_net_update_t  user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    local_ldo.GetDataRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_ATOMIC_FADD;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset + local_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset + remote_offset;
    base_wr.operand1      = operand;
    base_wr.length        = 8;

    nnti::datatype::nnti_event_callback *atomic_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        atomic_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        atomic_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *atomic_cb);

    t_->atomic_fop(&wr, &wid);
}

/*
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * @param[in] peer           A handle to the target peer.
 * @param[in] local_ldo      The result of the atomic buffer.
 * @param[in] local_offset   The offset into the result buffer.
 * @param[in] remote_buffer  The target of the atomic operation.
 * @param[in] remote_offset  The offset into the target buffer.
 * @param[in] length         The length of the atomic operation.
 * @param[in] operand1       The first operand of the atomic operation.
 * @param[in] operand2       The second operand of the atomic operation.
 * @param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation with two operands on peer at
 * remote_buffer.  length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject           local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    int64_t              operand1,
    int64_t              operand2,
    lambda_net_update_t  user_cb)
{
    NNTI_work_request_t base_wr = NNTI_WR_INITIALIZER;
    NNTI_work_id_t      wid;

    NntiBufferRemote *nbr = (NntiBufferRemote *)remote_buffer;

    NNTI_peer_t peer_hdl = peer->p;

    NNTI_buffer_t remote_hdl;
    t_->dt_unpack((void*)&remote_hdl, &nbr->packed[0], MAX_NET_BUFFER_REMOTE_SIZE-8);

    NntiBufferLocal *local_bl;
    uint32_t         local_bl_offset;
    local_ldo.GetDataRdmaHandle( (void **)&local_bl, local_bl_offset);

    base_wr.op            = NNTI_OP_ATOMIC_CSWAP;
    base_wr.flags         = NNTI_OF_LOCAL_EVENT;
    base_wr.trans_hdl     = nnti::transports::transport::to_hdl(t_);
    base_wr.peer          = peer_hdl;
    base_wr.local_hdl     = local_bl->nnti_buffer;
    base_wr.local_offset  = local_bl_offset + local_offset;
    base_wr.remote_hdl    = remote_hdl;
    base_wr.remote_offset = nbr->offset + remote_offset;
    base_wr.operand1      = operand1;
    base_wr.operand2      = operand2;
    base_wr.length        = 8;

    nnti::datatype::nnti_event_callback *atomic_cb = nullptr;
    if (user_cb) {
        user_invoking_callback *uicb = new user_invoking_callback(user_cb, new DataObject(local_ldo));
        atomic_cb = new nnti::datatype::nnti_event_callback(t_, *uicb);
    } else {
        default_callback *dcb = new default_callback();
        atomic_cb = new nnti::datatype::nnti_event_callback(t_, *dcb);
    }

    nnti::datatype::nnti_work_request wr(t_, base_wr, *atomic_cb);

    t_->atomic_cswap(&wr, &wid);
}

}
}
