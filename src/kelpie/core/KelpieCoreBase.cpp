// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.




#include "kelpie/Kelpie.hh"
#include "kelpie/core/KelpieCoreBase.hh"
#include "kelpie/core/KelpieCoreUnconfigured.hh"

using namespace std;
using namespace faodel;

namespace kelpie {
namespace internal {

extern KelpieCoreUnconfigured kelpie_core_unconfigured;  //For resource registration

KelpieCoreBase::KelpieCoreBase()
        : LoggingInterface("kelpie") {
}
KelpieCoreBase::~KelpieCoreBase() = default;


}  // namespace internal
}  // namespace kelpie
