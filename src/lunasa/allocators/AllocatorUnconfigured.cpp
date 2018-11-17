// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "lunasa/allocators/AllocatorUnconfigured.hh"


using namespace std;

namespace lunasa {
namespace internal {


AllocatorUnconfigured::AllocatorUnconfigured()
        : AllocatorBase(faodel::Configuration(), "Unconfigured", false) {
}

void AllocatorUnconfigured::Panic(std::string fname) const {
  fatal("Lunasa used " + fname + "() before calling Init()");
}

void AllocatorUnconfigured::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth < 0) return;
  ss << std::string(indent, ' ') << "[Allocator] Type: Unconfigured" << endl;
}

} // namespace internal
} // namespace lunasa
