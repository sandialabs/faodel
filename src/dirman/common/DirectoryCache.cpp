// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <algorithm>

#include "faodel-common/Debug.hh"

#include "dirman/common/DirectoryCache.hh"


using namespace std;
using namespace faodel;

namespace dirman {

const std::string DirectoryCache::auto_generate_option_label = "ag";

DirectoryCache::DirectoryCache(const string &full_name)
        : LoggingInterface(full_name), mutex(nullptr) {

}

DirectoryCache::~DirectoryCache(){
  if(mutex){
    mutex->WriterLock();
    for(auto &known_resource : known_resources) {
      delete known_resource.second;
    }
    mutex->Unlock();
    delete mutex;
  }
}
void DirectoryCache::Init(const faodel::Configuration &config, string threading_model, string mutex_type) {
  kassert(mutex==nullptr, "Initialized more than once");
  ConfigureLogging(config);
  mutex = GenerateMutex(threading_model, mutex_type);

}
/**
 * @brief Add a new DirectoryInfo to the cache. Abort if item already exists
 * @param resource -  The new resource
 * @retval TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource already exists and therefore was NOT modified
 */
bool DirectoryCache::Create(const DirectoryInfo &resource){
  dbg("Create "+ resource.url.GetFullURL());
  return write(resource, false);
}
/**
 * @brief Add one or more  DirectoryInfos to the cache. Abort if item already exists
 * @param[in] resources The new resources to create
 * @param[out] num_created The number of resources that were created
 * @retval TRUE All resources were all unknown and were created ok
 * @retval FALSE One or more of the resources could not be created (already existed, or invalid)
 */
bool DirectoryCache::Create(const vector<DirectoryInfo> &resources, int *num_created){
  dbg("Create vector with "+to_string(resources.size())+" items");
  return write(resources, num_created, false);
}
/**
 * @brief Create an entry and all missing parents
 * @param[in] resource The new resource
 * @retval TRUE The resource and all parents were created ok
 * @retval FALSE The resource could not be created (already existed, or invalid)
 * @todo Parents inherit their bucket and ref node from the resource. Ref node should be set by user, instead
 */
bool DirectoryCache::CreateAndLinkParents(const DirectoryInfo &resource){
  bool ok;
  dbg("CreateAndLinkParents "+resource.url.GetFullURL());
  if(!resource.url.Valid()) return false;
  mutex->WriterLock();

  //try writing this entry
  ok=_write(resource,false);

  //Link in the parents
  if(ok){
    DirectoryInfo *r;
    bool found_existing_parent=false;
    ResourceURL child_url = resource.url;

    while( (!found_existing_parent) && (!child_url.IsRootLevel())){

      ResourceURL parent_url = child_url.GetParent();

      found_existing_parent = _lookup(parent_url, &r, nullptr);

      if(!found_existing_parent){
        //Didn't find parent, so create it and add to the map
        r = new DirectoryInfo(parent_url);
        known_resources[parent_url.GetBucketPathName()] = r;
      }
      //Either way, link to the child
      bool tmp_ok = r->Join(child_url.reference_node, child_url.name);
      kassert(tmp_ok, "Error creating parent in directory tree");

      child_url=parent_url; //Move up one level
    }
  }

  mutex->Unlock();
  return ok;
}
/**
 * @brief Remove an entry and all of its children from the cache (and update parent's children)
 * @param[in] dir_url The resource to remove
 * @retval TRUE Entry was found and removed ok
 * @retval FALSE Entry was not found in the cache
 */
bool DirectoryCache::Remove(const ResourceURL &dir_url){
  dbg("Remove "+ dir_url.GetFullURL());

  mutex->WriterLock();
  DirectoryInfo *r;

  //First, see if we even exist
  bool found = _lookup(dir_url, &r, nullptr);
  if(found){

    //Remove Parent link first if we're not root dir
    if(dir_url.path != "/"){
      DirectoryInfo *rp;
      faodel::ResourceURL parent_url = dir_url.GetParent();
      found = _lookup(parent_url, &rp, nullptr);
      if(found){
        rp->LeaveByName(dir_url.name);
      }
    }
    //Work through the tree
    vector<ResourceURL> remove_urls { dir_url };
    while(!remove_urls.empty()){
      ResourceURL url = remove_urls.back();
      remove_urls.pop_back();
      _removeSingleDir(url, &remove_urls);
    }
  }

  mutex->Unlock();
  return found;

}

/**
 * @brief Update a resource in this cache. If unknown, create it.
 * @param[in] resource  The resource to update
 * @return TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource wasn't valid
 */
bool DirectoryCache::Update(const DirectoryInfo &resource) {
  dbg("Update "+resource.url.GetFullURL());
  return write(resource, true);
}
/**
 * @brief Update a list of resources in this cache. If unknown, create them.
 * @param[in] resources  The list of resource to create
 * @param[out] num_created The number of resources that were updated
 * @return TRUE All of the resources were valid
 * @retval FALSE One or more of the resources were not valid
 */
bool DirectoryCache::Update(const vector<DirectoryInfo> &resources, int *num_created){
  dbg("Update vector with "+to_string(resources.size())+" items");
  return write(resources, num_created, true);
}

/**
 * @brief Add a new DirectoryInfo to the cache. Abort if item already exists.
 * @param[in] resource The new resource
 * @param[in] overwrite_existing Whether this has permission to overwrite an existing record
 * @retval TRUE The resource wasn't known and was created ok
 * @retval FALSE The resource already exists and was not modified
 */
bool DirectoryCache::write(const DirectoryInfo &resource, bool overwrite_existing){
  bool ok;
  dbg("Write resource "+resource.url.GetFullURL());
  if(!resource.url.Valid()) return false;
  mutex->WriterLock();
  ok = _write(resource, overwrite_existing);
  mutex->Unlock();
  return ok;
}

/**
 * @brief Add multiple Resources to the cache. Skip any that were already known
 * @param[in] resources List of resources to register
 * @param[out] num_created How many resources were added in this run
 * @param[in] overwrite_existing Whether this has permission to overwrite an existing record

 * @retval TRUE All resources were all unknown and were created ok
 * @retval FALSE One or more of the resources could not be created (already existed, or invalid)
 */
bool DirectoryCache::write(const vector<DirectoryInfo> &resources, int *num_created, bool overwrite_existing){
  size_t count=0;
  mutex->WriterLock();
  for(auto &ri : resources){
    if(ri.url.Valid()){
      bool ok = _write(ri, overwrite_existing);
      if(ok) count++;
    }
  }
  mutex->Unlock();
  if(num_created) *num_created = count;
  return (resources.size() == count);
}


bool DirectoryCache::_write(const DirectoryInfo &resource_info, bool overwrite_existing){
  DirectoryInfo *r;
  bool found=_lookup(resource_info.url, &r, nullptr);

  if(found){
    if(!overwrite_existing) return false; //don't touch (create only)
    delete r; //get rid old version
  }

  //Copy the resource and place it in
  r = new DirectoryInfo;
  *r = resource_info;
  known_resources[resource_info.url.GetBucketPathName()] = r;
  return true;
}

/**
 * @brief Let a node join an existing resource.
 * @param child_url -  The url for the child (parent found via GetLineage)
 * @param resource_info -  Copy of the updated DirectoryInfo if found, empty otherwise
 * @retval TRUE Found and updated the resource
 * @retval FALSE Did not find the resource here (or attempted to register root-level url). no changes made
 */
bool DirectoryCache::Join(const faodel::ResourceURL &child_url, DirectoryInfo *resource_info){

  dbg("Join resource "+child_url.GetURL());

  string option_autogen = child_url.GetOption(auto_generate_option_label);
  bool needs_autogen = (option_autogen=="1");

  faodel::ResourceURL parent;
  DirectoryInfo *r;
  bool found;

  //Identify the parent dir where the new info will go
  parent = (needs_autogen) ? child_url : child_url.GetParent();

  //Abort if we were given a named child and its at the root - nowhere to add
  if((!needs_autogen) &&child_url.IsRootLevel()) {
    dbg("Attempted join using a root url "+child_url.GetURL());
    if(resource_info) *resource_info = DirectoryInfo();
    return false;
  }

  mutex->WriterLock();
  found = _lookup(parent, &r, nullptr);
  if(found){
    string name = (needs_autogen) ? "" : child_url.name;
    found = r->Join(child_url.reference_node, name);
  }
  if(resource_info) *resource_info = (found) ? *r : DirectoryInfo();
  mutex->Unlock();
  return found;
}

/**
 * @brief Let a node leave an existing resource
 * @param child_url -  The url for the child (the parent is found internally by GetParent)
 * @param resource_info -  Copy of the updated DirectoryInfo if found, empty otherwise
 * @retval TRUE An item was removed
 * @retval FALSE Item or Entry was not found (no removal)
 */
bool DirectoryCache::Leave(const faodel::ResourceURL &child_url, DirectoryInfo *resource_info){

  faodel::ResourceURL parent = child_url.GetParent();
  DirectoryInfo *r;
  bool found;
  bool removed=false;

  dbg("Leave resource "+child_url.GetURL());

  //Abort if this was a root url. Nothing to join
  if(child_url.path=="/") {
    dbg("Attempted join using a root url "+child_url.GetURL());
    if(resource_info) *resource_info = DirectoryInfo();
    return false;
  }
  mutex->WriterLock();
  found = _lookup(parent, &r, nullptr);
  if(found){
    //Search for name first, then if not found try nodeid
    removed=r->Leave(child_url);
  }
  if(resource_info) *resource_info = (found) ? *r : DirectoryInfo();
  mutex->Unlock();
  return removed;
}

/**
 * @brief Determine if resource is in this cache and copy its contents back to user
 * @param search_url -  Reference to the desired resource
 * @param resource_info -  Copy of the resource info
 * @param reference_node -  Copy of the node responsible for holding the data
 * @retval TRUE Reference to resource was found in this cache
 * @retval FALSE Reference to resource was not found in this cache
 */
bool DirectoryCache::Lookup(const faodel::ResourceURL &search_url, DirectoryInfo *resource_info, faodel::nodeid_t *reference_node) {
  bool found;
  DirectoryInfo *r;

  dbg("Lookup "+search_url.GetFullURL());

  mutex->ReaderLock();
  found = _lookup(search_url, &r, reference_node);
  if(resource_info) *resource_info = (found) ? *r : DirectoryInfo(); //Not found: set to empty
  mutex->Unlock();
  return found;
}

/**
 * @brief Determine if a list of resources are known to this cache and retrieve their info.
 * @param[in] resource_urls Vector of resources to lookup
 * @param[in] resource_infos Vector of directory infos that were found in this cache
 * @retval TRUE All items were found in this cache
 * @retval FALSE One or more items were not found in this cache
 */
bool DirectoryCache::Lookup(const vector<faodel::ResourceURL> &resource_urls, vector<DirectoryInfo> *resource_infos){
  bool found;
  bool all_found=true;

  mutex->ReaderLock();
  for(const auto &resource_url : resource_urls) {
    DirectoryInfo *r;
    found = _lookup(resource_url, &r, nullptr);
    if(resource_infos){
      resource_infos->push_back( (found) ? *r : DirectoryInfo() );
    }
    all_found = all_found && found;
  }
  mutex->Unlock();

  return all_found;
}



/**
 * @brief Return a copy of all the entries that are known
 * @param urls The vector to append results to
 */
void DirectoryCache::GetAllURLs(vector<faodel::ResourceURL> *urls){
  if(!urls) return;
  mutex->ReaderLock();
  for(auto &known_resource : known_resources) {
    urls->push_back(known_resource.second->url);
  }
  mutex->Unlock();
}

/**
 * @brief Return a copy of all the entries that are known
 * @retval urls A vector of urls
 */
vector<faodel::ResourceURL> DirectoryCache::GetAllURLs(){
  vector<faodel::ResourceURL> urls;
  mutex->ReaderLock();
  for(auto &kv : known_resources){
    urls.push_back(kv.second->url);
  }
  mutex->Unlock();
  return urls;
}

/**
 * @brief Return a string list of known resources, in the form of "[bucket]/path/name"
 * @param names The vector to append the results to
 */
void DirectoryCache::GetAllNames(vector<string> *names) {
  mutex->ReaderLock();
  for(auto &kv : known_resources){
    names->push_back(kv.first);
  }
  mutex->Unlock();
}

void DirectoryCache::webhookInfo(faodel::ReplyStream &rs){

  rs.tableBegin("DirectoryCache "+GetComponentName());
  rs.tableTop({"Name","ReferenceNode","NumChildren","Info"});
  mutex->ReaderLock();
  for(auto &name_dirptr : known_resources){
    stringstream ss;
    ss<<"<a href=/dirman/entry&name="<<name_dirptr.first<<">"
      <<name_dirptr.first<<"</a>";
    rs.tableRow( {
          ss.str(),//name_dirptr.first,
          name_dirptr.second->GetReferenceNode().GetHtmlLink(),
          to_string(name_dirptr.second->children.size()),
          name_dirptr.second->info });
  }
  mutex->Unlock();
  rs.tableEnd();
}

void DirectoryCache::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ')<<"["<<GetFullName()<<"] Items: "<<known_resources.size()
     <<" Debug: "<<GetDebug()<<endl;
  if(depth>0) {
    //for(map<string,DirectoryInfo *>::const_iterator it = known_resources.begin(); it!=known_resources.end(); it++){
    // ss << string(indent+2,' ')
    //     << it->first <<" "
    //     << it->second->url.resource_type << " "
    //     << hex <<it->second->url.reference_node << dec<<" "
    //     << it->second->url.options<<endl;
    int i=0;
    for(auto &name_riptr : known_resources){
      // ss << string(indent+2,' ')
      //  <<"["<<i<<"] "
      //  << name_riptr.second->url.GetFullURL() <<endl;

      //if(depth>0){
      ss<< string(indent+2,' ') << "["<<i<<"] ";
        name_riptr.second->sstr(ss, depth-1, indent+6);
        //}
      i++;
    }
  }
}

/**
 * @brief Internal lookup of a resource (requires lock)
 * @param url -  The resource to locate
 * @param resource_info -  Internal pointer to the entry
 * @param reference_node -  The node responsible for this resource
 * @retval TRUE Resource was found
 * @retval FALSE Resource not found
 */
bool DirectoryCache::_lookup(const faodel::ResourceURL &url, DirectoryInfo **resource_info, nodeid_t *reference_node){

  kassert(resource_info!=nullptr, "Invalid resource info");
  kassert(url.Valid(), "Invalid url given to DC:"+url.GetFullURL());

  map<string,DirectoryInfo *>::iterator it;
  string bucket_path_name = url.GetBucketPathName();

  it=known_resources.find(bucket_path_name);
  if(it==known_resources.end()) {
    dbg("_lookup miss for "+bucket_path_name);
    //cout <<"  DC known items:\n";
    //for(auto &name_rip : known_resources){
    //  cout <<"    "<<name_rip.first <<" --> "<<name_rip.second->url.str();
    //}
    if(reference_node) *reference_node = NODE_UNSPECIFIED;
    return false;
  }
  dbg("_lookup hit for "+bucket_path_name+
      " Url is "+it->second->url.GetFullURL() +
      " Node is "+it->second->url.reference_node.GetHex());

  *resource_info = it->second;
  if(reference_node) *reference_node = it->second->url.reference_node;
  return true;
}

/**
 * @brief Remove a single directory and report who its children were
 * @param url -  The resource to remove
 * @param children -  A list of children that this node had
 * @retval TRUE Resource found and removed
 * @retval FALSE Resource wasn't found. Not removed.
 */
bool DirectoryCache::_removeSingleDir(const faodel::ResourceURL &url, std::vector<ResourceURL> *children){

  map<string,DirectoryInfo *>::iterator it;
  string bucket_path_name = url.GetBucketPathName();

  it=known_resources.find(bucket_path_name);
  if(it==known_resources.end()) return false;

  dbg("_removeSingleDir removing: "+bucket_path_name);

  DirectoryInfo *r = it->second;
  known_resources.erase(it);
  for( auto &name_node : r->children ){
    if((children!=nullptr) && (!name_node.name.empty())){
      dbg("_removeSingleDir marking for removal: "+bucket_path_name+"/"+name_node.name);
      children->push_back( ResourceURL(bucket_path_name+"/"+name_node.name));
    }
  }
  delete r;

  return true;
}




} // namespace dirman
