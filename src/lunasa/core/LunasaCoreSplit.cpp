// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <assert.h>

#include "lunasa/Lunasa.hh"
#include "lunasa/allocators/Allocators.hh"
#include "lunasa/common/Allocation.hh"
#include "lunasa/core/LunasaCoreSplit.hh"
#include "lunasa/core/Singleton.hh"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

using namespace std;

namespace lunasa {
namespace internal {

LunasaCoreSplit::LunasaCoreSplit() : LunasaCoreBase("Split") {
  //Plug in a placeholder that panics if trying to use unconfigured. Pass in
  //empty config to disable the logging
  faodel::Configuration empty_config;
  lazy_allocator = createAllocator(empty_config,"unconfigured", false);
  eager_allocator = reuseAllocator(lazy_allocator);
}

LunasaCoreSplit::~LunasaCoreSplit() {
  lazy_allocator->DecrRef();
  eager_allocator->DecrRef();
}

void LunasaCoreSplit::init(string lmm_name, string emm_name, bool use_whookie,
                           const faodel::Configuration &config) {
  dbg("LunasaCoreSplit init. Lazy: "+lmm_name+" Eager: "+emm_name);
  
  AllocatorBase *new_lazy_allocator = createAllocator(config, lmm_name, false);
  AllocatorBase *new_eager_allocator = createAllocator(config, emm_name, true);

  if( new_lazy_allocator == NULL || new_eager_allocator == NULL) {
    throw LunasaConfigurationException("Invalid allocator configuration");
  }

  //Get rid of previous allocations by decr to zero
  lazy_allocator->DecrRef(); 
  eager_allocator->DecrRef();

  // If we reach this point, creation of the new allocators succeeded.  
  // Now update reference to point to new allocators
  lazy_allocator = new_lazy_allocator;
  eager_allocator = new_eager_allocator;
  
  dbg("LunasaCoreSplit allocators created. Updating whookie");

  whookie::Server::updateHook("/lunasa", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieStatus(args, results);
    });
  whookie::Server::updateHook("/lunasa/eager_details", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieEagerDetails(args, results);
    });
  whookie::Server::updateHook("/lunasa/lazy_details", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieLazyDetails(args, results);
    });
}

void LunasaCoreSplit::finish() {
  whookie::Server::deregisterHook("/lunasa");
  whookie::Server::deregisterHook("/lunasa/eager_details");
  whookie::Server::deregisterHook("/lunasa/lazy_details");
}

void LunasaCoreSplit::RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) {
  eager_allocator->RegisterPinUnpin(pin, unpin);
  lazy_allocator->RegisterPinUnpin(pin, unpin);
}

Allocation *LunasaCoreSplit::AllocateEager(uint32_t user_capacity) {
  return eager_allocator->Allocate(user_capacity);
}

Allocation *LunasaCoreSplit::AllocateLazy(uint32_t user_capacity) {
  return lazy_allocator->Allocate(user_capacity);
}

size_t LunasaCoreSplit::TotalAllocated() const{
  return lazy_allocator->TotalAllocated() + eager_allocator->TotalAllocated();
}
size_t LunasaCoreSplit::TotalManaged() const{
  return lazy_allocator->TotalManaged() + eager_allocator->TotalManaged();
}
size_t LunasaCoreSplit::TotalUsed() const{
  return lazy_allocator->TotalUsed() + eager_allocator->TotalUsed();
}
size_t LunasaCoreSplit::TotalFree() const{
  return lazy_allocator->TotalFree() + eager_allocator->TotalFree();
}
  
bool LunasaCoreSplit::SanityCheck() {
  return lazy_allocator->SanityCheck();
}
void LunasaCoreSplit::PrintState(ostream& stream) {
  return lazy_allocator->PrintState(stream);
}

Lunasa LunasaCoreSplit::GetLunasaInstance() {
  //return Lunasa(faodel::internal_use_only, defaultIsLazy, lazy_allocator, eager_allocator);
  return Lunasa(faodel::internal_use_only, lazy_allocator, eager_allocator);
}


void LunasaCoreSplit::HandleWhookieStatus(const map<string,string> &args, stringstream &results){

  faodel::ReplyStream rs(args, "Lunasa Status", &results);

  rs.mkSection("Lunasa: Split Allocator");
  rs.mkText( R"(Lunasa is currently configured to use Split allocators. This means Lunasa
has one allocator for tracking lazy-pinned memory (memory that is only pinned when it is about to leave
the network) and eager-pinned memory (memory that is pinned when requested.)");

  internal::Singleton::impl.dataobject_type_registry.DumpRegistryStatus(rs);


  if(lazy_allocator==eager_allocator) {
    rs.mkText(rs.createBold("Note:")+" Lunasa is currently configured to combine lazy and eager allocators");
    lazy_allocator->whookieStatus(rs, "Lazy/Eager");
    
  } else {
    eager_allocator->whookieStatus(rs, "Eager");
    rs.mkText(html::mkLink("Eager Memory Details", "/lunasa/eager_details"));
    lazy_allocator->whookieStatus(rs, "Lazy");
    rs.mkText(html::mkLink("Lazy Memory Details", "/lunasa/lazy_details"));         
  }
  rs.Finish(); 
}

void LunasaCoreSplit::HandleWhookieEagerDetails(const map<string,string> &args, stringstream &results) {

  faodel::ReplyStream rs(args, "Lunasa Eager Allocator Details", &results);
  eager_allocator->whookieStatus(rs, "Eager");
  eager_allocator->whookieMemoryAllocations(rs, "Eager");
  rs.Finish();
}

void LunasaCoreSplit::HandleWhookieLazyDetails(const map<string,string> &args, stringstream &results) {

  faodel::ReplyStream rs(args, "Lunasa Lazy Allocator Details", &results);
  lazy_allocator->whookieStatus(rs, "Lazy");
  lazy_allocator->whookieMemoryAllocations(rs, "Lazy");
  rs.Finish();
}


void LunasaCoreSplit::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[LunasaCore] "
     << "Type: "<<GetType()
     << endl;

  if(depth<1) return;
  ss << string(indent+2,' ') << "LazyAllocator:\n";
  lazy_allocator->sstr(ss, depth-1, indent+4);
  
  if(lazy_allocator == eager_allocator){
    ss << string(indent+2,' ') << "EagerAllocator: (same as LazyAllocator)\n";
  } else {
    ss  << string(indent+2,' ') << "EagerAllocator:\n";
    eager_allocator->sstr(ss, depth-1, indent+4);
  }
}

} // namespace internal
} // namespace lunasa
