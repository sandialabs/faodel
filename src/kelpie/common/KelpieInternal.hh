// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_KELPIEINTERNAL_HH
#define KELPIE_KELPIEINTERNAL_HH

// Note: This file is mainly here to expose internal items
//       out to user space applications, for debugging purposes


#include "kelpie/localkv/LocalKV.hh"


namespace kelpie {
namespace internal {

void getLKV(kelpie::LocalKV **lkv_ptr);


}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_KELPIEINTERNAL_HH
