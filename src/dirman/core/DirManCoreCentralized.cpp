// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "opbox/OpBox.hh"
#include "opbox/net/net.hh"
#include "opbox/services/dirman/core/DirManCoreCentralized.hh"
#include "opbox/services/dirman/ops/OpDirManCentralized.hh"

#include "webhook/Server.hh"

using namespace std;
using namespace faodel;

namespace opbox {
namespace dirman {
namespace internal {


DirManCoreCentralized::DirManCoreCentralized(const faodel::Configuration &config)
  : DirManCoreBase(config, "Centralized"), 
    am_root(false) {

  string root_node_hex;
  bool dbg_am_root;

  config.GetBool(&am_root,                  "dirman.host_root",            "false");
  config.GetLowercaseString(&root_node_hex, "dirman.root_node",            "");
  config.GetBool(&dbg_am_root,              "dirman.testing_mode.am_root", "false");


  if(dbg_am_root){
    //This is a special testing mode that allows tests to bring up node without any net ops
    am_root=true;
    my_node = faodel::nodeid_t(0x19901990,internal_use_only);
    root_id = my_node;
    dbg("Enabling testing mode. This node functions as the root");

  } else if (!root_node_hex.empty()){
    //Query to find the root node
    root_id = nodeid_t(root_node_hex);

  } else if (am_root){
    dbg("Am hosting root");
    //root_id can't be set here because network not up yet

  } else if (root_node_hex.empty()){
    error("DirManCoreCentralized: no dirman.root_node provided in configuration");
    KTODO("DMCC handle no root node in config");
  }
  //Note: my_node isn't set until net is ready, during start

  //Register our Op
  opbox::RegisterOp<OpDirManCentralized>();

}

DirManCoreCentralized::~DirManCoreCentralized() {
  opbox::DeregisterOp<OpDirManCentralized>(true);
}

void DirManCoreCentralized::start(){
  my_node = opbox::net::GetMyID();
  if(am_root) root_id=my_node;
  else        am_root = (root_id == my_node);
}

void DirManCoreCentralized::finish(){
  KTODO("DirManCoreCentralized Finish");
}

/**
 * @brief Locate the node that is responsible for hosting resource (note: always the root node)
 * @param search_url -  The resource to locate
 * @param reference_node -  The node that manages the resource (in this case, always the root node)
 * @retval TRUE Always successful, as the answer in this case is always the root node
 */
bool DirManCoreCentralized::Locate(const ResourceURL &search_url, nodeid_t *reference_node) {
  dbg("Locate "+search_url.GetURL());
  if(reference_node) *reference_node = root_id;
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
bool DirManCoreCentralized::GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, DirectoryInfo *dir_info) {
  dbg("GetDirInfo Requesting "+url.GetURL());

  //Fixup the url by filling in the bucket
  faodel::ResourceURL url_mod = localizeURL(url, false);

  if(am_root){
    return dc_mine.Lookup(url_mod, dir_info);

  } else {

    if(check_local){
      bool found = dc_others.Lookup(url_mod, dir_info);
      if(found) return found;
    }
    if(!check_remote) return false;

    //Launch a message
    OpDirManCentralized *op = new OpDirManCentralized(OpDirManCentralized::RequestType::GetInfo, root_id, url_mod);
    future<DirectoryInfo> fut1 = op->GetFuture();
    opbox::LaunchOp(op);

    //Block until get result
    DirectoryInfo di2 = fut1.get();
    dbg("GetDirInfo Got result back: "+di2.to_string());
    dc_others.CreateAndLinkParents(di2);

    if(dir_info) *dir_info = di2;

    return true;

  }
}

/**
 * @brief Create a new directory entry. In this case, push info to root node
 * @param dir_info -  Information for new directory entry
 * @retval TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource already exists and therefore was NOT modified
 */
bool DirManCoreCentralized::HostNewDir(const DirectoryInfo &dir_info) {
  dbg("HostNewDir "+dir_info.to_string());

  //Modify the dir_info so that (1) the url has our bucket in it if not set
  //and (2) the reference node is root.
  DirectoryInfo dir_info_mod = dir_info;
  dir_info_mod.url = localizeURL(dir_info.url, true);
  dir_info_mod.url.reference_node = root_id;

  if(am_root){
    return dc_mine.CreateAndLinkParents(dir_info_mod);

  } else {

    //Launch a message
    OpDirManCentralized *op = new OpDirManCentralized(OpDirManCentralized::RequestType::HostNewDir, root_id, dir_info_mod);
    future<DirectoryInfo> fut1 = op->GetFuture();
    opbox::LaunchOp(op);

    //Block until get result
    DirectoryInfo di2 = fut1.get();
    dbg("HostNewDir Got result back: "+di2.to_string());
    return dc_others.CreateAndLinkParents(di2);
    //TODO: is this right?
  }
}

/**
 * @brief Let a node join an existing resource.
 * @param[in] url The url for the child (parent found via GetLineage)
 * @param[in] name The name to use when joining
 * @param[out] dir_info The updated directory entry 
 * @retval TRUE Found and updated the resource
 * @retval FALSE Did not find the resource here (or attempted to register root-level url). no changes made
 */
bool DirManCoreCentralized::JoinDirWithName(const faodel::ResourceURL &url, string name, DirectoryInfo *dir_info) {
  dbg("JoinDir "+url.GetURL());

  //Fixup the dir_info by filling in the root/bucket
  faodel::ResourceURL url_mod = localizeURL(url, true);

  if(!name.empty()){
    url_mod.PushDir(name);
  }

  if(am_root){
    return dc_mine.Join(url_mod, dir_info);

  } else {

    //Launch a message to root
    OpDirManCentralized *op = new OpDirManCentralized(OpDirManCentralized::RequestType::JoinDir, root_id, url_mod);
    future<DirectoryInfo> fut1 = op->GetFuture();
    opbox::LaunchOp(op);

    //Block until get result
    DirectoryInfo di2 = fut1.get();
    dbg("JoinDir Got result back: "+di2.to_string());
    if(dir_info) *dir_info = di2;
    return dc_others.Update(di2);

  }
}

/**
 * @brief Remove a node from a particular directory's membership list
 * @param url -  The url for the node
 * @param dir_info -  The updated directory info
 * @retval TRUE Found the node and removed it from the resource
 * @retval FALSE Unable to remove the node (eg, not a member or resource does not exist)
 */
bool DirManCoreCentralized::LeaveDir(const faodel::ResourceURL &url, DirectoryInfo *dir_info) {
  dbg("LeaveDir "+url.GetURL());

  //Fixup the dir_info by filling in the bucket. Note
  faodel::ResourceURL url_mod = localizeURL(url, false);

  if(am_root){
    return dc_mine.Leave(url_mod, dir_info);

  } else {

    //Launch a message
    OpDirManCentralized *op = new OpDirManCentralized(OpDirManCentralized::RequestType::LeaveDir, root_id, url_mod);
    future<DirectoryInfo> fut1 = op->GetFuture();
    opbox::LaunchOp(op);

    //Block until get result
    DirectoryInfo di2 = fut1.get();
    dbg("LeaveDir Got result back: "+di2.to_string());
    return dc_others.Update(di2);
  }
}

bool DirManCoreCentralized::discoverParent(const ResourceURL &resource_url, nodeid_t *parent_node){

  dbg("discover parent of "+resource_url.GetFullURL());

  if(resource_url.IsRootLevel()) return false;
  if(parent_node) *parent_node=root_id;

  return true;
}

bool DirManCoreCentralized::cacheForeignDir(const DirectoryInfo &dir_info) {
  KTODO("cacheForeignDir");
}
bool DirManCoreCentralized::lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, DirectoryInfo *dir_info){
  KTODO("lookupRemote");
}
bool DirManCoreCentralized::joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply){
  KTODO("joinRemote");
}

/**
 * @brief Create a modified URL that fills in the default bucket (and node) if UNSPECIFIED
 * @param url the source url to use
 * @param change_node (optional) Also update reference_node w/ this node's value if NODE_UNSPECIFIED
 * @retval url A new url with updated fields
 */
faodel::ResourceURL DirManCoreCentralized::localizeURL(const faodel::ResourceURL &url, bool change_node){
  faodel::ResourceURL url_mod = url;

  if(url_mod.bucket == BUCKET_UNSPECIFIED)
     url_mod.bucket = default_bucket;

  if((change_node) &&
     (url_mod.reference_node == NODE_UNSPECIFIED))
     url_mod.reference_node = my_node;

  return url_mod;
}


void DirManCoreCentralized::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[DirManCentralized] "
     << "AmRoot: " << am_root
     << " Root ID: "<<root_id.GetHex()
     << endl;

}

} // namespace internal
} // namespace dirman
} // namespace opbox

