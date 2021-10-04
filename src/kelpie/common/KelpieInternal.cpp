// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/core/Singleton.hh"


namespace kelpie {

/**
 * @brief Shortcut to get a pointer back to Kelpie's LocalKV
 * @param[out] localkv_ptr Caller's pointer that needs updating
 */
void internal::getLKV(LocalKV **localkv_ptr) {
  return kelpie::internal::Singleton::impl.core->getLKV(localkv_ptr);
}


}  // namespace kelpie
