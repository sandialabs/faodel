// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_TYPES_HH
#define LUNASA_TYPES_HH

#include <functional>

#include "faodel-common/ReplyStream.hh"

//Forward refs
namespace lunasa { class DataObject; }

namespace lunasa {

typedef uint16_t dataobject_type_t;

using fn_DataObjectDump_t = std::function<void (const lunasa::DataObject &ldo, faodel::ReplyStream &rs)>;


} // namespace lunasa

#endif // LUNASA_TYPES_HH
