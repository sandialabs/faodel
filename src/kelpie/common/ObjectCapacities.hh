// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_OBJECTCAPACITIES_HH
#define FAODEL_OBJECTCAPACITIES_HH

#include <vector>

#include "faodel-common/InfoInterface.hh"

namespace kelpie {

class Key; //Forward reference

class ObjectCapacities :
        public faodel::InfoInterface {
public:

  //Note: These data structures are plain vectors because some callers need to update capacities first, then set the keys
  std::vector<kelpie::Key> keys;
  std::vector<size_t> capacities;

  /// Append a ket/capacity to this ObjectCapacities. Does NOT dedupe
  void Append(const kelpie::Key &key, size_t capacity);
  void Append(const ObjectCapacities &other);

  void Merge(const ObjectCapacities &other);

  int FindIndex(const kelpie::Key &key);
  void Wipe();

  /// How many entries are here
  size_t Size() const {  return keys.size();  }

  //Serialization hook
  template<typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & keys;
    ar & capacities;
  }

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth, int indent) const override;
};

}  // namespace kelpie

#endif //FAODEL_OBJECTCAPACITIES_HH
