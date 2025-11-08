// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_HELPERS_HH
#define LUNASA_HELPERS_HH

#include <string>
#include <sstream>

#include "lunasa/DataObject.hh"

namespace lunasa {

DataObject AllocateStringObject(const std::string &s);
std::string UnpackStringObject(DataObject &ldo);

} // namespace lunasa

#endif //FAODEL_HELPERS_HH
