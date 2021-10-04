// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "opbox/ops/Op.hh"


using namespace std;
using namespace opbox;

//Note: This should be mailbox_t, but fetch and add complains if it
//      isn't int?

namespace opbox {
namespace net {
namespace internal{
  atomic<int> next_mailbox = ATOMIC_VAR_INIT(1); //0 is reserved for no mailbox
} //end opbox::net::internal

opbox::mailbox_t GetNextMailbox(){
  return std::atomic_fetch_add(&internal::next_mailbox,1);
} //end opbox::net::internal
} //end opbox::net
} //end opbox


/**
 * @brief Return the unique mailbox for this op (generating one of unspecified)
 * @retval mailbox_t The unique mailbox identifier for this operation
 */
opbox::mailbox_t Op::GetAssignedMailbox(){
  //Set if not used
  if(mailbox==MAILBOX_UNSPECIFIED)
    mailbox=std::atomic_fetch_add(&opbox::net::internal::next_mailbox,1);
  return mailbox;
}

int Op::GetSecondsSinceCreated() const {
  return (getMSTimeStamp() - ts_created)/1000;
}
int Op::GetSecondsSinceAccessed() const {
  return (getMSTimeStamp() - ts_lastaccessed)/1000;
}
void Op::touch() {
  ts_lastaccessed = getMSTimeStamp();
}

int Op::getMSTimeStamp() const {
  auto unix_timestamp = std::chrono::seconds(std::time(nullptr));
  return std::chrono::milliseconds(unix_timestamp).count();
}
