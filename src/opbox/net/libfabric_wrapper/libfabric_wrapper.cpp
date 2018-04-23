// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <sstream>

#include <sys/unistd.h>		//gethostname
#include <assert.h>

#include "opbox/net/net.hh"
#include "opbox/net/peer.hh"

#include "webhook/Server.hh"
#include "webhook/client/Client.hh"
#include "webhook/common/QuickHTML.hh"

#include "fab_transport.hh"

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <vector>
#include <algorithm>

#include <rdma/fabric.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_rma.h>
#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "common/Configuration.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "opbox/common/Types.hh"



using namespace net;
using namespace std;
using namespace opbox;
using namespace faodel;
using namespace lunasa;
using namespace webhook;

namespace opbox {
namespace net {

bool configured_;
bool initialized_;
bool started_;

faodel::Configuration config_;

fab_transport  *fabtrns;

lunasa::Lunasa *lunasa_;

std::map<faodel::nodeid_t, opbox::net::peer_t *>  node_peermap;


class initiator_callback {
private:
    std::function< WaitingType(OpArgs *args) > user_cb_;
    struct fab_buf *send_buf;

    UpdateType completion_to_update_type(struct fi_cq_msg_entry  entry) {
        switch(entry.flags) {
            case FI_SEND:  return UpdateType::send_success;
            case FI_WRITE: return UpdateType::put_success;
            case FI_READ:  return UpdateType::get_success;
            case FI_ATOMIC: return UpdateType::atomic_success;
            default: abort();
        }
    }

public:
    initiator_callback() {
        return;
    }

    initiator_callback(std::function< WaitingType(OpArgs *args) > user_cb, struct fab_buf *buf) {
        user_cb_ = user_cb;
        send_buf = buf;
        return;
    }

    int operator() (struct fi_cq_msg_entry entry, struct fab_buf *buf) {
        int rc = 0;
        /*
        faodel::nodeid_t  nodeid= *entry.op_context;

        cout <<"fab wrapper_callback initiator callback" << std::endl;

        peer_t    sender(faodel::ConvertNodeIdToPeer(nodeid));
        OpArgs    args;
        results_t results;
        args.type                 = completion_to_update_type(entry);
        args.data.msg.sender      = &sender;
        args.data.msg.ptr = (message_t*)((char*)buf->buf + buf->offset);

        WaitingType cb_rc = user_cb_(args, &results);
         */
        return rc;
    }
};

initiator_callback *client_cb;

struct fabBufferRemote {
    uint64_t base;
    uint32_t offset;
    uint32_t length;
    uint64_t key;
};

struct fabBufferLocal : NetBufferLocal {
    struct fab_buf fbuf;

    void makeRemoteBuffer(size_t           remote_offset,   // in
        size_t           remote_length,   // in
        NetBufferRemote *remote_buffer);  // out
};

void fabBufferLocal::makeRemoteBuffer(
    size_t           remote_offset,   // in
    size_t           remote_length,   // in
    NetBufferRemote *remote_buffer) { // out

    auto rb = reinterpret_cast<fabBufferRemote *>(remote_buffer);

    rb->offset = remote_offset;
    rb->length = remote_length;
    rb->base   = (uint64_t)fbuf.buf;
    rb->key    = fbuf.key;
    //cout << "Make Remote buffer addr base " << rb->base << " key " << rb->key << std::endl;
    //cout << "Make Remote buffer offset " << rb->offset << " length " << rb->length << std::endl;

}

void RegisterMemory(void *base_addr, size_t length, void *&pinned) {
    auto local=new fabBufferLocal();
    fabtrns->register_memory(base_addr, length, &local->fbuf);
    pinned = (void*)local;
}

void UnregisterMemory(void *&pinned) {
    auto local = reinterpret_cast<fabBufferLocal *>(pinned);
    delete local;
    pinned = NULL;
}

void Configure(faodel::Configuration &config) {
    //cout <<"Net  configure\n";
    config_ = config;  // make a copy
    configured_=true;
}

/*
 *
 * Create an DataObject that can be used for zero copy sends.
 *
 */
DataObject NewMessage(uint64_t size) {
    //FAB_RECV_SIZE 4096 for each cmd message receive
    uint32_t meta_size = 0;
    DataObject ldo(meta_size, size,  DataObject::AllocatorType::eager);
    return std::move(ldo);
}

void RegisterRecvCallback(std::function<void(opbox::net::peer_ptr_t, opbox::message_t*)> recv_cb) {
    fabtrns->recv_cb_ = recv_cb;
}

void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject          local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    lambda_net_update_t  user_cb) {

    KTODO("Libfabric wrapper atomic");
    return;
}

/*
 * Execute a one-sided atomic operation with one operand on peer at
 * remote_buffer.  length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject          local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    uint64_t             operand,
    lambda_net_update_t  user_cb) {
    KTODO("Libfabric wrapper atomic");
    return;
}

void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    DataObject          local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,         // TODO: this param is ignored because we only do 64-bit atomics
    uint64_t             operand1,
    uint64_t             operand2,
    lambda_net_update_t  user_cb) {
    KTODO("Libfabric wrapper atomic");
    return;
}


void Put(peer_t          *peer,
    DataObject      local_ldo,
    NetBufferRemote *remote_buffer,
    std::function< WaitingType(OpArgs *args) > user_cb){

    //cout << "Calling put " << std::endl;

    auto nbr = reinterpret_cast<fabBufferRemote *>(remote_buffer);
    struct fi_rma_iov remote;
    struct fab_peer* fpeer = peer->p;

    fabBufferLocal   *local;
    uint32_t         local_bl_offset;
    //local_ldo.getRdmaPtr( (void **)&local, local_bl_offset);
    local_ldo.GetBaseRdmaHandle( (void **)&local, local_bl_offset); //CDU: FIX
    remote.addr = nbr->base + nbr->offset;
    //remote.addr = nbr->base;
    remote.key = nbr->key;

    //cout << "Put Remote buffer addr base " << nbr->base << " key " << nbr->key << std::endl;
    //cout << "Put Remote buffer offset " << nbr->offset << " length " << nbr->length << std::endl;
    fabtrns->Put(fpeer,&local->fbuf, local_ldo, remote, user_cb);

    return;
}

void Put(
    peer_t          *peer,
    DataObject      local_ldo,
    uint64_t         local_offset,
    NetBufferRemote *remote_buffer,
    uint64_t         remote_offset,
    uint64_t         length,
    std::function< WaitingType(OpArgs *args) > user_cb) {
    //cout << "Calling put " << std::endl;

    auto nbr = reinterpret_cast<fabBufferRemote *>(remote_buffer);
    struct fi_rma_iov remote;
    struct fab_peer* fpeer = peer->p;

    fabBufferLocal   *local;
    uint32_t         local_bl_offset;
    //local_ldo.getRdmaPtr( (void **)&local, local_bl_offset);
    local_ldo.GetBaseRdmaHandle( (void **)&local, local_bl_offset); //CDU: FIX
    remote.addr = nbr->base + nbr->offset + remote_offset;
    remote.key = nbr->key;

    fabtrns->Put(fpeer,&local->fbuf, local_ldo, local_offset, remote, length, user_cb);
    return;
}

void Get(
    peer_t          *peer,
    NetBufferRemote *remote_buffer,
    DataObject      local_ldo,
    std::function< WaitingType(OpArgs *args) > user_cb) {

    auto nbr = reinterpret_cast<fabBufferRemote *>(remote_buffer);
    struct fi_rma_iov remote;
    struct fab_peer* fpeer = peer->p;

    fabBufferLocal   *local;
    uint32_t         local_bl_offset;
    //local_ldo.getRdmaPtr( (void **)&local, local_bl_offset);
    local_ldo.GetBaseRdmaHandle( (void **)&local, local_bl_offset); //CDU: FIX
    remote.addr = nbr->base + nbr->offset;
    //remote.addr = nbr->base;
    remote.key = nbr->key;

    //cout << "Put Remote buffer addr base " << nbr->base << " key " << nbr->key << std::endl;
    //cout << "Put Remote buffer offset " << nbr->offset << " length " << nbr->length << std::endl;
    fabtrns->Get(fpeer,&local->fbuf, local_ldo, remote, user_cb);

    return;
}

void Get(
    peer_t          *peer,
    NetBufferRemote *remote_buffer,
    uint64_t         remote_offset,
    DataObject      local_ldo,
    uint64_t         local_offset,
    uint64_t         length,
    std::function< WaitingType(OpArgs *args) > user_cb) {

    auto nbr = reinterpret_cast<fabBufferRemote *>(remote_buffer);
    struct fi_rma_iov remote;
    struct fab_peer* fpeer = peer->p;

    fabBufferLocal   *local;
    uint32_t         local_bl_offset;
    //local_ldo.getRdmaPtr( (void **)&local, local_bl_offset);
    local_ldo.GetBaseRdmaHandle( (void **)&local, local_bl_offset); //CDU: FIX
    remote.addr = nbr->base + nbr->offset + remote_offset;
    remote.key = nbr->key;

    fabtrns->Get(fpeer,&local->fbuf, local_ldo, local_offset, remote, length, user_cb);
    return;

}

string GetDriverName(){
    return "libfabric";
}


void GetAttrs(opbox::net::Attrs &attrs) {
    attrs.mtu            = FAB_MTU_SIZE;
    attrs.max_eager_size = FAB_MTU_SIZE;
    //attrs.max_eager_size = 3900;
}

int Connect(
    peer_ptr_t *peer,
    const char *peer_addr,
    const char *peer_port)
{
    faodel::nodeid_t nodeid = faodel::nodeid_t(peer_addr, peer_port);
    return Connect(peer, nodeid);
}

int Connect(
    peer_ptr_t        *peer,
    faodel::nodeid_t  peer_nodeid)
{
    int rc;
    size_t addrlen;
    void *ep_name;
    string result;
    uint32_t src_port;
    struct fab_peer *p;

    stringstream ss_path;

    switch (fabtrns->my_transport_id) {
        case 1:
            // IB client creates a connection with known remote_nodeid, ep  etc and return fab_peer
            //cout << "Calling connect in wrapper verbs case" << " peer nodeid IP   " << peer_nodeid.GetIP() << " peer port : " <<peer_nodeid.GetPort() << std::endl;
            p = fabtrns->client_connect_ib (peer_nodeid);
            p->remote_nodeid = peer_nodeid;
            *peer =  new peer_t(p);
            break;
        case 2: // EP_RDM type
        case 3:
            p = fabtrns->create_rdm_connection_client(peer_nodeid);
            p->remote_nodeid = peer_nodeid;
            *peer =  new peer_t(p);
            break;

        default:
//            cout << "Calling connect in wrapper default case" << std::endl;
            break;
    }

    return 0;
}

int Disconnect(
    peer_ptr_t peer)
{
    return 0;
}

int Disconnect(
    faodel::nodeid_t peer_nodeid)
{
    struct fab_peer *p = fabtrns->find_peer(peer_nodeid);

    if (p == nullptr) {
        return -1;
    }
    return Disconnect((peer_ptr_t)p);
}


faodel::nodeid_t GetMyID() {
    int rc=0;
    return fabtrns->mynodeid;
}


void Start() {

    int rc = 0;
    int i, ret = 0;
    std::string trans_name;
    std::string name_key("net.transport.name");  // verbs, gni, sockets for now

    if(0 != config_.GetLowercaseString(&trans_name, name_key)) {
        std::cerr << "NetLibfabric -> Provider name (verbs/sockets/gni) not specified.  Defaulting to 'sockets'." << std::endl;;
        trans_name = "sockets";
    }
    assert(webhook::Server::IsRunning() && "Webhook not started before fabric started");
    fabtrns->mynodeid = webhook::Server::GetNodeID();
    if((trans_name == "gni") || (trans_name == "ugni")) {
        fabtrns->my_transport_id = 2;
        fabtrns->fab_init_rdm();
    } else if((trans_name == "verbs") || (trans_name == "ibverbs")) {
        fabtrns->my_transport_id = 1;
        fabtrns->fab_init_ib();
    } else if(trans_name == "sockets") {
        fabtrns->my_transport_id = 3;
        fabtrns->fab_init_rdm();
    } else if(trans_name == "mpi") {
        std::cerr << "NetLibfabric -> net.transport.name has unsupported value 'mpi'.  Failing back to 'sockets'." << std::endl;
        fabtrns->my_transport_id = 3;
        fabtrns->fab_init_rdm();
    } else {
        std::cerr << "NetLibfabric -> net.transport.name has unknown value '" << trans_name << "'.  Failing back to 'sockets'." << std::endl;
        fabtrns->my_transport_id = 3;
        fabtrns->fab_init_rdm();
    }
    //cout << "after calling init and before starting threads\n";

    lunasa::RegisterPinUnpin(RegisterMemory, UnregisterMemory);

    fabtrns->start();
}

void Start(const faodel::Configuration &config) {

}


void Finish() {
//    cout <<"Libfabric::Finish()" << endl;
    fabtrns->stop();
}

void ReleaseMessage(DataObject msg){
    //  delete msg;
}

faodel::nodeid_t ConvertPeerToNodeID(opbox::net::peer_t *peer) {
    auto p = reinterpret_cast<struct fab_peer *>(&peer);
    return faodel::nodeid_t(p->remote_nodeid);
}

opbox::net::peer_ptr_t ConvertNodeIDToPeer(faodel::nodeid_t nodeid) {
    KTODO("ConvertNodeIDToPeer");
}

void SendMsg(peer_t     *peer,
    DataObject msg,
    std::function< WaitingType(OpArgs *args) > user_cb) {

    struct fab_peer *fpeer = peer->p;
    fabBufferLocal  *msg_bl = NULL;
    uint32_t         msg_bl_offset = 0;

    //int rc = msg.getRdmaPtr( (void **)&msg_bl, msg_bl_offset);
    int rc = msg.GetBaseRdmaHandle( (void **)&msg_bl, msg_bl_offset); //CDU: fix
    if(rc != 0) {
        fprintf(stderr, "msg->GetRdmaPtr() failed: %d", rc);
        abort();
    }
    fabtrns->Send(fpeer, &msg_bl->fbuf, msg, user_cb);
}

void Init(const faodel::Configuration &config) {
    config_ = config; // Make a copy of config for own to use in Start()
    fabtrns = fabtrns->get_instance();
    //cout << "Net libfabric init Done\n";
}


void SendMsg(peer_t *remote_peer, DataObject msg) {

    struct fab_peer *fpeer = remote_peer->p;
    fabBufferLocal *msg_bl        = NULL;
    uint32_t         msg_bl_offset = 0;

    //int rc = msg.getRdmaPtr( (void **)&msg_bl, msg_bl_offset);
    int rc = msg.GetBaseRdmaHandle( (void **)&msg_bl, msg_bl_offset); //CDU: FIX
    if(rc != 0) {
        fprintf(stderr, "msg->GetRdmaPtr() failed: %d", rc);
        abort();
    }
    fabtrns->Send(fpeer, &msg_bl->fbuf, msg, NULL);
}


namespace internal {

//CDU: FIX GetOffset was missing
uint32_t GetOffset(
    opbox::net::NetBufferRemote *nbr) // in
{
    fabBufferRemote *b = (fabBufferRemote *)nbr;
    return b->offset;
}

uint32_t GetLength(opbox::net::NetBufferRemote *nbr) { // in

    fabBufferRemote *b = (fabBufferRemote *)nbr;
    return b->length;
}

int IncreaseOffset(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              addend)    {  // in

    fabBufferRemote *b = (fabBufferRemote *)nbr;
    if ((addend >= 0) && (addend < b->length)) {
        b->offset += addend;
        b->length -= addend;
    } else {
        return -1;
    }
    return 0;
}

int DecreaseLength(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              subtrahend) { // in

    fabBufferRemote *b = (fabBufferRemote *)nbr;
    if ((subtrahend >= 0) && (subtrahend < b->length)) {
        b->length -= subtrahend;
    } else {
        return -1;
    }
    return 0;
}

int TrimToLength(
    opbox::net::NetBufferRemote *nbr,      // in
    uint32_t              length)   {      // in
    fabBufferRemote *b = (fabBufferRemote *)nbr;
    if ((length > 0) && (length < b->length)) {
        b->length = length;
    } else {
        return -1;
    }
    return 0;
}

} // namespece internal

} // namespace net
} // namespace opbox
