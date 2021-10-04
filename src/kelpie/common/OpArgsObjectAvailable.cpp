// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>

#include "kelpie/common/OpArgsObjectAvailable.hh"

using namespace std;

namespace kelpie {

void OpArgsObjectAvailable::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss <<string(indent,' ')<<"[OpboxArg] Type: "+opbox::str(type);
}

}  // namespace opbox
