// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <unistd.h> // sleep

#include "common/Debug.hh"

#include "opbox/net/net.hh"
#include "opbox/services/dirman/core/DirManCoreDistributed.hh"

#include "webhook/Server.hh"

using namespace std;
using namespace faodel;

namespace opbox {
namespace dirman {
namespace internal {


DirManCoreDistributed::DirManCoreDistributed(const faodel::Configuration &config)
  : DirManCoreBase(config, "Distributed") {
  KTODO("DirManCoreDistributed not implemented yet");
}
DirManCoreDistributed::~DirManCoreDistributed(){
  KTODO("DMCD dtor");
}

void DirManCoreDistributed::start(){
  my_node = opbox::net::GetMyID();
  KTODO("DMCD start");
}

void DirManCoreDistributed::finish(){
  KTODO("DMCD finish");
}
bool DirManCoreDistributed::Locate(const ResourceURL &search_url, nodeid_t *reference_node) {
  KTODO("DMCD Locate");
}
bool DirManCoreDistributed::GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, DirectoryInfo *dir_info) {
  KTODO("DMCD GetDirectoryInfo")
}
bool DirManCoreDistributed::HostNewDir(const DirectoryInfo &dir_info) {
  KTODO("DMCD HostNewDir")
}
bool DirManCoreDistributed::JoinDirWithName(const faodel::ResourceURL &url, string name, DirectoryInfo *dir_info) {
  KTODO("DMCD JoinDirWithName")
}
bool DirManCoreDistributed::LeaveDir(const faodel::ResourceURL &url, DirectoryInfo *dir_info) {
  KTODO("DMCD LeaveDir")
}
bool DirManCoreDistributed::cacheForeignDir(const DirectoryInfo &dir_info) {
  KTODO("DMCD cacheForeignDir");
}


/**
 * @brief figure out which node host's the parent of a url (if unknown, walks upwards)
 * @param resource_url -  The URL for a resource we want to know the parent of
 * @param parent_node -  The parent's node
 * @return bool
 */
bool DirManCoreDistributed::discoverParent(const faodel::ResourceURL &resource_url, nodeid_t *parent_node){

  dbg("discover parent of "+resource_url.GetFullURL());

  if(resource_url.IsRootLevel()) return false;

  DirectoryInfo info;
  nodeid_t tmp_node;

  dbg("discoverParent looking for parent of "+resource_url.GetFullURL());

  faodel::ResourceURL parent_url = resource_url.GetLineageReference(1); //Figure out who our parent is


  //cout <<"XX about to lookup local on "<<parent_url.GetURL()<<endl;

  //See if we have local info about the parent on this node
  if(lookupLocal(parent_url, &info,  &tmp_node)){
    //Yes, we have a copy of the parent's info. See if that info
    //knows the child's location
    //cout <<"XX found "<<parent_url.GetURL()<<" in local lookup, node id is "<<hex<<tmp_node<<endl;

    nodeid_t child_node;
    if(info.GetChildReferenceNode(resource_url.name, &child_node)){
      //Yes, the copy of the parent knows about this node.
      if(parent_node) *parent_node = tmp_node;
      //cout <<"YY local has info on parent. Local already knows about the child. Thinks parent is "<<*parent_node<<endl;
      return true;
    }
    //cout <<"YY didn't know where the child is\n";

    //We know the parent and have a copy, but the copy doesn't know about the child. We
    //will want to refresh the data if we're walking the path.

    //Special case: if the parent lives on this node we don't need to refresh because
    //the data is the most recent available
    if((tmp_node==NODE_LOCALHOST) || (tmp_node==my_node)){
      dbg("discoverParent This node owns the parent, but child ("+resource_url.GetFullURL()+") was not known");
      //cout <<"Info: "<<info.str(4);
      //cout <<dbgStatus("rm")<<endl;
      if(parent_node) *parent_node = my_node; //Pass back our node
      return true;
    }
    //cout <<"ZZ parent node wasn't a local\n";
  }

  //If we don't know anything about the parent, we have to back up
  //a step and get more info about the parent.
  if(tmp_node==NODE_UNSPECIFIED){
    bool found = discoverParent(parent_url, &tmp_node);
    if(!found) return false;
  }


  //We know our parent, but we don't have it's info yet. See if we can fetch its info
  for(int i=0; i<10; i++){
    DirectoryInfo rinfo;
    if(lookupRemote(tmp_node, parent_url, &rinfo)){
      doc.Register(rinfo.url);
      if(parent_node) *parent_node = rinfo.url.reference_node;
      return true;
    }
    KDELAY(); //delay before retry
  }
  //Timeout.
  if(parent_node) *parent_node = NODE_UNSPECIFIED;
  return false;
}



/**
 * @brief Join a resource on a remote node
 * @param parent_node - The node where the parent resides
 * @param child_url - The resource to be registered
 * @param send_detailed_reply - Flag send detailed reply
 * @return bool
 */
bool DirManCoreDistributed::joinRemote(nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply){
  dbg("joinRemote Path: "+child_url.GetFullURL());
  error("joinRemote not implemented");
  KTODO("DMCD join Remote");

#if 0
  MessageResourceJoinRequest msg_req;
  msg_req.flags = (send_detailed_reply) ? msg_req.SEND_DETAILED_REPLY : 0;
  msg_req.new_urls.push_back(child_url);

  RPCRequest rpc_req;
  rpc_req.SetRequestMessage(&msg_req);
  rpc_req.SetNode(nc, parent_node);
  rc_t rc = rpc_req.InitAndIssue(KELPIE_RPC_RESOURCE_JOIN, NULL, 0, true);

  //Pull out the response
  MessageResourceJoinReply msg_reply;
  rpc_req.UnpackResultMessage<MessageResourceJoinReply>(msg_reply);

  //Cache all the items they gave us
  bool ok = dc_others.Update(msg_reply.resources);
  if(!ok) return false; //KELPIE_TODO;
  return true; //KELPIE_OK;
#endif
}

/**
 * @brief Send RPC to a particular node and retrieve info about a particular resource
 * @param nodeid - The node that hosts a resource
 * @param resource_url -  The resource we want to lookup
 * @param dir_info -  The resulting resource info
 * @return bool
 */
bool DirManCoreDistributed::lookupRemote(nodeid_t nodeid, const faodel::ResourceURL &resource_url, DirectoryInfo *dir_info){

  dbg("LookupRemote issue request to "+nodeid.GetHex()+" for url "+ resource_url.GetFullURL());
  if((nodeid == NODE_LOCALHOST) || (nodeid == my_node)){
    //Shouldn't be trying to look ourselves up?
    kassert(!strict_checking, "[dirman] LookupRemote issued for localhost or my node id.");
    return false;
  }
  error("LookupRemote not implemented");
  KTODO("DMCD lookupRemote");
  
#if 0
  MessageResourceLookupRequest msg_req;
  msg_req.flags = msg_req.SEND_DETAILED_REPLY;
  msg_req.urls.push_back(resource_url);

  RPCRequest rpc_req;
  rpc_req.SetRequestMessage(&msg_req);
  rpc_req.SetNode(nc, nodeid);
  rc_t rc = rpc_req.InitAndIssue(KELPIE_RPC_RESOURCE_LOOKUP, NULL, 0, true);

  //Pull out the response
  MessageResourceLookupReply msg_reply;
  rpc_req.UnpackResultMessage<MessageResourceLookupReply>(msg_reply);

  //Cache all the items they gave us
  bool ok = dc_others.Update(msg_reply.resources);

  if(!ok) return false; //KELPIE_TODO;
  if(msg_reply.num_found!=1)  return false; //KELPIE_ENOENT;
  if(dir_info) *dir_info = msg_reply.resources[0];
#endif
  return true; //KELPIE_OK;
}


void DirManCoreDistributed::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[DirManDistributed] "
     << endl;
  dc_others.sstr(ss,depth-1, indent+2);
  dc_mine.sstr(ss,depth-1, indent+2);
  doc.sstr(ss,depth-1,indent+2);
}

} // namespace internal
} // namespace opbox
} // namespace opbox

