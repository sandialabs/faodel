// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <algorithm>
#include <unistd.h> //sleep

#include "opbox/core/Singleton.hh"
#include "opbox/services/dirman/DirectoryManager.hh"

#include "opbox/services/dirman/core/Singleton.hh"
#include "opbox/services/dirman/core/DirManCoreBase.hh"
#include "opbox/services/dirman/core/DirManCoreUnconfigured.hh"


using namespace std;
using namespace faodel;

namespace opbox {
namespace dirman {


//Locate which node is responsible for hosting a particular dir. In
//centralized, the reference node is always the cental DM node. In distribted
//this is the node that is the owner of the dir.
bool Locate(const ResourceURL &search_url, nodeid_t *reference_node)   {
  return opbox::internal::Singleton::impl_dm.core->Locate(search_url, reference_node);
}

//Lookup information about a directory. Local cache is always queried first. This may
//incur additional network operations to (1) query remote nodes to find where the
//directory is hosted and (2) retrieve the actual directory info
bool GetDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info)       {
  return opbox::internal::Singleton::impl_dm.core->GetDirectoryInfo(url, true,  true,  dir_info);
}
bool GetLocalDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info)  {
  return opbox::internal::Singleton::impl_dm.core->GetDirectoryInfo(url, true,  false, dir_info);
}
bool GetRemoteDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info) {
  return opbox::internal::Singleton::impl_dm.core->GetDirectoryInfo(url, false, true,  dir_info);
}


//User wants to host a new directory that others can reference. Host the info
//locally, and possibly publish the reference to the resource's parent
bool HostNewDir(const DirectoryInfo &dir_info)                                     {
  return opbox::internal::Singleton::impl_dm.core->HostNewDir(dir_info);
}
bool HostNewDir(const ResourceURL &url)                                            {
  return opbox::internal::Singleton::impl_dm.core->HostNewDir(url); }

//User wants this node to be used as part of an existing dir (join) or
//removed from participation (leave). When joining you can provide a name
//for your entity or let the owner generate a name for you.
bool JoinDirWithoutName(const ResourceURL &url, DirectoryInfo *dir_info)           {
  return opbox::internal::Singleton::impl_dm.core->JoinDirWithoutName(url, dir_info);
}
bool JoinDirWithName(const ResourceURL &url, string name, DirectoryInfo *dir_info) {
  return opbox::internal::Singleton::impl_dm.core->JoinDirWithName(url, name, dir_info);
}

bool LeaveDir(const ResourceURL &url, DirectoryInfo *dir_info)                     {
  return opbox::internal::Singleton::impl_dm.core->LeaveDir(url, dir_info);
}

} // namespace dirman
} // namespace opbox

