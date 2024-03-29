// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <fstream>
#include <algorithm>
#include <unistd.h> //sleep

#include "opbox/core/Singleton.hh"
#include "dirman/DirMan.hh"

#include "dirman/core/Singleton.hh"
#include "dirman/core/DirManCoreBase.hh"
#include "dirman/core/DirManCoreUnconfigured.hh"
#include "DirMan.hh"


using namespace std;
using namespace faodel;

namespace dirman {


//Locate which node is responsible for hosting a particular dir. In
//centralized, the reference node is always the cental DM node. In distribted
//this is the node that is the owner of the dir.
bool Locate(const ResourceURL &search_url, nodeid_t *reference_node)   {
  return dirman::internal::Singleton::impl.core->Locate(search_url, reference_node);
}

//Lookup information about a directory. Local cache is always queried first. This may
//incur additional network operations to (1) query remote nodes to find where the
//directory is hosted and (2) retrieve the actual directory info
bool GetDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info)       {
  return dirman::internal::Singleton::impl.core->GetDirectoryInfo(url, true,  true,  dir_info);
}
bool GetLocalDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info)  {
  return dirman::internal::Singleton::impl.core->GetDirectoryInfo(url, true,  false, dir_info);
}
bool GetRemoteDirectoryInfo(const ResourceURL &url, DirectoryInfo *dir_info) {
  return dirman::internal::Singleton::impl.core->GetDirectoryInfo(url, false, true,  dir_info);
}


/**
 * @brief Define a new resource (when no nodes have been allocated yet)
 * @param url Information about the resource
 * @retval TRUE This was added and is a new item
 * @retval FALSE This was not added because the url wasn't valid
 *
 * This function is for when someone wants to define a new resource, but they
 * don't have a full list of nodes to populate the resource yet. In most cases
 * it is identical to HostNewDir. The main difference is that HostNewDir will
 * mark this node as the owner of the dir, which is problematic if you're
 * using the faodel CLI tool to define resources (eg, you don't want to
 * register the tool's nodeid as the reference for the system).
 */
bool DefineNewDir(const faodel::ResourceURL &url) {
  return dirman::internal::Singleton::impl.core->DefineNewDir(url);
}

//User wants to host a new directory that others can reference. Host the info
//locally, and possibly publish the reference to the resource's parent
/**
 * @brief Define a new resource and mark this node as the reference node
 * @param dir_info The directory info for the new resource
 * @retval TRUE This was added and is a new item
 * @retval FALSE This was not added because the dir's url wasn't valid
 *
 * @note The url needs to have the reference node set to this node.
 *
 * User wants to host a new directory that others can reference. Host the
 * info locally, and possibly publish the reference to the resource's parent.
 */
bool HostNewDir(const DirectoryInfo &dir_info) {
  return dirman::internal::Singleton::impl.core->HostNewDir(dir_info);
}
bool HostNewDir(const ResourceURL &url) {
  return dirman::internal::Singleton::impl.core->HostNewDir(url);
}

//User wants this node to be used as part of an existing dir (join) or
//removed from participation (leave). When joining you can provide a name
//for your entity or let the owner generate a name for you.
bool JoinDirWithoutName(const ResourceURL &url, DirectoryInfo *dir_info) {
  return dirman::internal::Singleton::impl.core->JoinDirWithoutName(url, dir_info);
}
bool JoinDirWithName(const ResourceURL &url, const string &name, DirectoryInfo *dir_info) {
  return dirman::internal::Singleton::impl.core->JoinDirWithName(url, name, dir_info);
}

bool LeaveDir(const ResourceURL &url, DirectoryInfo *dir_info) {
  return dirman::internal::Singleton::impl.core->LeaveDir(url, dir_info);
}

/**
 * @brief Tell directory manager to stop hosting information about a particular Dir
 * @param url The directory to remove
 * @retval TRUE Always completes
 * @note This only removes references on the dirman server. It does not shutdown the server or wipe
 *       out info cached at other nodes in the system
 */
bool DropDir(const faodel::ResourceURL &url) {
  return dirman::internal::Singleton::impl.core->DropDir(url);
}

/**
 * @brief Return info on which node dirman talks to for locating info
 * @return nodeid_t The id of the node that's in charge (eg root node)
 */
faodel::nodeid_t GetAuthorityNode() {
  return dirman::internal::Singleton::impl.core->GetAuthorityNode();
}

/**
 * @brief Get a list of the resource names this node currently knows about (in form "[bucket]/path/name")
 * @param[out] names The vector of names to append
 */
void GetCachedNames(vector<string> *names) {
  return dirman::internal::Singleton::impl.core->GetCachedNames(names);
}


} // namespace dirman


