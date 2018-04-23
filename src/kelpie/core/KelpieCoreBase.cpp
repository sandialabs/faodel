// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 




#include "kelpie/Kelpie.hh"
#include "kelpie/core/KelpieCoreBase.hh"
#include "kelpie/core/KelpieCoreUnconfigured.hh"

using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {

extern KelpieCoreUnconfigured kelpie_core_unconfigured;  //For resource registration

KelpieCoreBase::KelpieCoreBase() : LoggingInterface("kelpie") {
}
KelpieCoreBase::~KelpieCoreBase(){
}

#if 0
void KelpieCoreBase::registerPoolConstructor(string pool_name, fn_PoolCreate_t function_pointer) {
  //shortcut for registration. A core uses this to register its own handles
  //kelpie_core_unconfigured.pool_registry.RegisterPoolConstructor(pool_name, function_pointer);
  return kelpie::internal::RegisterPoolConstructor(pool_name, function_pointer);
}
#endif

}  // namespace internal
}  // namespace kelpie
