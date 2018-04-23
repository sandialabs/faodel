// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_UNCONFIGUREDPOOL_HH
#define KELPIE_UNCONFIGUREDPOOL_HH

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

#include "common/Common.hh"


#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"
#include "kelpie/pools/PoolBase.hh"

#include "lunasa/DataObject.hh"

namespace kelpie {

/**
 * @brief A dummy implementation of a pool that panics on any operation
 *
 * This class is provided as a debugging resource to protect against times
 * when a user requests a pool that cannot be located. The intent is for
 * this pool to trigger a panic operation on any call, in order to help
 * identify that a bad pool request was made.
 */
class UnconfiguredPool : public PoolBase {

public:

  UnconfiguredPool();
  ~UnconfiguredPool();

  //PoolBase functions
  rc_t Publish(const Key &key, fn_publish_callback_t callback);
  rc_t Publish(const Key &key, const lunasa::DataObject &user_ldo, fn_publish_callback_t callback);

  rc_t Want(const Key &key, size_t expected_ldo_user_bytes, fn_want_callback_t callback);

  rc_t Need(const Key &key, size_t expected_ldo_user_bytes, lunasa::DataObject *returned_ldo);  //Block until get
  rc_t Info(const Key &key, kv_col_info_t *col_info);
  rc_t RowInfo(const Key &key, kv_row_info_t *row_info);
  rc_t Drop(const Key &key);

  int FindTargetNode(const Key &key, faodel::nodeid_t *node_id=nullptr, net::peer_ptr_t *peer_ptr=nullptr);
  std::string TypeName() const { return "unconfigured"; }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  void Panic(std::string caller) const;

};  //UnconfiguredPool


//For use by connect
std::shared_ptr<PoolBase> UnconfiguredPoolCreate(const faodel::ResourceURL &pool_url);


}  // namespace kelpie

#endif  // KELPIE_UNCONFIGUREDPOOL_HH

