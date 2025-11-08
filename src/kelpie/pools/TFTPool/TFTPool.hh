// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_TFTPOOL_HH
#define KELPIE_TFTPOOL_HH

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "kelpie/pools/DHTPool/DHTPool.hh"

namespace kelpie {

/**
 * @brief Handle to a Tag Folding Table (TFT) Pool
 *
 * A Tag Folding Table (TFT) is a pool that uses a numerical tag encoded
 * into the end of the row portion of a key to determine which of the
 * pool nodes is responsible for hosting the data. The intent of this pool
 * is to provide an easy way for a user to group related items together.
 *
 * eg, a user generating an exact number of items that are assigned in order
 * to a pool of N nodes would (1) create the key "foo" for each item, (2) use
 * the key.SetK1Tag(id) option to modify the key to modify the key
 * to be "foo{0x1}", and (3) publish to a TFT. The TFT extracts the tag (0x1)
 * as an integer and does modulo NUM_POOL_NODES to figure out where it goes.
 */
class TFTPool : public DHTPool {

public:

  explicit TFTPool(const faodel::ResourceURL &pool_url);
  ~TFTPool() override = default;

  std::string TypeName() const override { return "TFT"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;

protected:
  uint32_t findNodeIndex(const Key &key) override;


};  //TFTPool


//For use by connect
std::shared_ptr<PoolBase> TFTPoolCreate(const faodel::ResourceURL &pool_url);


}  // namespace kelpie

#endif  // KELPIE_TFTPOOL_HH

