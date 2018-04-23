// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "lunasa/core/LunasaCoreUnconfigured.hh"

#include "webhook/Server.hh"

using namespace std;

namespace lunasa {
namespace internal {


LunasaCoreUnconfigured::LunasaCoreUnconfigured()
  : LunasaCoreBase("Unconfigured"){
  //No init needed - all handled by Singleton now
}

LunasaCoreUnconfigured::~LunasaCoreUnconfigured() {
  //No cleanup needed - all handled by Singleton now
}


void LunasaCoreUnconfigured::init(string lmm_name, string emm_name, bool use_webhook,
                               const faodel::Configuration &config){
  Panic("(LunasaCoreUnconfigured) init");
}

void LunasaCoreUnconfigured::RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) {
  Panic("(LunasaCoreUnconfigured) RegisterPinUnpin");
}

allocation_t *LunasaCoreUnconfigured::AllocateEager(uint32_t metaCapacity, uint32_t dataCapacity)
{
  Panic("AllocateEager");
  return nullptr;
}

allocation_t *LunasaCoreUnconfigured::AllocateLazy(uint32_t metaCapacity, uint32_t dataCapacity)
{
  Panic("AllocateLazy");
  return nullptr;
}

size_t LunasaCoreUnconfigured::TotalAllocated() const {
  Panic("Total Allocated");
  return 0;
}

size_t LunasaCoreUnconfigured::TotalManaged() const {
  Panic("Total Managed");
  return 0;
}

size_t LunasaCoreUnconfigured::TotalUsed() const {
  Panic("Total Used");
  return 0;
}

size_t LunasaCoreUnconfigured::TotalFree() const {
  Panic("Total Free");
  return 0;
}

bool LunasaCoreUnconfigured::SanityCheck() {
  return false;
}
void LunasaCoreUnconfigured::PrintState(ostream& stream) {
  stream << "Lunasa is in an Unconfigured state\n";
}

Lunasa LunasaCoreUnconfigured::GetLunasaInstance() {
  Panic("GetLunasaInstance");
  return Lunasa();
}


void LunasaCoreUnconfigured::Panic(string fname) const {
  cerr << "Error: Attempted to use Lunasa "<<fname<<"() before calling lunasa::Init().\n"
       << "       Lunasa must be initialized by hand or by faodel::Bootstrap before use\n";
  exit(-1);
}

void LunasaCoreUnconfigured::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[LunasaCore] "
     << " Type: " << GetType()
     << endl;
}

} // namespace internal
} // namespace lunasa
