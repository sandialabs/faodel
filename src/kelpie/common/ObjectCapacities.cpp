// Copyright 2021 National Technology & Engineering Solutions of Sandia,
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.

#include "kelpie/common/Types.hh"

using namespace std;

namespace kelpie {

/// Append a key/capacity to this ObjectCapacities. Does NOT dedupe
void ObjectCapacities::Append(const kelpie::Key &key, size_t capacity) {
  keys.emplace_back(key);
  capacities.emplace_back(capacity);
}

/// Append one ObjectCapacities to another. Does NOT dedupe
void ObjectCapacities::Append(const ObjectCapacities &other) {
  keys.insert(keys.end(), other.keys.begin(), other.keys.end());
  capacities.insert(capacities.end(), other.capacities.begin(), other.capacities.end());
}

/// Merge two ObjectCapacities. If key exists, do not append from other
void ObjectCapacities::Merge(const ObjectCapacities &other) {
  for(int i = 0; i<other.keys.size(); i++) {
    auto it = std::find(keys.begin(), keys.end(), other.keys[i]);
    if(it == keys.end()) {
      Append(other.keys[i], other.capacities[i]);
    }
  }
}

/// Locate a particular key
int ObjectCapacities::FindIndex(const kelpie::Key &key) {
  for(int i = 0; i<keys.size(); i++)
    if(keys[i] == key) return i;
  return -1;
}



/// Clear out all entries
void ObjectCapacities::Wipe() {
  keys.clear();
  capacities.clear();
}

void ObjectCapacities::sstr(std::stringstream &ss, int depth, int indent) const {
  ss<<string(indent,' ') << "ObjectCapacities Num: "<<keys.size()<<endl;
  if(depth>=0) {
    for(int i=0; i<keys.size();i++){
      ss<<string(indent+2,' ')<<"["<<i<<"] "<<keys[i].str() << "\t" << capacities[i]<<endl;
    }
  }
}

} // namespace kelpie
