// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <algorithm>
#include <fstream> //for ifstream parsing of file
#include <unistd.h> //for sleep

#include "opbox/net/net.hh"
#include "dirman/core/DirManCoreBase.hh"

using namespace std;
using namespace faodel;


namespace dirman {
namespace internal {

/**
 * @brief Special nop constructor for use by Unconfigured
 * @param called_by_unconfigured
 * @return DirManCoreBase
 */
DirManCoreBase::DirManCoreBase(faodel::internal_use_only_t called_by_unconfigured)
  : LoggingInterface("dirman","Unconfigured"),
    dc_others("dirman.cache.others"),
    dc_mine("dirman.cache.mine"),
    doc("dirman.cache.owners"){
}

/**
 * @brief Do a one-time configure of the DirManCore before it is used.
 * @param[in] config The faodel::Configuration object, which stores the list of settings
 * @param[in] component_type Which type of DirManCore this is (Centralized, Distributed), for logging purposes
 */
DirManCoreBase::DirManCoreBase(const faodel::Configuration &config, const string &component_type)
  : LoggingInterface("dirman", component_type),
    dc_others("dirman.cache.others"),
    dc_mine("dirman.cache.mine"),
    doc("dirman.cache.owners"),
    my_node(NODE_UNSPECIFIED), 
    debug(false), 
    strict_checking(false)  {

  ConfigureLogging(config);

  //Get the threading model and mutex type, then init the DirectoryCache
  string threading_model, mutex_type , s;
  bool am_root;
  vector<string> predefined_resources;

  config.GetLowercaseString(&threading_model,  "threading_model",        "pthreads");
  config.GetBool(&debug,                       "dirman.debug",           "false");
  config.GetBool(&am_root,                     "dirman.host_root",       "false");
  config.GetBool(&strict_checking,             "dirman.strict",          "strict");
  config.GetLowercaseString(&threading_model,  "dirman.threading_model", threading_model);
  config.GetLowercaseString(&mutex_type,       "dirman.mutex_type",      "rwlock");
  config.GetStringVector(&predefined_resources,"dirman.resources");
  config.GetDefaultSecurityBucket(&default_bucket);

  //TODO: init these with one mutex
  dc_others.Init(config, threading_model, mutex_type);
  dc_mine.Init(config, threading_model, mutex_type);
  doc.Init(config, threading_model, mutex_type);

  string fname, ename;
  config.GetString(&ename, "dirman.root_node.file.env_name","");
  if(!ename.empty()){
    char *penv = getenv(ename.c_str());
    fname = string(penv);
  }
  config.GetString(&fname, "dirman.root_node.file",fname);


  //Load any references into our database
  if(!predefined_resources.empty()) {
    //A user can append several things in the url list. The assumption is
    //the last entry is the one to keep. Walk backwards through the list
    //and add to a map if the path/name doesn't already exist.
    dbg("predefined resource size is "+std::to_string(predefined_resources.size()));
    map<std::string, ResourceURL> urls;
    for(int i=predefined_resources.size()-1; i>=0; i--){
      dbg("Considering "+predefined_resources[i]);
      ResourceURL url(predefined_resources[i]);
      if(url.resource_type=="ref") continue; //Never add pure references
      if(url.bucket == BUCKET_UNSPECIFIED) url.bucket = default_bucket;
      string path_name = url.GetPathName();
      if( urls.find(path_name) == urls.end() ){
        urls[path_name] = url;
      }
    }
    //Now throw all the entries into the other cache
    for(auto &key_url : urls) {
      DirectoryInfo di(key_url.second);


      //Any non-local resource defined here that doesn't have nodes in it
      //should be removed because it doesn't provide any actionable info.
      //If you don't do this, non-root nodes get stale info at init don't
      //bother to update from root.
      // Example: if we use mpisyncstart to create a dht, the root node
      //          gets a url with all the children, but the non-root nodes
      //          get zero children because mpisyncstart doesn't globally
      //          sync everything.
      if((di.url.resource_type!="local") && (di.children.size()==0)) {
        dbg("Not adding predefined resource "+key_url.first+" because it is not local and does not have any members");
        throw runtime_error("Abort on adding "+key_url.first);
        continue;
      }
      dbg("adding predefined resource "+key_url.first+" --> "+di.url.GetFullURL()+" Num Children="+std::to_string(di.children.size()));
      dc_others.Create(di); //Note: this does not link parents

    }
  }


#if 0

  vector<string> surls_to_host; //urls encoded in config that need hosting
  vector<ResourceURL> urls_to_host;
  vector<ResourceURL> urls_to_check;


  //See if we're responsible for hosting a resource off the bat.
  string hosting_options[] = { "dirman.hosting", "hosting"};
  for(auto tag : hosting_options){
    config.GetLowercaseString(&s,tag,"");
    if(s != ""){
      vector<string> new_paths = Split(s, ';', true); //Multiple paths can be store on a single line if separated by ;
      for(auto new_path : new_paths){
        surls_to_host.push_back(new_path);
        faodel::ResourceURL new_url(new_path);
        new_url.reference_node = my_node;
        if(new_url.bucket==BUCKET_UNSPECIFIED) new_url.bucket = default_bucket;
        if(new_url.resource_type == "") new_url.resource_type = "ref";
        urls_to_host.push_back(new_url);
        dbg("This node now hosts: "+s+" which has url "+new_path);
      }
    }
  }

  //Write to File: Sometimes when debugging its easier to just write our data to a file
  config.GetLowercaseString(&s,"dirman.write_to_file");
  if(s != ""){
    writeURLsToFileOrDie(s, urls_to_host);
  }


  //Find any url files our config file says we should parse. Block until each is available
  string file_vars[] = { "dirman.read_from_file"};
  for(auto config_tag : file_vars){
    config.GetLowercaseString(&s, config_tag);
    if(s != ""){
      vector<string> urls_from_file = readURLsFromFilesWithRetry(s);
      urls_to_check.insert(urls_to_check.end(), urls_from_file.begin(), urls_from_file.end());
    }
  }

  //See if we've been handed one or more urls through configuration
  config.GetLowercaseString(&s, "dirman.use_url");
  if(s != ""){
    vector<string> urls_from_conf;
    Split(urls_from_conf, s, ';', true); //Multiple paths can be store on a single line if separated by ;
    urls_to_check.insert(urls_to_check.end(), urls_from_conf.begin(), urls_from_conf.end());
  }

  cout<<"About to do next\n";

  //Walk through all reference urls and plug them into resource owner cache. This
  //just lets us have basic info available if we need to look it up, and does
  //not trigger any network operations
  for(auto url_string : urls_to_check){
    //See if we already know this as something we host
    faodel::ResourceURL new_url = faodel::ResourceURL(url_string);
    if(!new_url.Valid()){
      KWARN("Trying to register bad resource url? "+url_string.GetFullURL());
      continue;
    }
    doc.Register(new_url);
  }


  //First, we have to organize urls by path depth so we can complete
  vector<pair<int,string> > hosted_urls_by_depth;
  for(auto surl : surls_to_host){
    size_t n = count(surl.begin(), surl.end(), '/');
    hosted_urls_by_depth.push_back(pair<int,string>(n,surl));
  }
  sort(hosted_urls_by_depth.begin(), hosted_urls_by_depth.end());

  //Second, we register the actual resources
  for(auto depth_pair : hosted_urls_by_depth){
    HostNewDir(faodel::ResourceURL(depth_pair.second));
  }
#endif

}
DirManCoreBase::~DirManCoreBase() = default;

/**
 * @brief Create a new local resource and update parent
 * @param url -  The bucket, path, and name of the resource
 * @retval TRUE This was added and is a new item
 * @retval FALSE This was not added because the url wasn't valid
 */
bool DirManCoreBase::HostNewDir(const faodel::ResourceURL &url){

  faodel::ResourceURL tmp_url = url;

  //Plug in our default bucket and node id
  if(tmp_url.bucket==BUCKET_UNSPECIFIED) {
    kassert(!strict_checking, "HostNewDir given a url with a null bucket");
    tmp_url.bucket=default_bucket;
  }

  tmp_url.reference_node = my_node;

  //Plug in a default reference in none provided
  if(tmp_url.resource_type.empty()){
    kassert(!strict_checking, "HostNewDir url did not specify resource_type");
    tmp_url.resource_type="ref";
  }

  return HostNewDir(DirectoryInfo(tmp_url));
}


bool DirManCoreBase::JoinDirWithoutName(const faodel::ResourceURL &url, DirectoryInfo *dir_info) {

  ResourceURL url_mod = url;
  url_mod.SetOption(DirectoryCache::auto_generate_option_label, "1");
  return JoinDirWithName(url_mod, "", dir_info);

}

/**
 * @brief Get a list of all the named resources that this node currently knows about
 * @param[out] resource_names A vector to append with all the node's known resources
 */
void DirManCoreBase::GetCachedNames(std::vector<std::string> *resource_names) {
  dc_others.GetAllNames(resource_names);
  dc_mine.GetAllNames(resource_names);
}


/**
 * @brief Query local resources to see if info exists about a particular resource
 * @param search_url -  search url
 * @param dir_info -  Where to copy the resource info if known
 * @param reference_node -  The node responsible for this data, or UNSPECIFIED if no info known
 * @retval TRUE DirectoryInfo was known and returned
 * @retval FALSE DirectoryInfo was not known. Set will be set to the owner for the resource if known, UNSPECIFIED otherwise
 */
bool DirManCoreBase::lookupLocal(const faodel::ResourceURL &search_url, DirectoryInfo *dir_info, faodel::nodeid_t *reference_node){

  if( (  dc_mine.Lookup(search_url, dir_info, reference_node)) ||
      (dc_others.Lookup(search_url, dir_info, reference_node))    ){
    return true;
  }
  //Didn't find, but we might still know node that is responsible
  doc.Lookup(search_url, reference_node);
  return false;
}

/**
 * @brief Query local resources to see if info exists about a resource
 * @param bucket_path -  A string with just "<bucket>/path" for the resouce
 * @param dir_info -  Where to copy the resource info if known
 * @param reference_node -  The node responsible for this data, or UNSPECIFIED if no info known
 * @retval TRUE DirectoryInfo was known and returned
 * @retval FALSE DirectoryInfo was not known. Set will be set to the owner for the resource if known, UNSPECIFIED otherwise
 */
bool DirManCoreBase::lookupLocal(const string &bucket_path, DirectoryInfo *dir_info, faodel::nodeid_t *reference_node){
  return lookupLocal(faodel::ResourceURL(bucket_path), dir_info, reference_node);
}

/**
 * @brief Look through a list of urls and retrieve any resourceInfo's that are known
 * @param search_url -  Search list
 * @param dir_info -  Results
 * @retval TRUE All items were found
 * @retval FALSE One or more items were not found
 */
bool DirManCoreBase::lookupLocal(const vector<faodel::ResourceURL> &search_url,  vector<DirectoryInfo> *dir_info){
  bool all_found=true;
  for(const auto &url : search_url){
    DirectoryInfo ri;
    bool ok = lookupLocal( url, &ri);
    if(ok){
      if(dir_info) dir_info->push_back(ri);
    } else {
      all_found=false;
    }
  }
  return all_found;
}

#if 0
//REMOVING: derived classes all have their own management quirks
bool DirManCoreBase::joinLocal(const faodel::ResourceURL &child_url, DirectoryInfo *parent_dir_info) {
  if(child_url.IsRootLevel()) return false;
  return dc_mine.Join(child_url, parent_dir_info);
}
#endif



/**
 * @brief Reads in urls from one or more files
 * @param file_names - list of files to read, separated by semicolon
 * @retval vector<string> - List of plain text urls
 */
vector<string> DirManCoreBase::readURLsFromFilesWithRetry(string file_names){

  vector<string> out_urls;
  vector<string> files = Split(file_names, ';', true);
  for(const auto &file_name : files){
    //dbg("reading urls from file "+file_name);
    int sleep_time=1;
    for(bool completed=false; !completed; ){
      ifstream f;
      f.open(file_name);
      if(!f.is_open()){
        dbg("could not open file "+file_name+".. Retry in "+to_string(sleep_time)+" seconds");
        sleep(sleep_time);
        if(sleep_time<16) sleep_time*=2;
      } else {
        string url;
        while(f >> url){
          out_urls.push_back(url);
        }
        f.close();
        completed=true;
      }
    }
  }
  return out_urls;
}

/**
 * @brief Write out a list of URLs to a file, or die trying
 * @param[in] file_name Name of the file name to be written
 * @param[in] urls List of URLs
 * @retval TRUE Completed ok
 * @retval FALSE Had problems (and KFAIL wasn't configured to halt)
 */
bool DirManCoreBase::writeURLsToFileOrDie(const string file_name, const vector<faodel::ResourceURL> &urls){

  ofstream f;
  f.open(file_name);
  if(!f.is_open()){
    //TODO: Better error handling here?
    error("Could not write url to file "+file_name);
    KFAIL("Could not open output file");
    return false;
  }
  for(const auto &url : urls){
    f << url.GetFullURL() << endl;
  }
  f.close();
  return true;

}

bool DirManCoreBase::cacheForeignDir(const DirectoryInfo &dir_info){

  dbg("cacheForeignDir "+dir_info.url.GetFullURL());
  if((!dir_info.url.Valid()) ||
     (dir_info.url.reference_node == NODE_LOCALHOST) ||
     (dir_info.url.reference_node == my_node)     ){
    error("cacheForeignDir asked to cache invalid or local resource: "+dir_info.url.GetFullURL());
    return false;
  }

  doc.Register(dir_info.url);
  bool new_resource = dc_others.Create(dir_info);
  if(!new_resource){
    dbg("Attempted to cache resource that's already registered");
  }
  return true;
}
bool DirManCoreBase::Locate(const faodel::ResourceURL &search_url, nodeid_t *reference_node) {

  bool ok = lookupLocal(search_url, nullptr, reference_node);
  KWARN("Locate should be global, not just local");
  return ok;

}

#if 0
//REMOVING: This used to be an example of how to join, but derived classes all do their own thing
bool DirManCoreBase::JoinDir(const faodel::ResourceURL &my_url, DirectoryInfo *dir_info){

  //Can't join something if we're the root!
  if(my_url.IsRootLevel()) return false;

  nodeid_t parent_node;
  bool ok = discoverParent(my_url, &parent_node);
  kassert(ok,"couldn't discover parent for "+my_url.str());

  //If Local, no need to do rpc
  if((parent_node == my_node)||(parent_node==NODE_LOCALHOST)){
    return joinLocal(my_url, dir_info);
  }
  KWARN("Join resource should be global, not just local");
  return joinRemote(parent_node, my_url); //todo
}
#endif


/**
 * @brief Create a new local resource and update parent
 * @param dir_info -  All of the info needed to describe this resource
 * @retval TRUE This was added and is a new item
 * @retval FALSE This was not added because the url wasn't valid
 */
bool DirManCoreBase::HostNewDir(const DirectoryInfo &dir_info){

  dbg("HostNewDir "+dir_info.url.GetFullURL());

  //Can't host it if it isn't valid
  if(!dir_info.url.Valid()){
    error("Attempted to host new resource with invalid url "+dir_info.url.GetFullURL());
    return false;
  }

  //Make sure url points to us
  if(dir_info.url.reference_node != my_node){
    error("Attempted to host resource that didn't have our node's id. Had "+
             dir_info.url.reference_node.GetHex() +
             " instead of "+my_node.GetHex());
    return false;
  }

  //Create the actual resource (if it doesn't exist) and update doc
  bool new_resource = dc_mine.Create(dir_info);
  if(!new_resource) {
    dbg("Attempted to create resource that's already registered?");
    return false; //It's already been registered
  }
  doc.Register(dir_info.url); //Just to make sure its known locally

  //Bail out here if this is a root: no parent to notify
  if(dir_info.url.IsRootLevel()) return true;


  //See if our parent is hosted here. Is so, join locally.
  nodeid_t parent_node;
  bool ok = discoverParent(dir_info.url, &parent_node);
  dbg("hostresource discovered ok="+to_string(ok)+" parent was "+parent_node.GetHex());

  kassert(ok,"couldn't discover parent for "+dir_info.url.GetFullURL());
  if((parent_node == my_node)||(parent_node==NODE_LOCALHOST)){
    dbg("hosted resource's parent available here. Joining.");
    return dc_mine.Join(dir_info.url);
  }

  //Not local, we must join a resource
  dbg("hosted resource's parent not available here. remote joining.");
  ok = joinRemote(parent_node, dir_info.url);
  kassert(ok,"Couldn't host resource, because couldn't join parent?");

  return true;
}


bool DirManCoreBase::GetDirectoryInfo(const faodel::ResourceURL &search_url, bool check_local, bool check_remote, DirectoryInfo *dir_info){

  if(check_local){
    bool ok = lookupLocal(search_url, dir_info, nullptr);
    if(ok || !check_remote) return ok;
  }
  if(check_remote){
    KWARN("GetDirectoryInfo should be global, not just local");
    return false;
  }
  return false;
}



void DirManCoreBase::HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results){

  faodel::ReplyStream rs(args, "Directory Manager", &results);
  rs.tableBegin("Directory Manager");
  rs.tableTop({"Parameter","Setting"});
  rs.tableRow({"Type:", GetType()});
  rs.tableRow({"Default Bucket:", default_bucket.GetHex()});
  appendWebhookParameterTable(&rs);
  rs.tableEnd();

  dc_mine.webhookInfo(rs);
  dc_others.webhookInfo(rs);
  doc.webhookInfo(rs);

  rs.Finish();
}
void DirManCoreBase::HandleWebhookEntry(const std::map<std::string,std::string> &args, std::stringstream &results){
  faodel::ReplyStream rs(args, "Directory Manager", &results);
  auto it = args.find("name");
  if(it==args.end()){
    //Error
  } else {
    string s = it->second;
    DirectoryInfo dir_info;
    bool found = lookupLocal(s, &dir_info);
    if(found){
      dir_info.webhookInfo(rs);
    }
  }
  rs.Finish();
}

/**
 * @brief Generate any derived-class info to put into parameter list for DirMan webhook
 * @param rs The reply stream to update. This is already in the middle of a table
 */
void DirManCoreBase::appendWebhookParameterTable(faodel::ReplyStream *rs){
  // ex.. rs->tableRow({"My Param", "My Value"});
}

void DirManCoreBase::sstr(std::stringstream &ss, int depth, int indent) const {
  ss<<string(indent,' ') <<"[DirMan] MyNode: "<<my_node.GetHex()<<" DefBucket: "<<default_bucket.GetHex()<<" Debug: "<<debug<<endl;
  if(depth>0){
    dc_mine.sstr(ss, depth-1, indent+2);
    dc_others.sstr(ss, depth-1, indent+2);
    doc.sstr(ss, depth-1, indent+2);
  }
}


} // namespace internal
} // namespace dirman

