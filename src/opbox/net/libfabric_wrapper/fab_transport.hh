// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FABTRANSPORT_HH
#define FABTRANSPORT_HH


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
#include <atomic>

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
#include <arpa/inet.h>

#include "opbox/net/libfabric_wrapper/shared.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace lunasa;


namespace opbox
{
namespace net
{

class fab_transport {

private:
  fab_transport();
  bool configured = false;
  bool initialized = false;
  unsigned int my_fab_port;
  static fab_transport *single_fab;
  struct fid_fabric *fabric;
  struct fid_domain *domain;
  fi_addr_t my_fi_addr;
	std::atomic<std::uint64_t>  mrkey;
  struct fid_wait *waitset;
  struct fid_poll *pollset;
  struct fid_pep *pep;
  struct fi_info *hints;
  struct fid_cq *cq;
  struct fid_cntr *txcntr, rxcntr;
  struct fid_av *av;
  struct fid_eq *eq;
  size_t buf_size, tx_size, rx_size;
  int timeout;
  struct fi_context tx_ctx, rx_ctx;
  struct fi_av_attr av_attr;
  struct fi_eq_attr  eq_attr;
  struct fi_cq_attr cq_attr;
  struct fi_cntr_attr cntr_attr;
  std::deque<struct fab_buf> recv_buffers_;
  std::map <faodel::nodeid_t,fab_connection*> pending_connections;
  std::map <faodel::nodeid_t,fab_peer*> peer_map;
  std::mutex conn_mutex;
  std::mutex compq_mutex;

  int64_t *operand1_ptr;
  int64_t *operand2_ptr;
  struct fid_mr *operand1_mr;
  struct fid_mr *operand2_mr;


public:
  struct fi_info *fi;
  int    my_transport_id;
  faodel::nodeid_t    mynodeid;
  struct fid_ep *ep;

  std::thread  progress_thread_;
  std::thread  conn_thread_;

  bool shutdown_requested = false;

  std::function<void(opbox::net::peer_ptr_t, opbox::message_t*)> recv_cb_;

//Common Methods
  fab_transport* get_instance();
  fab_transport(faodel::Configuration &config);
  void start(void);
  void stop(void);
  void start_progress_thread(void);
  void stop_progress_thread(void);
  void start_ib_connection_thread(void);
  void stop_connection_thread(void);
  int disconnect(struct fab_peer *peer);
  int check_completion();
  int fab_post_recv (struct fid_ep *ep, void *buf, struct fid_mr *mr, int size, int n);
  int init_endpoint(fid_ep *ep);
  void eq_readerr (struct fid_eq *eq, const char *eq_str);
  fab_peer* find_peer (faodel::nodeid_t nodeid);

// IB related functions  EP_MSG
//
  int fab_init_ib(const char *provider_name);
  void create_connections (faodel::nodeid_t nodeid, fid_ep * ep);
  struct fab_connection *create_ib_pending_connection(const std::map<std::string,std::string> &args);
  fab_peer* client_connect_ib (faodel::nodeid_t   peer_nodeid);
  void  find_and_update_connection(fid_ep* ep);
  void ib_server_conn();
  void create_peer_connection(struct fi_eq_cm_entry entry);

// EP_RDM calls
//
  fab_peer* create_rdm_connection_client (faodel::nodeid_t   peer_nodeid);
  void create_rdm_connection_server(const std::map<std::string,std::string> &args, std::stringstream &results);
  int fab_init_rdm(const char *provider_name);
  void destroy_rdm_connection_server(const std::map<std::string,std::string> &args, std::stringstream &results);

// net layer calls
  void Send(
      fab_peer *remote_peer,
      fab_buf *msg,
      DataObject ldo,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Get(
      fab_peer *remote_peer,
      fab_buf *msg,
      DataObject ldo,
      struct fi_rma_iov remote,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Get(
      fab_peer *remote_peer,
      fab_buf *msg,
      DataObject ldo,
      uint64_t loffset,
      struct fi_rma_iov remote,
      uint64_t length,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Put(
      fab_peer *remote_peer,
      fab_buf *msg,
      DataObject ldo,
      struct fi_rma_iov remote,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Put(
      fab_peer *remote_peer,
      fab_buf *msg,
      DataObject ldo,
      uint64_t loffset,
      struct fi_rma_iov remote,
      uint64_t length,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Atomic(
      fab_peer *remote_peer,
      AtomicOp op,
      fab_buf *msg,
      DataObject ldo,
      uint64_t loffset,
      struct fi_rma_iov remote,
      uint64_t length,
      int64_t operand,
      std::function< WaitingType(OpArgs *args) > user_cb);
  void Atomic(
      fab_peer *remote_peer,
      AtomicOp op,
      fab_buf *msg,
      DataObject ldo,
      uint64_t loffset,
      struct fi_rma_iov remote,
      uint64_t length,
      int64_t operand1,
      int64_t operand2,
      std::function< WaitingType(OpArgs *args) > user_cb);

  void register_memory (void *base_addr, int length, struct fab_buf *send_buf);
  void unregister_memory (struct fab_buf *send_buf);

//
  void setupRecvQueue();
  void teardownRecvQueue();
  void print_addr(fid_ep *ep);
//

};


}
}
#endif
