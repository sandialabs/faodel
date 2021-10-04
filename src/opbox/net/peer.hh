// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_PEER_HH
#define OPBOX_PEER_HH

namespace opbox {
namespace net {

/*
 * This is just a forward declaration.  The implementation is in each network module.
 */
struct peer_t;

typedef struct peer_t* peer_ptr_t;

}
}

#endif // OPBOX_PEER_HH
