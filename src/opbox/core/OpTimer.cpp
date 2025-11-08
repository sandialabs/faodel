// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <faodel-common/Debug.hh>
#include <opbox/OpBox.hh>
#include "opbox/core/OpTimer.hh"

using namespace std;

namespace opbox {
namespace internal {

OpTimer::OpTimer() {
  mutex = faodel::GenerateMutex();
}
OpTimer::~OpTimer() {
  delete mutex;
}

std::string OpTimerEvent::str() {
  switch(value) {
    case OpTimerEvent::Incoming: return "Incoming";
    case OpTimerEvent::Update: return "Update";
    case OpTimerEvent::Launch: return "Launch";
    case OpTimerEvent::Trigger: return "Trigger";
    case OpTimerEvent::Dispatched: return "Dispatched";
    case OpTimerEvent::ActionComplete: return "ActionComplete";
  }
  return "Unknown?";
}

uint64_t OpTimer::op_timestamp_t::getGapTimeUS(const op_timestamp_t &prv) {
  uint64_t t1 = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
  uint64_t t0 = std::chrono::duration_cast<std::chrono::microseconds>(prv.time.time_since_epoch()).count();
  return t1-t0;
}


void OpTimer::Mark(Op *op, OpTimerEvent event) {
  op_timestamp_t ts(op,event);
  mutex->WriterLock();
  timestamps.emplace_back(std::move(ts));
  mutex->Unlock();
}

void OpTimer::MarkDispatched(mailbox_t m) {
  op_timestamp_t ts(m);
  mutex->WriterLock();
  timestamps.emplace_back(std::move(ts));
  mutex->Unlock();
}

void OpTimer::Dump() {

  mutex->Lock();
  std::set<mailbox_t> visited_mboxes;

  cout<<"Time stamps ("<<timestamps.size()<<"):\n";
  for(int i=0; i<timestamps.size(); i++) {

    if(visited_mboxes.find(timestamps[i].mbox) != visited_mboxes.end() )
      continue;

    op_timestamp_t ts=timestamps[i];
    visited_mboxes.insert(ts.mbox);
    string opname = opbox::GetOpName(ts.opid);
    if(opname.empty()) opname = "Unknown?";

    cout <<"OP_TIMER["<<ts.mbox<<"]"
         <<" Op: \t"<<opname;

    std::chrono::high_resolution_clock::time_point ptime = timestamps[i].time;
    for(int j=i; j<timestamps.size(); j++) {
      if(timestamps[j].mbox==ts.mbox) {
        cout << "\t"<<timestamps[j].event.str() << " +"<<timestamps[j].getGapTimeUS(ts)<<"us";
        ts = timestamps[j];
      }
    }
    cout <<endl;
  }
  mutex->Unlock();

}

} //namespace internal
} //namespace opbox
