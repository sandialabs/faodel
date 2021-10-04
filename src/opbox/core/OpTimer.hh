// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPBOX_OPTIMER_HH
#define OPBOX_OPBOX_OPTIMER_HH

#include <vector>
#include "opbox/ops/Op.hh"

namespace opbox {
namespace internal {

class OpTimerEvent {
public:
  enum Value: int {
    Incoming=0,
    Update,
    Launch,
    Trigger,
    Dispatched,
    ActionComplete
  };
  OpTimerEvent()=default;
  std::string str();
  constexpr OpTimerEvent(Value e) :value(e) {}
private:
  Value value;
};

/**
 * @brief Timing code to help estimate how long it takes to execute ops
 *
 * This timer creates a trace of all the different events that are passed
 * to each op. An instrumented OpBoxCore should include one of these
 * structures and use Mark commands to add new events to the trace. The
 * op's mailbox id is extracted from the op to provide a tag for grouping
 * ops. The Dump command sorts by mailbox and shows the amount of time
 * since the previous marker in this op.
 */
class OpTimer {

public:
  OpTimer();
  ~OpTimer();
  void Mark(Op *op, OpTimerEvent event);
  void MarkDispatched(mailbox_t);
  void Dump();

private:
  struct op_timestamp_t {
    mailbox_t mbox;
    unsigned int opid;
    OpTimerEvent event;
    std::chrono::high_resolution_clock::time_point time;

    op_timestamp_t(Op *op, OpTimerEvent event)
            : mbox(op->GetAssignedMailbox()), opid(op->getOpID()), event(event) {
      time = std::chrono::high_resolution_clock::now();
    }
    op_timestamp_t(mailbox_t m)
            : mbox(m), opid(0), event(OpTimerEvent::Dispatched) {
      time = std::chrono::high_resolution_clock::now();
    }

    uint64_t getGapTimeUS(const op_timestamp_t &prv);
  };


private:

  faodel::MutexWrapper *mutex;
  std::vector <op_timestamp_t> timestamps;
};

} //namespace internal
} //namespace opbox

#endif //OPBOX_OPBOX_OPTIMER_HH
