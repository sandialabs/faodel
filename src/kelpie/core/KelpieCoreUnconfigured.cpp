// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "kelpie/common/KelpieInternal.hh"

#include "kelpie/core/KelpieCoreUnconfigured.hh"

using namespace std;


namespace kelpie {
namespace internal {


KelpieCoreUnconfigured::KelpieCoreUnconfigured() : KelpieCoreBase() {
}

KelpieCoreUnconfigured::~KelpieCoreUnconfigured(){
}


//The unconfigured core should not be asked to do the internal start, init, or
//finish calls because it is never the real module. Init should pick a real
//core to create and then direct start/finish/init to it.
void KelpieCoreUnconfigured::start()                                           { Panic("start"); }
void KelpieCoreUnconfigured::finish()                                          { Panic("finish"); }
void KelpieCoreUnconfigured::init(const faodel::Configuration &config)         { Panic("init"); }
void KelpieCoreUnconfigured::getLKV(LocalKV **localkv_ptr)                     { Panic("getLKV"); }
void KelpieCoreUnconfigured::RegisterPoolConstructor(std::string pool_name,
                                       fn_PoolCreate_t ctor_function)          { Panic("RegisterPoolConstructor"); }
Pool KelpieCoreUnconfigured::Connect(const faodel::ResourceURL &resource_url)  { Panic("Connect"); return Pool(); }
vector<string> KelpieCoreUnconfigured::GetRegisteredPoolTypes() const          { Panic("GetRegisterePoolTypes"); return {}; }

int  KelpieCoreUnconfigured::JoinServerPool(const faodel::ResourceURL & url,
                                       const std::string & optional_node_name) { Panic("JoinServerPool"); return -1; }


void KelpieCoreUnconfigured::sstr(std::stringstream &ss, int depth, int indent) const {
  ss <<"Kelpie: Currently Unconfigured (call kelpie::Init(config))\n";
}

void KelpieCoreUnconfigured::Panic(const string fname) const {
  fatal("Attempted to use Kelpie "+fname+"() before calling Kelpie::Init().\n"
        + "       Kelpie must be initialized by hand or by faodel::Bootstrap before use.");
}

}  // namespace internal
}  // namespace kelpie
