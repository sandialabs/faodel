// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef SHARED_HH
#define SHARED_HH

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
#include <thread>
#include <mutex>

#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

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
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#define FAB_MTU_SIZE 4096
#define FAB_NRECV 1000
#define FAB_MR_KEY 0xC0DE
#define FAB_PORT_AUX 3015


using namespace std;
using namespace lunasa;

namespace opbox{
namespace net{

/**
 * @brief A small, network-specific handle for communicating with a peer
 * @note doxygen documentation is likely wrong, due to build system
 */
typedef struct peer_t {
    struct fab_peer  *p;
    peer_t(struct fab_peer *fp) : p(fp) {}
} peer_t;

struct fab_peer {
    struct fid_ep *ep_addr;
    char  *dst_addr;
    char  *dst_port;
    fi_addr_t  remote_addr;
    faodel::nodeid_t  remote_nodeid;
    uint32_t  rem_addrlen;
};

struct fab_buf {
    uint64_t  buf;
    uint64_t offset;
    uint64_t len;
    uint64_t key;
    struct fid_mr *buf_mr;
};

struct fab_op_context {
    fab_peer   *remote_peer;
    fab_buf    *msg;
    DataObject  ldo;
    uint64_t    loffset;
    std::function< WaitingType(OpArgs *args) > user_cb;
};

struct fab_recvbuf {
    void *buf;
    struct fid_mr *mr;
    uint64_t len;
    int recv_cnt;
    faodel::nodeid_t rem_nodeid; // When we disconnect we will remove this from recv pool
};

struct fab_recvreq{
    uint64_t repost_buf;
    struct fid_mr *mr;
    uint64_t len;
    uint64_t offset;
    struct fab_peer* peer;
};

/**
 *  *  *  * This custom key is used to look up existing connections.
 *   *   *   */
struct addrport_key
{
    uint32_t addr;
    uint16_t port;
    bool operator< (const addrport_key & key1) const
    {
        if (addr < key1.addr){
            return true;
        }
        else if (addr == key1.addr){
            if (port < key1.port)
            {
                return true;
            }
        }
        return false;
    }
    bool operator> (const addrport_key & key1) const
    {
        if (addr > key1.addr)
        {
            return true;
        }
        else if (addr == key1.addr)
        {
            if (port > key1.port)
            {
                return true;
            }
        }
        return false;
    }
};

struct fab_connection {
    fi_addr_t remote_fi_addr;
    char rem_name[64];
    string   src_ip;
    string   sport;
    void*    src_addr;
    void*    dst_addr;
    struct 	 fid_ep *ep;
    int 	addrlen;
    faodel::nodeid_t remote_nodeid;
    bool connection_ready;  // type ==1 server, type =2 client for IB bverbs CONNREQ type only
};

#define PP_PRINTERR(call, retv)                                                \
fprintf(stderr, "%s(): %s:%-4d, ret=%d (%s)\n", call, __FILE__,        \
    __LINE__, (int)retv, fi_strerror((int) -retv))




enum {
    PINGPONG_RECV_WCID = 1,
    PINGPONG_SEND_WCID = 2,
};

}
}
#endif
