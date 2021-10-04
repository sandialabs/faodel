// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>

#include <mpi.h>

#include "faodel-common/Common.hh"
#include "faodel-common/Debug.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/pools/RFTPool/RFTPool.hh"



using namespace std;
using namespace faodel;

namespace kelpie {


RFTPool::RFTPool(const ResourceURL &pool_url)
  : DHTPool(pool_url) {

  //TODO: the DHTPool ctor makes connections to ALL resources. We only need the one

  string intended_rank = pool_url.GetOption("rank");
  if(intended_rank.empty()) {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  } else {
    mpi_rank = std::stoi(intended_rank, nullptr, 0);
  }

}

/**
 * @brief Determine the index of the RFT list that owns the data
 * @param key The Key label for the blob (Ignored!)
 * @return The index of the node in the table that does the work (mpi_rank % pool_nodes.size())
 */
uint32_t RFTPool::findNodeIndex(const Key &key){

  return (mpi_rank % nodes.size());
}

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void RFTPool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "RFTPool "<< endl;
  dir_info.sstr(ss, depth-1, indent+2);
  lkv->sstr(ss, depth-1,indent+1);
  //TODO: internals
}

/**
 * @brief Pool constructor function for creating a new DHTPool via a URL
 * @param pool_url The URL for the pool
 * @return New DHTPool
 */
shared_ptr<PoolBase> RFTPoolCreate(const ResourceURL &pool_url) {
  return make_shared<RFTPool>(pool_url);
}

}  // namespace kelpie
