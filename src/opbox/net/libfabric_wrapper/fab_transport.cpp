// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <iostream>
#include <sstream>

#include <sys/unistd.h> //gethostname
#include <assert.h>
#include "opbox/net/net.hh"
#include "opbox/net/peer.hh"

#include "webhook/Server.hh"
#include "webhook/client/Client.hh"
#include "faodel-common/QuickHTML.hh"

#include <boost/algorithm/string.hpp>

#include "opbox/net/libfabric_wrapper/shared.hh"
#include "opbox/net/libfabric_wrapper/fab_transport.hh"

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
#include <rdma/fi_atomic.h>

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


#include "faodel-common/Configuration.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "opbox/common/Types.hh"



using namespace std;
using namespace opbox;
using namespace opbox::net;
using namespace faodel;
using namespace lunasa;

namespace opbox
{
namespace net
{

fab_transport* fab_transport::single_fab = NULL;
std::mutex conn_mutex;
std::mutex compq_mutex;
static std::map <faodel::nodeid_t,fab_peer * > peer_map;
typedef std::map <faodel::nodeid_t,fab_peer * >::iterator nodeid_peer_iter_t;

#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 8
#endif
void
hexdump(
    void *mem,
    unsigned int len)
{
    unsigned int i, j;

    for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
        /* print offset */
        if(i % HEXDUMP_COLS == 0) {
            printf("0x%06x: ", i);
        }
        /* print hex data */
        if(i < len) {
            printf("%02x ", 0xFF & ((char*)mem)[i]);
        } else {
            /* end of block, just aligning for ASCII dump */
            printf("   ");
        }

        /* print ASCII dump */
        if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
            for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
                if(j >= len) {
                    /* end of block, not really printing */
                    putchar(' ');
                } else if(isprint(((char*)mem)[j])) {
                    /* printable char */
                    putchar(0xFF & ((char*)mem)[j]);
                } else {
                    /* other char */
                    putchar('.');
                }
            }
            putchar('\n');
        }
    }
}

#define FAB_PRINTERR(call, retv) \
do {fprintf(stderr, call "(): %s:%d, ret=%d (%s)\n", __FILE__, __LINE__, (int) retv, fi_strerror((int) -retv));} while (0)

//default constructor
fab_transport::fab_transport()
{

}

fab_transport::fab_transport(
    faodel::Configuration &config)
{

}

fab_transport*
fab_transport::get_instance()
{
    if(single_fab==NULL) single_fab = new fab_transport();
    return single_fab;
}

//This is to print verbose  fi_info
static int
print_long_info(
    struct fi_info *info)
{
    for (struct fi_info *cur = info; cur; cur = cur->next) {
        fprintf(stderr, "---\n");
        fprintf(stderr, "%s", fi_tostr(cur, FI_TYPE_INFO));
    }
    return 0;
}

//This is to print verbose  fi_info
fi_info*
select_fi_info(
    struct fi_info *info,
    struct fi_info *hints)
{
//    fprintf(stderr, "---%d  %s\n", hints->ep_attr->type, hints->fabric_attr->prov_name);
    for (struct fi_info *cur = info; cur; cur = cur->next) {
//        fprintf(stderr, "cur ---%d  %s\n", cur->ep_attr->type, cur->fabric_attr->prov_name);
        if((cur->ep_attr->type == hints->ep_attr->type) && strcmp(cur->fabric_attr->prov_name, hints->fabric_attr->prov_name) == 0) {
//            fprintf(stderr, "%s", fi_tostr(cur, FI_TYPE_INFO));
            return cur;
        }
    }
    return NULL;
}

int
error_check(
    int ret,
    std::string err_string)
{
    if (ret) {
        fprintf(stderr, "%s\n",err_string.c_str(), ret);
        abort();
    } else {
        return FI_SUCCESS;
    }
}

// Error entry reading event queue
//
void
fab_transport::eq_readerr(
    struct fid_eq *eq,
    const char *eq_str)
{
    struct fi_eq_err_entry eq_err;
    const char *err_str;
    int rd;

    rd = fi_eq_readerr (eq, &eq_err, 0);
    if (rd != sizeof (eq_err)) {
        fprintf (stderr, "ERROR: fi_eq_readerr\n");
    } else {
        err_str = fi_eq_strerror (eq, eq_err.prov_errno, eq_err.err_data, NULL, 0);
        fprintf (stderr, "%s: %d %s\n", eq_str, eq_err.err,
            fi_strerror (eq_err.err));
        fprintf (stderr, "%s: prov_err: %s (%d)\n", eq_str, err_str,
            eq_err.prov_errno);
    }
}

void
fab_transport::print_addr(fid_ep *ep)
{
    void *localaddr;
    void *remoteaddr;
    size_t addrlen;
    char* src_addr;
    uint16_t src_port;
    char* dst_addr;
    uint16_t dst_port;


    if (ep !=NULL) {
        localaddr = malloc(fi->src_addrlen);
        remoteaddr = malloc(fi->src_addrlen);
        fi_getname (&ep->fid, localaddr, &addrlen);
        assert(addrlen!=0);

        if(localaddr !=NULL) {
            src_addr = (char*) inet_ntoa(((struct sockaddr_in *) localaddr)->sin_addr);
            src_port = (uint16_t) ntohs (((struct sockaddr_in *) localaddr)->sin_port);
        }
        fi_getpeer (ep, remoteaddr, &addrlen);
        if(remoteaddr !=NULL) {
            dst_addr = (char*) inet_ntoa(((struct sockaddr_in *) remoteaddr)->sin_addr);
            dst_port = (uint16_t) ntohs (((struct sockaddr_in *) remoteaddr)->sin_port);
        }
//        cout << "inside print addr - " << "local addr: "<< src_addr << " local port: " << src_port << "remote addr: " << dst_addr << " remote port: " << dst_port << std::endl;
    } else {
//        cout << "ep is NULL" << std::endl;
    }
}

//// Transport related methods
void
fab_transport::start()
{
    int ret;

    operand1_ptr = (int64_t*)malloc(sizeof(int64_t));
    operand2_ptr = (int64_t*)malloc(sizeof(int64_t));

    ret = fi_mr_reg(domain,
                    operand1_ptr,
                    sizeof(int64_t),
                    FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE,
                    0,
                    FAB_MR_KEY,
                    0,
                    &operand1_mr,
                    NULL);
    error_check(ret,"fi_mr_reg");

    ret = fi_mr_reg(domain,
                    operand2_ptr,
                    sizeof(int64_t),
                    FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE,
                    0,
                    FAB_MR_KEY,
                    0,
                    &operand2_mr,
                    NULL);
    error_check(ret,"fi_mr_reg");

    //setupRecvQueue();  Now implementing per connection recv buffer
    if(my_transport_id == 1){
        start_ib_connection_thread();
    }
    start_progress_thread();
}

void
fab_transport::stop()
{
    shutdown_requested = true;

    if(my_transport_id == 1){ //verbs
        stop_connection_thread();
    }
    stop_progress_thread();
//    cout << "Transport closing" << std::endl;
}

void
fab_transport::register_memory (
    void *base_addr,
    int length,
    struct fab_buf *send_buf)
{
    int ret =0;
    ret = fi_mr_reg(domain, base_addr, length, FI_RECV | FI_SEND | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE,
                    0, FAB_MR_KEY, 0, &send_buf->buf_mr, NULL);
    error_check(ret,"fi_mr_reg");
    send_buf->buf = (uint64_t)static_cast<void*>(base_addr);
    send_buf->offset = 0;
    send_buf->len = length;
    send_buf->key = fi_mr_key(send_buf->buf_mr);
    //cout << "Came to register memory buffer  addr " << send_buf->buf << " key  "<< send_buf->key << std::endl;
}

void
fab_transport::unregister_memory (
    struct fab_buf *send_buf)
{
    int ret =0;
    ret = fi_close(&send_buf->buf_mr->fid);
    error_check(ret,"fi_close");
}

fab_peer*
fab_transport::find_peer (
    faodel::nodeid_t nodeid)
{
    fab_peer *peer;

    std::lock_guard < std::mutex > lock (conn_mutex);
    if (peer_map.find (nodeid) != peer_map.end ()) {
        peer = peer_map[nodeid];        // add to connection map
    } else {
        peer = NULL;
    }
    return peer;
}

void
fab_transport::setupRecvQueue()
{
    int ret;
    fid_mr *mr;
    struct fab_buf rx_buf;
    for (int i=0;i<10;i++) {
        uint32_t meta_size = 0;
        DataObject ldo(meta_size, fi->ep_attr->max_msg_size, DataObject::AllocatorType::eager);
        //ret = fi_mr_reg(domain, ldo.rawPtr(), ldo.rawSize(), FI_RECV, 0, 0, 0, &mr, NULL);
        //CDU: TODO should the length be replaced with a better header+meta+data size?
        ret = fi_mr_reg(domain,
                        ldo.internal_use_only.GetHeaderPtr(),
                        ldo.GetHeaderSize()+meta_size+fi->ep_attr->max_msg_size, FI_RECV, 0, 0, 0, &mr, NULL); //CDU-check
        error_check(ret,"fi_mr_reg");
        if (ret ==FI_SUCCESS) {
            //rx_buf.buf= (uint64_t)ldo.rawPtr();
            rx_buf.buf= (uint64_t)ldo.internal_use_only.GetHeaderPtr();
            rx_buf.offset=0;
            rx_buf.len = ldo.GetHeaderSize()+meta_size+fi->ep_attr->max_msg_size;// ldo.rawSize(); //CDU
            rx_buf.buf_mr = mr;
        }
        recv_buffers_.push_back(rx_buf);
    }
}

void
fab_transport::teardownRecvQueue()
{
    while(!recv_buffers_.empty()) {
        fab_buf  rbuf = recv_buffers_.front();
        recv_buffers_.pop_front();
    }
}

void
fab_transport::start_progress_thread(void)
{
    progress_thread_ = std::thread(&fab_transport::check_completion, this);
}
void
fab_transport::stop_progress_thread(void)
{
    fi_cq_signal(cq);
    progress_thread_.join();
}

void
print_fi_wc(struct fi_cq_data_entry *wc)
{
    stringstream ss;
    int indent=0;

    ss << string(indent,' ')  <<"[wc] "<<endl
       << string(indent+1,' ')<<"flags:    "<<std::hex<<wc->flags<<std::dec<<endl
       << string(indent+1,' ')<<"len:      "<<wc->len<<endl
       << string(indent+1,' ')<<"buf:      "<<wc->buf<<endl
       << string(indent+1,' ')<<"data:     "<<wc->data<<endl;
    cout << ss.str();
}

void
print_fab_peer(stringstream &ss, struct fab_peer *peer, int indent=0)
{
    ss << string(indent,' ')  <<"[fab_peer] "<<endl
       << string(indent+1,' ')<<"ep_addr:            "<<peer->ep_addr<<endl
//       << string(indent+1,' ')<<"dst_addr:           "<<peer->dst_addr<<endl
//       << string(indent+1,' ')<<"dst_port:           "<<peer->dst_port<<endl
       << string(indent+1,' ')<<"remote_addr:        "<<peer->remote_addr<<endl
       << string(indent+1,' ')<<"remote_nodeid:      "<<peer->remote_nodeid.GetHex()<<endl
       << string(indent+1,' ')<<"rem_addrlen:        "<<peer->rem_addrlen<<endl;
}

void
print_fab_recvreq(struct fab_recvreq *rreq)
{
    stringstream ss;
    int indent=0;

    ss << string(indent,' ')  <<"[rreq] "<<endl
       << string(indent+1,' ')<<"repost_buf:    "<<(void*)rreq->repost_buf<<endl
       << string(indent+1,' ')<<"mr:            "<<rreq->mr<<endl
       << string(indent+1,' ')<<"len:           "<<rreq->len<<endl
       << string(indent+1,' ')<<"offset:        "<<rreq->offset<<endl
       << string(indent+1,' ')<<"peer:          "<<rreq->peer<<endl;
    print_fab_peer(ss, rreq->peer, 4);
    cout << ss.str();
}

int
fab_transport::check_completion ()
{
    struct fi_cq_data_entry wc;
    struct fi_cq_err_entry cq_err;
    struct fab_recvreq* rreq;
    struct fab_op_context* context;
    int rd, rc;

    while(!shutdown_requested) {
        do {
            rd = fi_cq_read (cq, &wc, 1);
        } while ((rd == -FI_EAGAIN) && !shutdown_requested);

//        print_fi_wc(&wc);

        if (shutdown_requested == true) {
            return 0;
        }

        if (rd < 0) {
            fi_cq_readerr (cq, &cq_err, 0);
            fprintf (stderr, "cq fi_cq_readerr() %s (%d)\n",
                fi_cq_strerror (cq, cq_err.err, cq_err.err_data, NULL, 0),
                cq_err.err);
            cerr << "read error on RECV CQ flags " << cq_err.flags << " provider errno "<< cq_err.prov_errno <<std::endl;
            return -1;
        }
        if (wc.flags & FI_RECV) {
//            fprintf(stdout, "got a RECV completion - wc.flags=%X\n", wc.flags);
            if(wc.op_context != NULL) {
//                cout << "successful recv - wc.flags=" << std::hex << wc.flags << std::dec << " length " << wc.len << std::endl;
                rreq=(fab_recvreq*)wc.op_context;
                peer_t *sender = new peer_t(rreq->peer);
                message_t *msg = (message_t*)((char*) rreq->repost_buf);
                fab_peer *msg_src_peer = find_peer(rreq->peer->remote_nodeid);
                if (!(rreq->peer->remote_nodeid == msg_src_peer->remote_nodeid)) {
                    cout << "************** rreq->peer != msg->src -- using msg->src" << endl;
                    abort();
                    delete sender;
                    sender = new peer_t(msg_src_peer);
                }
//                cout << "incoming message from " << rreq->peer->remote_nodeid.GetHex() << endl;
//                cout << msg->str() << endl;
                recv_cb_(sender, msg);
                rc = fi_recv(rreq->peer->ep_addr, (char*) rreq->repost_buf, FAB_MTU_SIZE , fi_mr_desc(rreq->mr),
                             0, rreq);
            }
        } else if (wc.flags & FI_SEND) {
//            fprintf(stdout, "got a SEND completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::send_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else if (wc.flags == (FI_RMA | FI_READ)) {
//            fprintf(stdout, "got a RMA+READ completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::get_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else if (wc.flags == (FI_RMA | FI_WRITE)) {
//            fprintf(stdout, "got a RMA+WRITE completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::put_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else if (wc.flags == (FI_ATOMIC | FI_READ)) {
//            fprintf(stdout, "got a ATOMIC+READ completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::atomic_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else if (wc.flags == (FI_ATOMIC | FI_WRITE)) {
//            fprintf(stdout, "got a ATOMIC+WRITE completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::atomic_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else if (wc.flags == FI_ATOMIC) {
//            fprintf(stdout, "got a ATOMIC completion - wc.flags=%X\n", wc.flags);
            context = (struct fab_op_context*)wc.op_context;
            OpArgs args(UpdateType::atomic_success);
            if (context->user_cb) {
//                fprintf(stdout, "invoking user callback\n");
                WaitingType cb_rc = context->user_cb(&args);
            }
//            fprintf(stdout, "check_completion - ldo value=%X\n", *(uint64_t*)((char *)context->ldo.GetDataPtr() + context->loffset));
        } else {
            fprintf(stdout, "got completion with unknown flags - wc.flags=%X\n", wc.flags);
        }
    }
    return 0;
}

int
fab_transport::init_endpoint(
    fid_ep *ep)
{
    int ret;

    ret = fi_endpoint(domain, fi, &ep, NULL);
    error_check(ret, "fi_endpoint");

    ret = fi_ep_bind(ep, &av->fid, 0);
    error_check(ret, "fi_ep_bind");

    ret = fi_ep_bind(ep, &cq->fid, FI_SEND|FI_RECV);
    error_check(ret, "fi_ep_bind");

    ret = fi_enable(ep);
    error_check(ret, "fi_enable");

    return ret; //TODO: Is this right?
}

void
fab_transport::create_rdm_connection_server(
    const std::map<std::string,std::string> &args,
    std::stringstream &results)
{
    int ret;
    void   *nep_name;
    size_t  len;
    struct fab_peer* peer=new fab_peer();
    struct fab_recvbuf* rcvbuf=new fab_recvbuf();
    string value;
    uint64_t offset;
    struct fi_info *my_fi;
    string hostname, port, rem_name, rem_port;

    auto new_val = args.find("rem_webhook_hostname");
    if(new_val != args.end()){
        hostname = new_val->second;
    }
    new_val = args.find("rem_webhook_port");
    if(new_val != args.end()){
        port = new_val->second;
    }
    new_val = args.find("rem_peer_port");
    if(new_val != args.end()){
        rem_port = new_val->second;
    }
    faodel::nodeid_t remote_nodeid(hostname, port);

//    cout << "inbound connection from " << remote_nodeid.GetHex() << endl;

    ret = fi_getinfo (FI_VERSION(1,0), hostname.c_str(), rem_port.c_str(), 0, hints, &my_fi);
    if (ret){
        cerr << "ERROR: RDM client_server for dest: fi_getinfo" << endl;
        exit(-1);
    }
    if(my_fi->dest_addr !=NULL){
        //hexdump(my_fi->dest_addr, my_fi->dest_addrlen);
        ret = fi_av_insert(av, my_fi->dest_addr, 1, &peer->remote_addr, 0, NULL);
        if (ret < 0) {
            FAB_PRINTERR("fi_av_insert", ret);
            exit(-1);
        }
    }

    peer->ep_addr = ep;
    peer->remote_nodeid = remote_nodeid;

    rcvbuf->buf = malloc(FAB_MTU_SIZE * FAB_NRECV); // post 1000 recv 4k each for each connection
    fi_mr_reg(domain,rcvbuf->buf, FAB_MTU_SIZE * FAB_NRECV, FI_RECV, 0, 0, 0, &rcvbuf->mr, NULL);
    for (int i=0; i< 1000; ++i){
        struct fab_recvreq* rreq = new fab_recvreq();
        offset = (FAB_MTU_SIZE * i);
        rreq->repost_buf = (uint64_t) ((char*)rcvbuf->buf + offset);
        rreq->peer       = peer;
        rreq->mr         = rcvbuf->mr;
//        print_fab_recvreq(rreq);
        ret = fi_recv(
            rreq->peer->ep_addr,
            (char*)rreq->repost_buf,
            FAB_MTU_SIZE,
            fi_mr_desc(rreq->mr),
            0,
            rreq);
    }
    std::lock_guard < std::mutex > lock (conn_mutex);
    peer_map[remote_nodeid] = peer;
    string s_host, s_port;
    mynodeid.GetIPPort(&s_host, &s_port);
    std::string s_port_aux = std::to_string(my_fab_port);
    results<< s_host<<"/"<<s_port_aux<<"\n";

//    fprintf(stdout, "create_rdm_connection_server(): result fab_peer=%p  fab_peer->nodeid=%s\n", peer, peer->remote_nodeid.GetHex().c_str());

    //Note: could this just be the hex nodeid?
//    cout << "gni_server_connection sent result" << results.str() << "\n";
}


int
fab_transport::fab_init_rdm(const char *provider_name)
{

    int ret = 0;
    struct fi_av_attr *av_attr;
    size_t addrlen;
    const char* port;
    void *ep_name;
    std::stringstream ss;

    hints = fi_allocinfo();
    if (!hints)
        return -1;

    hints->ep_attr->type = FI_EP_RDM;
//    hints->ep_attr->protocol = FI_PROTO_SOCK_TCP;
//    hints->caps = FI_MSG | FI_RMA;
    hints->caps = FI_MSG | FI_RMA | FI_ATOMIC;
    hints->mode = FI_LOCAL_MR | FI_CONTEXT;
//    hints->addr_format = FI_SOCKADDR_IN;
    hints->fabric_attr->prov_name = strdup(provider_name);
    hints->domain_attr->mr_mode = FI_MR_BASIC;

    //src_port = webhook::port();
    // have a global port relative to webhook port , which is unique to start libfabric as server connection
    // There is really no connection but this is to indetify the src_addr, dest_addr etc for webhook exchange
    //
    faodel::nodeid_t nid = webhook::Server::GetNodeID();;
    string s_host, s_port;
    uint32_t b_host; uint16_t b_port;

    nid.GetIPPort(&s_host, &s_port);
    nid.GetIPPort(&b_host, &b_port);
//    cout << "webhook " << s_host<<" / "<<s_port<<std::endl;
    my_fab_port = b_port + FAB_PORT_AUX;
    std::string s_port_aux = std::to_string(my_fab_port);
    ret = fi_getinfo(FI_VERSION(1,0), s_host.c_str(), s_port_aux.c_str(), FI_SOURCE, hints, &fi);

    if (ret) {
        cerr << "fi_getinfo in rdm init failed " << "return value " << ret << std::endl;
        FAB_PRINTERR("fi_getinfo", ret);
        return ret;
    }
    //ret = print_long_info(fi);
    ret = fi_fabric(fi->fabric_attr, &fabric, NULL);
    error_check (ret, "fi_fabric") ;

    ret = fi_domain(fabric, fi, &domain, NULL);
    error_check (ret, "fi_domain") ;

    av_attr = (struct fi_av_attr *)calloc(1, sizeof(*av_attr));
    av_attr->type = FI_AV_MAP;
    av_attr->count = 16;
    av_attr->name = NULL;
    if (fi->ep_attr->type == FI_EP_RDM) {
        ret = fi_av_open(domain, av_attr, &av, NULL);
        if (ret) {
            FAB_PRINTERR("fi_av_open", ret);
            return ret;
        }
    }
    cq_attr.format = FI_CQ_FORMAT_DATA;
    cq_attr.wait_obj = FI_WAIT_NONE;
    cq_attr.size = fi->tx_attr->size;

    ret = fi_cq_open(domain, &cq_attr, &cq, 0);
    error_check(ret, "fi_cq_open");

    ret = fi_endpoint(domain, fi, &ep, NULL);
    error_check(ret, "fi_endpoint");

    ret = fi_ep_bind(ep, &av->fid, 0);
    error_check(ret, "fi_ep_bind");

    ret = fi_ep_bind (ep, &cq->fid, FI_SEND|FI_RECV);
    error_check(ret, "fi_ep_bind");

    ret = fi_enable(ep);
    error_check(ret, "fi_enable");
    webhook::Server::registerHook("/fab/rdmlookup", [this] (const map<string,string> &args, stringstream &results) {
        create_rdm_connection_server(args, results);
    });
    //cout << "Fab init RDM done " << std::endl;
    initialized = true;

    return ret; //TODO: is this right?

}

fab_peer*
fab_transport::create_rdm_connection_client(
    faodel::nodeid_t   peer_nodeid)
{
    int ret;
    uint64_t offset=0;
    std::stringstream ss_path;
    string result;
    const char* remote_port;
    struct fi_info*  my_fi;
    struct fab_peer* peer=new fab_peer();
    struct fab_recvbuf* rcvbuf=new fab_recvbuf();

    std::string s = std::to_string(my_fab_port);

    fab_peer *existing_peer = find_peer(peer_nodeid);
    if (existing_peer != NULL) {
//        cout << "found existing connection to " << peer_nodeid.GetHex() << endl;
        return existing_peer;
    }

//    cout << "outbound connection to " << peer_nodeid.GetHex() << endl;

    ss_path << "/fab/rdmlookup&rem_webhook_hostname="<<mynodeid.GetIP()<<"&rem_webhook_port="<<mynodeid.GetPort()<< "&rem_peer_port=" <<s;
    ret = webhook::retrieveData(peer_nodeid.GetIP() , peer_nodeid.GetPort() , ss_path.str(), &result);
    boost::trim_right(result);
    vector<string> result_parts = faodel::SplitPath(result);
    //cout <<"Result is : '" << result_parts[0] << "' part 1  '" << result_parts[1] << "'\n";
    ret = fi_getinfo (FI_VERSION(1,0), result_parts[0].c_str(), result_parts[1].c_str(), 0, hints, &my_fi);
    if (ret){
        cerr << "ERROR: RDM client_connect: fi_getinfo" << endl;
        return NULL;
    }
    peer->rem_addrlen = my_fi->dest_addrlen;
    peer->ep_addr = ep;
    peer->remote_nodeid = peer_nodeid;
    if(my_fi->dest_addr !=NULL){
        //hexdump(my_fi->dest_addr, my_fi->dest_addrlen);
        ret = fi_av_insert(av, my_fi->dest_addr, 1, &peer->remote_addr, 0, NULL);
        if (ret < 0) {
            FAB_PRINTERR("fi_av_insert", ret);
            return NULL;
        }
    }
    rcvbuf->buf = malloc(FAB_MTU_SIZE * FAB_NRECV); // post 1000 recv 4k each for each connection
    fi_mr_reg(domain,rcvbuf->buf, FAB_MTU_SIZE * FAB_NRECV, FI_RECV, 0, 0, 0, &rcvbuf->mr, NULL);
    for (int i=0; i< 1000; ++i){
        struct fab_recvreq* rreq = new fab_recvreq();
        offset = (FAB_MTU_SIZE * i);
        rreq->repost_buf = (uint64_t) ((char*)rcvbuf->buf + offset);
        rreq->peer = peer;
        rreq->mr= rcvbuf->mr;
//        print_fab_recvreq(rreq);
        ret = fi_recv(ep, (char*)rcvbuf->buf + offset, FAB_MTU_SIZE , fi_mr_desc(rcvbuf->mr),
                      0, rreq);
    }
    if (peer != NULL) {
        peer_map[peer_nodeid] = peer;
    }

//    fprintf(stdout, "create_rdm_connection_client(): result fab_peer=%p  fab_peer->nodeid=%s\n", peer, peer->remote_nodeid.GetHex().c_str());

    return peer;
}



/* Infiniband related methods */

int
get_ipofIBaddr(
    char *hostname)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list of interfaces, maintaining head pointer so we
       can free list later */

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;
        family = ifa->ifa_addr->sa_family;
        /* For an AF_INET, display IB0  address, shoudl use faodel::config interface */

        if (family == AF_INET && !strncmp(ifa->ifa_name, "ib0",3)) {
            s = getnameinfo(ifa->ifa_addr,
                            sizeof(struct sockaddr_in),
                            hostname, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", fi_strerror(s));
                exit(EXIT_FAILURE);
            }
        }
    }
    freeifaddrs(ifaddr);
    return 0;
}


int
fab_transport::fab_init_ib (const char *provider_name)
{
    struct fi_info *cur_fi;
    int rx_depth_default = 500;
    int rc=0;
    struct fi_eq_attr eq_attr;
    struct fi_cq_attr cq_attr;
    string my_port;
    std::stringstream ss;

    hints = fi_allocinfo ();
    if (!hints)
        return -1; //ENOMEM

    hints->ep_attr->type          = FI_EP_MSG;
//    hints->ep_attr->protocol      = FI_PROTO_RDMA_CM_IB_RC;
//    hints->caps                   = FI_MSG | FI_RMA | FI_ATOMIC | FI_READ | FI_WRITE | FI_RECV | FI_SEND | FI_REMOTE_READ | FI_REMOTE_WRITE;
    hints->caps                   = FI_MSG | FI_RMA | FI_ATOMIC;
    hints->mode                   = FI_LOCAL_MR | FI_CONTEXT;
//    hints->addr_format            = FI_SOCKADDR_IN;
    hints->fabric_attr->prov_name = strdup(provider_name);
    hints->domain_attr->mr_mode   = FI_MR_BASIC;

    faodel::nodeid_t nid = webhook::Server::GetNodeID();;
    string s_host, s_port;
    uint32_t b_host;
    uint16_t b_port;

    nid.GetIPPort(&s_host, &s_port);
    nid.GetIPPort(&b_host, &b_port);
    //cout << "webhook " << s_host<<" / "<<s_port<<std::endl;
    my_fab_port = b_port + FAB_PORT_AUX;
    std::string s_port_aux = std::to_string(my_fab_port);

    rc = fi_getinfo (FI_VERSION(1,0), s_host.c_str() , s_port_aux.c_str(), FI_SOURCE, hints, &cur_fi);
//    rc = fi_getinfo (FI_VERSION(1,0), NULL, NULL, FI_SOURCE, hints, &cur_fi);
    if (rc){
        cerr << "fab_init_ib:fi_getinfo failed" << endl;
        FAB_PRINTERR("fi_getinfo", rc);
        abort();
        return rc;
    }
//    cout << "successful getinfo for init IB " << std::endl;

//    print_long_info(cur_fi);
    fi = select_fi_info(cur_fi, hints);
    rc = fi_fabric (fi->fabric_attr, &fabric, NULL);
    error_check(rc,"fi_fabric");
    eq_attr.wait_obj = FI_WAIT_UNSPEC;
    rc = fi_eq_open (fabric, &eq_attr, &eq, NULL);
    error_check(rc,"fi_eq_open");
    rc = fi_passive_ep (fabric, fi, &pep, NULL);
    error_check(rc,"fi_passive_ep");
    rc = fi_pep_bind (pep, &eq->fid, 0);
    error_check(rc,"fi_pep_bind");
    rc = fi_listen (pep);
    error_check(rc,"fi_listen`");
    rc = fi_domain (fabric, fi, &domain, NULL);
    error_check(rc,"fi_domain");
    memset (&cq_attr, 0, sizeof cq_attr);
    cq_attr.format = FI_CQ_FORMAT_DATA;
    cq_attr.wait_obj = FI_WAIT_NONE;
    cq_attr.size = rx_depth_default + 1;

    rc = fi_cq_open (domain, &cq_attr, &cq, NULL);
    error_check(rc,"fi_cq_open");
    // Register for webhooks to lookup my IB Ip address and port, where fabric connection mgmt running
    webhook::Server::registerHook("/fab/iblookup", [this] (const map<string,string> &args, stringstream &results) {
        struct fab_connection *conn=create_ib_pending_connection(args);
        std::string my_port = to_string(my_fab_port);
        string s_host, s_port;
        mynodeid.GetIPPort(&s_host, &s_port);
        std::string s_port_aux = std::to_string(my_fab_port);
        //
        //
        //
        //
        //
        // TODO THK: s_host is the nodeid host.  should send back the fabric IP.
        //
        //
        //
        //
        //
        results<< s_host<<"/"<<s_port_aux<<"\n";
    });
    //cout << "fab_ib_init complete " << std::endl;
    initialized = true;

    return rc;  //TODO: Is this right?
}


// This will be called either from Connect (client) or from server during initial exchnage
// Sever initial xchng will have no EP but src_addr from remote
// client will not send src_addr but ep from Connect call
//
void
fab_transport::create_connections (
    faodel::nodeid_t nodeid,
    fid_ep * ep)
{
    struct fab_connection* conn= new fab_connection;

    if (ep !=NULL){
        conn->ep = ep;
        conn->remote_nodeid = nodeid;
        std::lock_guard < std::mutex > lock (conn_mutex);
        if (pending_connections.find (nodeid) == pending_connections.end ())
        {
            pending_connections[nodeid] = conn;      // add to connection map
        }
    }
}

void
fab_transport::create_peer_connection(
    struct fi_eq_cm_entry entry)
{

    int ret;
    uint64_t offset = 0;
    struct fab_peer* peer=new fab_peer();
    struct fab_recvbuf* rcvbuf=new fab_recvbuf();

    fab_connection *conn = NULL;
    std::map<faodel::nodeid_t, fab_connection*>::iterator it = pending_connections.begin();

    std::lock_guard < std::mutex > lock (conn_mutex);
    while (it != pending_connections.end())
    {
        conn = it->second;
        if(conn!=NULL){
            if (&conn->ep->fid == entry.fid) // Now pending conn completed
            {
                rcvbuf->buf = malloc(FAB_MTU_SIZE * FAB_NRECV); // post 1000 recv 4k each for each connection
                fi_mr_reg(domain,rcvbuf->buf, FAB_MTU_SIZE * FAB_NRECV, FI_RECV, 0, 0, 0, &rcvbuf->mr, NULL);
                peer->ep_addr = conn->ep;
                peer->remote_nodeid = conn->remote_nodeid;
                for (int i=0; i< 1000; ++i){
                    struct fab_recvreq* rreq = new fab_recvreq();
                    offset = (FAB_MTU_SIZE * i);
                    rreq->repost_buf = (uint64_t) ((char*)rcvbuf->buf + offset);
                    rreq->peer = peer;
                    rreq->mr= rcvbuf->mr;
//                    print_fab_recvreq(rreq);
                    ret = fi_recv(conn->ep, (char*)rcvbuf->buf + offset, FAB_MTU_SIZE , fi_mr_desc(rcvbuf->mr),
                                  0, rreq);
                }
                peer_map[conn->remote_nodeid] = peer;
            }
            it++;
        }
    }
}

void
fab_transport::find_and_update_connection(
    fid_ep* ep)
{

    char*  src_addr;
    uint16_t src_port;
    fab_connection *conn = NULL;
    void *remoteaddr;
    size_t addrlen;
    uint16_t tmp_port;
    string tmp;
    stringstream tmp1;

    if (ep !=NULL){
        //cout << "Inside print addr " <<std::endl;
        remoteaddr = malloc(fi->src_addrlen);
        fi_getpeer (ep, remoteaddr, &addrlen);
        assert(addrlen!=0);

        if(remoteaddr !=NULL){
            src_addr =
                (char*) inet_ntoa(((struct sockaddr_in *) remoteaddr)->sin_addr);
            src_port =
                (uint16_t) ntohs (((struct sockaddr_in *) remoteaddr)->sin_port);
        }
        tmp=string(src_addr);
        tmp1 << src_port;
        //cout << "find and update connection ip " << src_addr << "port " << src_port << std::endl;
    } else {
        cout << "ep is NULL to find connection" << std::endl;
        abort();
    }

    std::lock_guard < std::mutex > lock (conn_mutex);
    std::map<faodel::nodeid_t, fab_connection*>::iterator it = pending_connections.begin();
    while (it != pending_connections.end())
    {
        // shyamali debug, currently port shows bogus so one connection per node, fix it!!.
        conn = it->second;
        if(conn!=NULL){
            //cout << "walking connection list ip " << conn->src_ip << "port " << conn->sport << std::endl;
            if ((conn->src_ip == tmp )){
                conn->src_addr = remoteaddr;
                conn->dst_addr = fi->src_addr;;
                conn->ep = ep;
                //cout << "updated a connection" << std::endl;
                break;
            }
        }
        it++;
    }
}


struct fab_connection*
fab_transport::create_ib_pending_connection (
    const map<string,string> &args)
{
    string hostname, port, rem_name, rem_port;
    char* addr;
    uint16_t lport;
    struct fab_connection* conn = new fab_connection;
    auto new_val = args.find("rem_webhook_hostname");
    if(new_val != args.end()){
        hostname = new_val->second;
    }
    new_val = args.find("rem_webhook_port");
    if(new_val != args.end()){
        port = new_val->second;
    }
    new_val = args.find("rem_peer_name");
    if(new_val != args.end()){
        rem_name = new_val->second;
    }
    new_val = args.find("rem_peer_port");
    if(new_val != args.end()){
        rem_port = new_val->second;
    }
    faodel::nodeid_t remote_nodeid(hostname, port);

//    cout << "inbound connection from " << remote_nodeid.GetHex() << endl;

    conn->src_ip = rem_name;
    conn->sport = rem_port;
    //cout << "creating server side connection for  :" << conn->src_ip << "conn port  " << conn->sport << std::endl;
    conn->remote_nodeid = remote_nodeid;
    std::lock_guard < std::mutex > lock (conn_mutex);
    if (pending_connections.find(remote_nodeid) == pending_connections.end ())
    {
        pending_connections[remote_nodeid] = conn;      // add to connection map
    }
    //cout << "All done creating pending connection" << "\n";
    return conn;
}

void
fab_transport::ib_server_conn ()
{
    struct fi_eq_cm_entry entry;
    uint32_t event;
    int rc = 0;
    int rd;
    struct fid_ep *conn_ep = NULL;
    int routs;
    struct fab_connection *conn;
    struct fab_peer* peer=new fab_peer();
    char errstr[1024];

    while(!shutdown_requested){
        rd = fi_eq_sread (eq, &event, &entry, sizeof entry, 500, 0);

        if (rd < 0) {
            eq_readerr(eq, errstr);
            fprintf(stderr, "ib_server_conn() - rd=%d, event=%d, errstr=%s, errno=%d (%s)\n", rd, event, errstr, errno, strerror(errno));
            continue;
        }

        switch(event){
            case FI_CONNREQ: //Server receives this event
//                cout << "Got a connect request event" << std::endl;
                rc = fi_endpoint (domain, entry.info, &conn_ep, NULL);
                error_check(rc, "fi_endpoint");
                rc = fi_ep_bind (conn_ep, &eq->fid, 0);
                error_check(rc, "fi_ep_bind");
                rc = fi_ep_bind (conn_ep, &cq->fid, FI_SEND|FI_RECV);
                if (rc) {
                    FAB_PRINTERR("fi_ep_bind", rc);
                    exit(-1);
                }
                rc = fi_enable (conn_ep);
                error_check(rc, "fi_enable");
                rc = fi_accept (conn_ep, NULL, 0);
                error_check(rc, "fi_accept");
//                print_addr(conn_ep);
                find_and_update_connection(conn_ep);
                break;

            case FI_CONNECTED:
//                cout << "Got a connected event" << std::endl;
                create_peer_connection(entry);
                break;
            default:
                break;
        }
        fi_freeinfo (entry.info);
    }
}


fab_peer*
fab_transport::client_connect_ib (
    faodel::nodeid_t   peer_nodeid)
{
    int ret;
    struct fi_info *my_fi;
    stringstream ss_path;
    stringstream ss_port;
    struct fab_connection *conn;
    string tmp;
    struct fab_peer* p;

    string result;
    void *ep_name;
    char *src_addr;
    uint16_t src_port;
    char* my_port;
    char* my_src;

    fab_peer *existing_peer = find_peer(peer_nodeid);
    if (existing_peer != NULL) {
//        cout << "found existing connection to " << peer_nodeid.GetHex() << endl;
        return existing_peer;
    }

//    cout << "outbound connection to " << peer_nodeid.GetHex() << endl;

    src_addr =
        (char*) inet_ntoa(((struct sockaddr_in *) fi->src_addr)->sin_addr);
    src_port =
        (uint16_t) ntohs (((struct sockaddr_in *) fi->src_addr)->sin_port);
//    cout << "client_ib_connect() - My addr:" << src_addr << " conn port  " << src_port << " addrlen " << fi->src_addrlen <<std::endl;

    ss_path << "/fab/iblookup&rem_webhook_hostname=" << mynodeid.GetIP()<<"&rem_webhook_port="<<mynodeid.GetPort()<<"&rem_peer_name="<<src_addr<<"&rem_peer_port="<<src_port;
//    cout << "client_ib_connect() - ss_path='" << ss_path.str() << "'" << endl;

    int retries = 5;
    ret = webhook::retrieveData(peer_nodeid.GetIP() , peer_nodeid.GetPort() , ss_path.str(), &result);
    while (ret != 0 && --retries) {
        sleep(1);
        ret = webhook::retrieveData(peer_nodeid.GetIP() , peer_nodeid.GetPort() , ss_path.str(), &result);
    }
    if (ret != 0) {
        std::cout << "client_connect_ib() - webhook::retrieveData() timed out" << std::endl;
        return(NULL);
    }

    boost::trim_right(result);
    vector<string> result_parts = faodel::SplitPath(result);
//    cout <<"Result is '" << result_parts[0] << "'  '" << result_parts[1] << "'\n";

    if (!hints)
        return NULL;
    /*
hints->ep_attr->type = FI_EP_MSG;
hints->caps = FI_RMA;
hints->mode = FI_LOCAL_MR;
hints->addr_format = FI_SOCKADDR_IN;
hints->fabric_attr->prov_name = strdup("verbs");
     */
    ret = fi_getinfo (FI_VERSION(1,0), result_parts[0].c_str(), result_parts[1].c_str(), 0, hints, &my_fi);
    if (ret){
        cerr << "ERROR: client_connect: fi_getinfo" << endl;
        return NULL;
    }
    ret = fi_endpoint (domain, my_fi, &ep, NULL);
    if (ret){
        cerr << "ERROR: fi_endpoint" << endl;
        return NULL;
    }
    ret = fi_ep_bind (ep, &cq->fid, FI_SEND|FI_RECV);
    if (ret) {
        FAB_PRINTERR("fi_ep_bind", ret);
        exit(-1);
    }
    ret = fi_ep_bind (ep, &eq->fid, 0);
    if (ret){
        cerr << "ERROR: fi_ep_bind event Q" << endl;
        return NULL;
    }
    ret = fi_enable (ep);
    if (ret){
        cerr << "ERROR: fi_enable" << endl;
        return NULL;
    }
    //print_addr(ep);
    create_connections(peer_nodeid, ep);
    ret = fi_connect (ep, my_fi->dest_addr, NULL, 0);
    if (ret){
        cerr << "CLIENT CONNECT ERROR :fi_connect" << endl;
        return  NULL;
    }
    while(1){
        p =find_peer(peer_nodeid);
        if (p == NULL ){
            usleep(100);
        }else{
//            cout << "new peer is " << p << " endpoint " << ep << std::endl;
            p->ep_addr=ep;
            return p;
        }
    }
    return NULL;  /// Search here for peer map to have a valid peer struct to return
}

void
fab_transport::start_ib_connection_thread(void)
{
    conn_thread_ = std::thread(&fab_transport::ib_server_conn, this);
}

void
fab_transport::stop_connection_thread(void)
{
    conn_thread_.join();
}

/* End of Infiniband related methods */


void
fab_transport::Send(
    fab_peer *remote_peer,
    fab_buf *msg,
    DataObject ldo,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret;
    struct fi_cq_data_entry wc = { 0 };
    struct fi_cq_err_entry error = { 0 };

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = 0;
    context->user_cb     = user_cb;

//    message_t *msgt = (message_t*)((char*) ldo.GetDataPtr());
//    cout << "outgoing message to " << remote_peer->remote_nodeid.GetHex() << endl;
//    cout << msgt->str() << endl;

//    cout << "Trying to send to  Send buffer "<< ldo.GetDataPtr() <<  " length " << ldo.GetDataSize() << std::endl;
//    cout << "Send  peer "<< remote_peer <<  " endpoint " << remote_peer->ep_addr << std::endl;
    //CDU: Note- we send starting with the data ptr
    rc = fi_send(remote_peer->ep_addr,
                 ldo.GetDataPtr(),
                 ldo.GetDataSize(),
                 fi_mr_desc(msg->buf_mr),
                 remote_peer->remote_addr,
                 context); //CDU
    if (rc) {
        cerr << "fi_send error " << rc << endl;
    }
}

void
fab_transport::Get(
    fab_peer *remote_peer,
    fab_buf *msg,
    DataObject ldo,
    struct fi_rma_iov remote,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;
    struct fi_cq_data_entry wc = { 0 };
    struct fi_cq_err_entry error = { 0 };
    uint64_t  cq_data;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = 0;
    context->user_cb     = user_cb;

    //cout << " Get  peer  "<< remote_peer <<  " endpoint " << remote_peer->ep_addr << std::endl;
    rc = fi_read(remote_peer->ep_addr,
                 ldo.internal_use_only.GetHeaderPtr(),
                 ldo.GetWireSize(),
                 fi_mr_desc(msg->buf_mr),
                 remote_peer->remote_addr,
                 remote.addr,
                 remote.key,
                 context);
    if (rc) {
        cerr << "fi_write error " << rc << endl;
    }
}

void
fab_transport::Get(
    fab_peer *remote_peer,
    fab_buf *msg,
    DataObject ldo,
    uint64_t local_offset,
    struct fi_rma_iov remote,
    uint64_t len,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;
    struct fi_cq_data_entry wc = { 0 };
    struct fi_cq_err_entry error = { 0 };
    uint64_t  cq_data;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = local_offset;
    context->user_cb     = user_cb;

    //cout << "Get  remote addr with offset : "<< remote.addr <<  " key " << remote.key << std::endl;
    rc = fi_read(remote_peer->ep_addr,
                 (char *)ldo.internal_use_only.GetHeaderPtr() + local_offset,
                 len,
                 fi_mr_desc(msg->buf_mr),
                 remote_peer->remote_addr,
                 remote.addr,
                 remote.key,
                 context); //CDU
    if (rc) {
        cerr << "fi_write error " << rc << endl;
    }
}

/*
FI_RMA] : Indicates that an RMA operation completed.
This flag may be combined with an FI_READ, FI_WRITE, FI_REMOTE_READ, or
FI_REMOTE_WRITE flag.

 */
void
fab_transport::Put(
    fab_peer *remote_peer,
    fab_buf *msg,
    DataObject ldo,
    struct fi_rma_iov remote,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;
    struct fi_cq_data_entry wc = { 0 };
    struct fi_cq_err_entry error = { 0 };
    uint64_t  cq_data;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = 0;
    context->user_cb     = user_cb;

    //cout << "Put  remote addr : "<< remote.addr <<  " key " << remote.key << std::endl;
    //cout << " Put  peer  "<< remote_peer <<  " endpoint " << remote_peer->ep_addr << std::endl;
    rc = fi_write (remote_peer->ep_addr,
                   ldo.internal_use_only.GetHeaderPtr(),
                   ldo.GetWireSize(),
                   fi_mr_desc(msg->buf_mr),
                   remote_peer->remote_addr,
                   remote.addr,
                   remote.key,
                   context); //cdu
    if (rc) {
        cerr << "fi_write error " << rc << endl;
    }
}

void
fab_transport::Put(
    fab_peer *remote_peer,
    fab_buf *msg,
    DataObject ldo,
    uint64_t local_offset,
    struct fi_rma_iov remote,
    uint64_t len,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;
    struct fi_cq_data_entry wc = { 0 };
    struct fi_cq_err_entry error = { 0 };
    uint64_t  cq_data;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = local_offset;
    context->user_cb     = user_cb;

    //cout << "Put  remote addr with offset : "<< remote.addr <<  " key " << remote.key << std::endl;
    rc = fi_write(remote_peer->ep_addr,
                  (char *)ldo.internal_use_only.GetHeaderPtr() + local_offset,
                  len,
                  fi_mr_desc(msg->buf_mr),
                  remote_peer->remote_addr,
                  remote.addr,
                  remote.key,
                  context); //cdu
    if (rc) {
        cerr << "fi_write error " << rc << endl;
    }
}

void
fab_transport::Atomic(
    fab_peer *remote_peer,
    AtomicOp op,
    fab_buf *msg,
    DataObject ldo,
    uint64_t loffset,
    struct fi_rma_iov remote,
    uint64_t length,
    int64_t operand,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;

    *operand1_ptr = operand;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = loffset;
    context->user_cb     = user_cb;

//    fprintf(stdout, "fab::Atomic(fadd) - operand=%ld  &operand=%p  *operand_ptr=%ld  operand_ptr=%p\n", operand, &operand, *operand1_ptr, operand1_ptr);
//    fprintf(stdout, "fab::Atomic(fadd) - ldo value=%X\n", *(uint64_t*)((char *)ldo.GetDataPtr() + loffset));
//    fprintf(stdout, "fab::Atomic(fadd) - ldo.data_ptr=%p, loffset=%lX\n", ldo.GetDataPtr(), loffset);

    rc = fi_fetch_atomic(
        remote_peer->ep_addr,

        (void*)operand1_ptr,
        1,
        fi_mr_desc(operand1_mr),

        (char *)ldo.GetDataPtr() + loffset,
        fi_mr_desc(msg->buf_mr),

        remote_peer->remote_addr,
        remote.addr,
        remote.key,

        FI_INT64,
        FI_SUM,
        context);
    if (rc) {
        cerr << "fi_fetch_atomic error " << rc << endl;
    }
}

void
fab_transport::Atomic(
    fab_peer *remote_peer,
    AtomicOp op,
    fab_buf *msg,
    DataObject ldo,
    uint64_t loffset,
    struct fi_rma_iov remote,
    uint64_t length,
    int64_t operand1,
    int64_t operand2,
    std::function< WaitingType(OpArgs *args) > user_cb)
{
    int rc, ret, retry;

    *operand1_ptr = operand1;
    *operand2_ptr = operand2;

    struct fab_op_context *context = new struct fab_op_context;
    context->remote_peer = remote_peer;
    context->msg         = msg;
    context->ldo         = ldo;
    context->loffset     = loffset;
    context->user_cb     = user_cb;

//    fprintf(stdout, "fab::Atomic(cswap) - operand1=%ld  operand2=%ld  *operand1_ptr=%ld  operand1_ptr=%lu  *operand2_ptr=%ld  operand2_ptr=%lu\n",
//            operand1, operand2, *operand1_ptr, operand1_ptr, *operand2_ptr, operand2_ptr);
//    fprintf(stdout, "fab::Atomic(cswap) - ldo value=%X\n", *(uint64_t*)((char *)ldo.GetDataPtr() + loffset));

    rc = fi_compare_atomic(
        remote_peer->ep_addr,

        (void*)operand2_ptr,
        1,
        fi_mr_desc(operand2_mr),

        (void*)operand1_ptr,
        fi_mr_desc(operand1_mr),

        (char *)ldo.GetDataPtr() + loffset,
        fi_mr_desc(msg->buf_mr),

        remote_peer->remote_addr,
        remote.addr,
        remote.key,

        FI_INT64,
        FI_CSWAP,
        context);
    if (rc) {
        cerr << "fi_compare_atomic error " << rc << endl;
    }
}


} // end of namespace
} // end of namespace
