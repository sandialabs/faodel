// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "dirman/core/DirManCoreBase.hh"
#include "dirman/core/DirManCoreUnconfigured.hh"
#include "dirman/core/DirManCoreCentralized.hh"



#include "whookie/Server.hh"

using namespace std;
using namespace faodel;


namespace dirman {
namespace internal {

DirManCoreUnconfigured::DirManCoreUnconfigured()
  : DirManCoreBase(faodel::internal_use_only) {
}
DirManCoreUnconfigured::~DirManCoreUnconfigured() = default;


//The unconfigured core should not be asked to do the internal start, init, or
//finish calls because it is never the real module. Init should pick a real
//core to create and then direct start/finish/init to it.
void DirManCoreUnconfigured::start()                                                                  { Panic("start"); }
void DirManCoreUnconfigured::finish()                                                                 { Panic("finish"); }

bool DirManCoreUnconfigured::Locate(const ResourceURL &u, nodeid_t *r)                                { return Panic("Locate"); }
bool DirManCoreUnconfigured::GetDirectoryInfo(const ResourceURL &u, bool l, bool r, DirectoryInfo *d) { return Panic("GetDirectoryInfo"); }
bool DirManCoreUnconfigured::DefineNewDir(const DirectoryInfo &d)                                     { return Panic("DefineNewDir"); }
bool DirManCoreUnconfigured::HostNewDir(const DirectoryInfo &d)                                       { return Panic("HostNewDir"); }
bool DirManCoreUnconfigured::JoinDirWithName(const ResourceURL &u, string name, DirectoryInfo *d)     { return Panic("JoinDirWithName"); }
bool DirManCoreUnconfigured::LeaveDir(const ResourceURL &u, DirectoryInfo *d)                         { return Panic("LeaveDir"); }
bool DirManCoreUnconfigured::DropDir(const ResourceURL &u)                                            { return Panic("DropDir");}
//Internal API for implementing Exposed API. Most of these call Panic() to catch unconfigured system
bool DirManCoreUnconfigured::discoverParent(const ResourceURL &u, nodeid_t *r)                        { return Panic("discoverParent"); }
bool DirManCoreUnconfigured::cacheForeignDir(const DirectoryInfo &d)                                  { return Panic("cacheForeignDir"); }
bool DirManCoreUnconfigured::lookupRemote(nodeid_t n, const ResourceURL &u, DirectoryInfo *d)         { return Panic("lookupRemote"); }
bool DirManCoreUnconfigured::joinRemote(nodeid_t p, const ResourceURL &u, bool s)                     { return Panic("joinRemote"); }



bool DirManCoreUnconfigured::Panic(string fname) const {

  if(dirman_service_none){
    cerr << "Error: Attemped to use DirMan command "<<fname<<"(), but the DirMan was not\n"
         << "       configured to run. The DirMan can be enabled by setting dirman.type in Configuration\n"
         << "       to a functional implementation (eg, 'centralized' or 'static')\n";
    exit(-1);
  } else {

    cerr << "Error: Attempted to use DirMan "<<fname<<"() before calling DirMan::Init().\n"
         << "       DirMan must be initialized by hand or by faodel::Bootstrap before use\n";
    exit(-1);
  }
  return false;
}

void DirManCoreUnconfigured::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[DirMan] CurrentType: Unconfigured";
}

} // namespace internal
} // namespace dirman


