// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <algorithm>

#include "faodel-common/Debug.hh"

#include "dirman/common/DirectoryOwnerCache.hh"


using namespace std;
using namespace faodel;

namespace dirman {

DirectoryOwnerCache::DirectoryOwnerCache(const string &full_name)
        : LoggingInterface(full_name), mutex(nullptr) {
}

DirectoryOwnerCache::~DirectoryOwnerCache(){
  if(mutex){
    delete mutex;
  }
}
void DirectoryOwnerCache::Init(const faodel::Configuration &config, string threading_model, string mutex_type){
  kassert(mutex== nullptr, "Initialized more than once");
  ConfigureLogging(config);
  mutex = GenerateMutex(threading_model, mutex_type);
}

bool DirectoryOwnerCache::Register(const faodel::ResourceURL &resource_url){
  bool ok;
  dbg("Register URL "+resource_url.GetFullURL()+" Valid: "+to_string(resource_url.Valid()));
  if(!resource_url.Valid()) return false;
  mutex->WriterLock();
  ok = _Register(resource_url);
  mutex->Unlock();
  return ok;
}


bool DirectoryOwnerCache::Register(const std::vector<faodel::ResourceURL> &resource_urls){

  bool ok=true;

  dbg("Register URL "+to_string(resource_urls.size())+" URLs");

  //Only accept valid urls
  for(auto &url : resource_urls){
    if((!url.Valid()) || (url.reference_node == NODE_UNSPECIFIED)) return false;
  }

  mutex->WriterLock();
  for(auto &url: resource_urls){
    dbg("Register URL "+url.GetURL()+" Valid: "+to_string(url.Valid()));
    ok = ok && _Register(url);
  }
  mutex->Unlock();
  return ok;
}


bool DirectoryOwnerCache::_Register(const faodel::ResourceURL &resource_url){
  known_resource_owners[resource_url.GetBucketPathName()] = resource_url.reference_node;
  return true;
}
/**
 * @param[in] search_url The url of an item we're looking for
 * @param[out] reference_node The node that is responsible for that resource
 * @retval TRUE found in this cache
 * @retval FALSE Did not find in this cache
 */
bool DirectoryOwnerCache::Lookup(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node) {
  bool found;
  nodeid_t node;

  mutex->ReaderLock();
  found = _Lookup(search_url, &node);
  if(reference_node) *reference_node = node; //_Lookup sets node to UNSPECIFIED if not found
  mutex->Unlock();
  dbg("Lookup URL "+search_url.GetURL()+" found: "+to_string(found)+" node: "+node.GetHex());

  return found;
}
bool DirectoryOwnerCache::Lookup(const vector<faodel::ResourceURL> &search_urls, vector<faodel::nodeid_t> *reference_nodes){
  bool found;
  bool all_found=true;

  mutex->ReaderLock();
  for(auto &url : search_urls){
    nodeid_t node;
    found = _Lookup(url, &node);
    if(reference_nodes){
      reference_nodes->push_back(node); //_Lookup sets node to UNSPECIFIED if not found
    }
    all_found = all_found && found;
  }
  mutex->Unlock();
  dbg("Lookup "+to_string(search_urls.size())+" URLs, found_all: "+to_string(all_found));
  return all_found;
}

// bool DirectoryOwnerCache::FindLastKnownParent(const faodel::ResourceURL &search_url, nodeid_t *reference_node, int *parent_steps_up){

//   mutex->ReaderLock();
//   int i=-1;
//   string sparent;
//   bool found=false;
//   DirectoryInfo *r;
//   do {
//     i++;
//     sparent = search_url.GetParentBucketPath(i);
//     if(sparent!="")
//       found = _Lookup(sparent, &r);
//   } while((!found) && (sparent!=""));

//   if(reference_node)  *reference_node = (found) ? r->url.reference_node : UNSPECIFIED;
//   if(parent_steps_up) *parent_steps_up = (found) ? i : -1;

//   mutex->Unlock();
//   return found;
// }

void DirectoryOwnerCache::whookieInfo(faodel::ReplyStream &rs){

  rs.tableBegin("DirectoryOwnerCache");
  rs.tableTop({"Name","ReferenceNode"});
  mutex->ReaderLock();
  for(auto &name_owner : known_resource_owners){
    rs.tableRow( { name_owner.first,
                   name_owner.second.GetHtmlLink()});
  }
  mutex->Unlock();
  rs.tableEnd();
}


void DirectoryOwnerCache::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ')<<"["<<GetFullName()<<"] Items: "
     <<known_resource_owners.size()
     <<" Debug: "<<GetDebug()<<endl;
  if(depth>0){
    int i=0;
    for(auto &tag_id : known_resource_owners){
      ss << string(indent+2,' ')
         << "["<<i<<"] "
         << tag_id.first <<" "
         << tag_id.second.GetHex()
         << endl;
      i++;
    }
  }
}


bool DirectoryOwnerCache::_Lookup(const faodel::ResourceURL &url, nodeid_t *node_id){

  kassert(node_id!=nullptr, "Invalid node_id");
  kassert(url.Valid(), "Invalid url given to RIC:"+url.GetFullURL());

  string bucket_path_name = url.GetBucketPathName();

  auto it=known_resource_owners.find(bucket_path_name);
  if(it==known_resource_owners.end()){
    *node_id = NODE_UNSPECIFIED;
    return false;
  }

  *node_id = it->second;
  return true;
}


} // namespace dirman
