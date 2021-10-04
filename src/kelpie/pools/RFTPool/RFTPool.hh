// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_RFTPOOL_HH
#define KELPIE_RFTPOOL_HH

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "kelpie/pools/DHTPool/DHTPool.hh"

namespace kelpie {

/**
 * @brief Handle to a Rank Folding Table (RFT) Pool
 *
 * A Rank Folding Table (RFT) is a pool that uses the rank id of the
 * client to determine which of the pool nodes is responsible for hosting
 * the data. The intent with this pool is to provide an easy way for 
 * an mpi application to route data through caching nodes in siturations
 * where the mpi job is doing concurrent bulk I/O (ie, a regular, all-write
 * pattern).
 *
 * If there are M MPI ranks and N pool nodes, pool id is (N % rank_id).
 */
class RFTPool : public DHTPool {

public:

  explicit RFTPool(const faodel::ResourceURL &pool_url);
  ~RFTPool() override = default;

  std::string TypeName() const override { return "rft"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

protected:
  uint32_t findNodeIndex(const Key &key) override;

private:
  int mpi_rank;

};  //RFTPool


//For use by connect
std::shared_ptr<PoolBase> RFTPoolCreate(const faodel::ResourceURL &pool_url);


}  // namespace kelpie

#endif  // KELPIE_RFTPOOL_HH

