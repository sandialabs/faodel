// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstdio>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif


#include "faodel-common/Common.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/core/Singleton.hh"
#include "kelpie/pools/TracePool/TracePool.hh"

using namespace std;
using namespace faodel;

namespace kelpie {

TracePool::TracePool(const ResourceURL &pool_url)
        : PoolBase(pool_url, PoolBehavior::DefaultLocal), use_relative_time(true) {

  //Set the time vals
  t_last = t_start = std::chrono::high_resolution_clock::now();

  string dashed_pool_name=pool_url.Dashify();
  string extra_bucket = pool_url.GetOption("bucket");
  if(!extra_bucket.empty())
    dashed_pool_name += "_"+extra_bucket;

  string next_pool_name = pool_url.GetOption("next_pool", "null:");
  string fname = pool_url.GetOption("file", "trace"+dashed_pool_name);
  string rank = pool_url.GetOption("rank");

  #ifdef Faodel_ENABLE_MPI_SUPPORT
  if(rank.empty()) {
    int mpi_rank=0;
    int is_initialized=0;
    MPI_Initialized(&is_initialized);
    if(is_initialized) {
      MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    }
    rank = std::to_string(mpi_rank);
  }
  #endif
  if(!rank.empty()) {
    fname = fname+"."+rank;
    rank_flag = " -r "+rank +" "; //Convert to a proper flag
  }
  f.open(fname, ios::out);
  if(!f.is_open()) {
    throw std::runtime_error("Could not open trace file "+fname);
  }

  //Dump what pool this is
  f << "set pool "<<pool_url.GetPathName()<<endl;

  //Attempt to connect to next pool
  next_pool = kelpie::Connect(next_pool_name);

  //Set debug info
  SetSubcomponentName("-Trace-"+pool_url.bucket.GetHex());

}


rc_t TracePool::Publish(const Key &key, const fn_publish_callback_t &callback) {
  appendTrace("kput", key.str_as_args());
  return next_pool.Publish(key, callback);
}

rc_t TracePool::Publish(const Key &key,
                       const lunasa::DataObject &user_ldo,
                       const fn_publish_callback_t &callback) {

  stringstream ss;
  ss<< "-M "<<user_ldo.GetMetaSize()
    << " -D "<<user_ldo.GetDataSize()
    << " " << key.str_as_args();

  appendTrace("kput", ss.str());
  return next_pool.Publish(key, user_ldo, callback);
}

rc_t TracePool::Want(const Key &key, size_t expected_ldo_user_bytes, const fn_want_callback_t &callback) {

  appendTrace("kget", key.str_as_args());
  return next_pool.Want(key, expected_ldo_user_bytes, callback);
}

rc_t TracePool::Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo){

  appendTrace("kget", key.str_as_args());
  return next_pool.Need(key, expected_ldo_user_bytes, returned_ldo);

}

rc_t TracePool::Compute(const Key &key, const std::string &function_name, const std::string &function_args, const fn_compute_callback_t &callback) {
  stringstream ss;
  ss<<key.str_as_args()
    <<" -F "<<function_name
    <<" -A "<<function_args<<endl;
  appendTrace("kcomp", ss.str());
  return next_pool.Compute(key, function_name, function_args, callback);
}

rc_t TracePool::Info(const Key &key, object_info_t *info) {

  appendTrace("kinfo", key.str_as_args());
  return next_pool.Info(key, info);
}

rc_t TracePool::RowInfo(const Key &key, object_info_t *info) {

  appendTrace("kinfo", key.str_as_args());
  return next_pool.RowInfo(key, info);
}

rc_t TracePool::Drop(const Key &key, fn_drop_callback_t callback){

  appendTrace("kdrop", key.str_as_args());
  return next_pool.Drop(key, callback);
}

rc_t TracePool::List(const kelpie::Key &search_key, ObjectCapacities *object_capacities) {

  appendTrace("klist", search_key.str_as_args());
  return next_pool.List(search_key, object_capacities);
}

int TracePool::FindTargetNode(const Key &key, faodel::nodeid_t *node_id, net::peer_ptr_t *peer_ptr){
  return next_pool.FindTargetNode(key, node_id, peer_ptr);
}


void TracePool::appendTrace(const string &cmd, const std::string &s) {

  using namespace std::chrono;

  f_mutex.lock();
  auto t_stamp = high_resolution_clock::now();
  auto t_us = duration_cast<microseconds>(t_stamp - ((use_relative_time) ? t_last : t_start)).count();

  if(use_relative_time) {
    f << "delayfor " << t_us << "us " << rank_flag << ";";
  } else {
    f << "delayuntil " << t_us << "us " << rank_flag << ";";
  }

  f << cmd << rank_flag << s << endl;
  t_last = t_stamp;
  f_mutex.unlock();
}


void TracePool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "TracePool "
     << " Iom: "<< ((iom==nullptr) ? "None" : iom->Name())
     << endl;
}



/**
 * @brief Pool constructor function for creating a new TracePool via a URL
 * @param pool_url The URL for the pool
 * @return New TracePool
 */
shared_ptr<PoolBase> TracePoolCreate(const ResourceURL &pool_url) {
  return make_shared<TracePool>(pool_url);
}

}  // namespace kelpie
