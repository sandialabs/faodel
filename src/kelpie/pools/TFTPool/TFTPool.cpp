// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
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
#include "kelpie/pools/TFTPool/TFTPool.hh"



using namespace std;
using namespace faodel;

namespace kelpie {


TFTPool::TFTPool(const ResourceURL &pool_url)
  : DHTPool(pool_url) {
}

/**
 * @brief Determine the index of the TFT list that owns the data
 * @param key The Key label for the blob (must contain a tag encoded via SetK1Tag()!)
 * @return The index of the node in the table that does the work (tag % pool_nodes.size())
 */
uint32_t TFTPool::findNodeIndex(const Key &key){

  uint32_t tag;
  rc_t rc = key.GetK1Tag(&tag);

  if(rc!=KELPIE_OK) {
     warn("No tag detected by TFTPool for Key "+key.str());
     if(nodes.size()<2) return 0;  //Skip if no hash needed
     tag = faodel::hash_dbj2(default_bucket, key.K1() );
  }

  return (tag % nodes.size());
}

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void TFTPool::sstr(stringstream &ss, int depth, int indent) const {

  ss << string(indent,' ') + "TFTPool "<< endl;
  dir_info.sstr(ss, depth-1, indent+2);
  lkv->sstr(ss, depth-1,indent+1);
  //TODO: internals
}

/**
 * @brief Pool constructor function for creating a new DHTPool via a URL
 * @param pool_url The URL for the pool
 * @return New DHTPool
 */
shared_ptr<PoolBase> TFTPoolCreate(const ResourceURL &pool_url) {
  return make_shared<TFTPool>(pool_url);
}

}  // namespace kelpie
