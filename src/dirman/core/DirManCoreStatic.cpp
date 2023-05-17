// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "opbox/OpBox.hh"
#include "opbox/net/net.hh"
#include "dirman/core/DirManCoreStatic.hh"

#include "whookie/Server.hh"

using namespace std;
using namespace faodel;

namespace dirman {
namespace internal {


DirManCoreStatic::DirManCoreStatic(const faodel::Configuration &config)
  : DirManCoreBase(config, "Static") {


  //The base class may have plugged a bunch of urls from config into dc_others. The root
  //node needs these moved to dc_mine because root will only look there.
  vector<ResourceURL> predefined_urls;
  vector<DirectoryInfo> dirs;
  dc_others.GetAllURLs(&predefined_urls);
  dc_others.Lookup(predefined_urls, &dirs);
  for(auto &d : dirs){
      dc_others.Remove(d.url);
      d.url = localizeURL(d.url, true);
      dc_others.Update(d);
  }

}

DirManCoreStatic::~DirManCoreStatic() {
}

void DirManCoreStatic::start(){
}

void DirManCoreStatic::finish(){
}

/**
 * @brief Locate the node that is responsible for hosting resource (note: always the root node)
 * @param search_url -  The resource to locate
 * @param reference_node -  The node that manages the resource (in this case, always the root node)
 * @retval TRUE Always successful, as the answer in this case is always the root node
 */
bool DirManCoreStatic::Locate(const ResourceURL &search_url, nodeid_t *reference_node) {
  dbg("Locate "+search_url.GetURL());
  if(reference_node) *reference_node = my_node;
  return true;
}

/**
 * @brief Retrieve info about a particular resource directory entry
 * @param url -  Resource URL to look up
 * @param check_local - Check our local cache for answer first
 * @param check_remote - Check the remote node responsible for dir (if local not successful)
 * @param dir_info -  The resulting directory info (set to empty value if no entry)
 * @retval TRUE The entry was found
 * @retval FALSE The entry was no found
 */
bool DirManCoreStatic::GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, DirectoryInfo *dir_info) {
  dbg("GetDirInfo Requesting "+url.GetURL());

  //Fixup the url by filling in the bucket
  faodel::ResourceURL url_mod = localizeURL(url, false);
  bool found = dc_mine.Lookup(url_mod, dir_info);
  if(!found) {
    found = dc_others.Lookup(url_mod, dir_info);
  }
  return found;
}

/**
 * @brief Define a new directory entry (but don't host it)
 * @param dir_info Information for new directory entry
 * @retval TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource already exists and therefore was NOT modified
 */
bool DirManCoreStatic::DefineNewDir(const DirectoryInfo &dir_info) {
  dbg("DefineNewDir "+dir_info.to_string());
  return HostNewDir(dir_info);
}

/**
 * @brief Create a new directory entry. In this case, push info to root node
 * @param dir_info Information for new directory entry
 * @retval TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource already exists and therefore was NOT modified
 */
bool DirManCoreStatic::HostNewDir(const DirectoryInfo &dir_info) {
  dbg("HostNewDir "+dir_info.to_string());

  //Modify the dir_info so that (1) the url has our bucket in it if not set
  //and (2) the reference node is root.
  DirectoryInfo dir_info_mod = dir_info;
  dir_info_mod.url = localizeURL(dir_info.url, true);

  return dc_mine.CreateAndLinkParents(dir_info_mod);

}

/**
 * @brief Let a node join an existing resource.
 * @param[in] url The url for the child (parent found via GetLineage)
 * @param[in] name The name to use when joining
 * @param[out] dir_info The updated directory entry 
 * @retval TRUE Found and updated the resource
 * @retval FALSE Did not find the resource here (or attempted to register root-level url). no changes made
 */
bool DirManCoreStatic::JoinDirWithName(const faodel::ResourceURL &url, string name, DirectoryInfo *dir_info) {
  dbg("JoinDir "+url.GetURL());

  //Fixup the dir_info by filling in the root/bucket
  faodel::ResourceURL url_mod = localizeURL(url, true);

  if(!name.empty()){
    url_mod.PushDir(name);
  }

  return dc_mine.Join(url_mod, dir_info);
}

/**
 * @brief Remove a node from a particular directory's membership list
 * @param url -  The url for the node
 * @param dir_info -  The updated directory info
 * @retval TRUE Found the node and removed it from the resource
 * @retval FALSE Unable to remove the node (eg, not a member or resource does not exist)
 */
bool DirManCoreStatic::LeaveDir(const faodel::ResourceURL &url, DirectoryInfo *dir_info) {
  dbg("LeaveDir "+url.GetURL());

  //Fixup the dir_info by filling in the bucket. Note
  faodel::ResourceURL url_mod = localizeURL(url, false);

  return dc_mine.Leave(url_mod, dir_info);
}

/**
 * @brief Instruct the root node to drop a specific directory
 * @param url The resource reference to drop
 * @return true
 * @note This only removes the entry from the local node. It doesn't remove references elsewhere
 */
bool DirManCoreStatic::DropDir(const faodel::ResourceURL &url) {
  dbg("DropDir "+url.GetURL());

  //Fixup the dir_info by filling in the bucket. Note
  faodel::ResourceURL url_mod = localizeURL(url, false);

  return dc_mine.Remove(url_mod);
}

bool DirManCoreStatic::discoverParent(const ResourceURL &resource_url, nodeid_t *parent_node){
  F_TODO("discoverParent");
}

bool DirManCoreStatic::cacheForeignDir(const DirectoryInfo &dir_info) {
  F_TODO("cacheForeignDir");
}
bool DirManCoreStatic::lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, DirectoryInfo *dir_info){
  F_TODO("lookupRemote");
}
bool DirManCoreStatic::joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply){
  F_TODO("joinRemote");
}

/**
 * @brief Create a modified URL that fills in the default bucket (and node) if UNSPECIFIED
 * @param url the source url to use
 * @param change_node (optional) Also update reference_node w/ this node's value if NODE_UNSPECIFIED
 * @retval url A new url with updated fields
 */
faodel::ResourceURL DirManCoreStatic::localizeURL(const faodel::ResourceURL &url, bool change_node){
  faodel::ResourceURL url_mod = url;

  if(url_mod.bucket == BUCKET_UNSPECIFIED)
     url_mod.bucket = default_bucket;

  if((change_node) &&
     (url_mod.reference_node == NODE_UNSPECIFIED))
     url_mod.reference_node = my_node;

  return url_mod;
}

void DirManCoreStatic::appendWhookieParameterTable(faodel::ReplyStream *rs){
}

void DirManCoreStatic::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[DirManStatic] "
     << endl;

}

} // namespace internal
} // namespace dirman

