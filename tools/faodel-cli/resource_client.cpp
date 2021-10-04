#include <iomanip>
#include "faodel-common/StringHelpers.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

int resourceList(const vector<string> &args);
int resourceListRecursive(const vector<string> &args);
int resourceDefine(const vector<string> &args);
int resourceDrop(const vector<string> &args);
int resourceKill(const vector<string> &args);


bool dumpHelpResource(string subcommand) {

  string help_rlist[5] = {
          "resource-list", "rlist", "<urls>", "Retrieve list of known resource names",
          R"(
Connect to dirman and get current directory info for one or more resources.

Example:

  faodel rlist /my/resource1 /my/resource2
)"
  };

  string help_rdef[5] = {
          "resource-define","rdef", "<urls>", "Define new resource",
          R"(
This command connects to dirman and instructs it to define the resources
specified by urls. Defining a resource is the first step in creating a
resource, and should be thought of as a way to specify parameters for a
resource as opposed to the actual nodes that are part of the resource. A URL
should include the type, path, name, and paramters for the resource (eg
minimum number of nodes or iom names).

Example:

  faodel rdef "dht:/my/dht1&min_members=4"
  faodel rdef "dht:/my/dht2&min_members=3&behavior=defaultlocaliom&iom=io1

Behaviors let you control how values are cached at different stages in the
pipeline. You can supply a list of '_' separated values together in the url.
current behaviors are:

 Individual level controls:
  writetolocal, writetoremote, writetoiom : publish goes to local/remote/iom
  readtolocal,  readtoremote              : want/need cached at local/remote

 Common aggregations
  writearound : publishes only to the iom (no caching)
  writeall    : publishes to all layers
  readtonone  : don't cache at local or remote node

  defaultiom        : writetoiom_readtonone
  defaultlocaliom   : writetoiom_readtonone
  defaultremoteiom  : writetoiom_readtoremote
  defaultcachingiom : writetoall_readtolocal_readtoremote

)"
  };

  string help_rdrop[5] = {
          "resource-drop", "rdrop", "<urls>", "Remove references to resources in dirman",
          R"(
This command instructs dirman to remove references to resources specified by
one or more urls. This command ONLY removes references on the dirman server
and does NOT invalidate the info in existing clients. Nodes that are part of a
resource will continue to run.

Example:

  faodel rdrop /my/dht1
)"
  };

#if 0
  string help_rkill[5] = {
          "resource-kill", "rkill", "<urls>", "Shutdown resources (drop+kill)",
          R"(
NOT IMPLEMENTED YET: This feature is not yet implemented

This command instructs dirman to remove references to one or more resources
specified by urls (similar to rdrop). It will also connect to the nodes in the
resource and instruct them to shutdown.

Example:

  faodel rkill /my/dht1
)"
  };
#endif


  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_rlist);
  found |= dumpSpecificHelp(subcommand, help_rdef);
  found |= dumpSpecificHelp(subcommand, help_rdrop);
  //found |= dumpSpecificHelp(subcommand, help_rkill);
  return found;
}

int checkResourceCommands(const std::string &cmd, const vector<string> &args) {

  if(     (cmd == "resource-list")   || (cmd == "rlist"))  return resourceList(args);
  else if((cmd == "resource-listr")  || (cmd == "rlistr")) return resourceListRecursive(args);
  else if((cmd == "resource-define") || (cmd == "rdef"))   return resourceDefine(args);
  else if((cmd == "resource-drop")   || (cmd == "rdrop"))  return resourceDrop(args);
  //else if((cmd == "resource-kill")   || (cmd == "rkill"))  return resourceKill(args);

  //No command
  return ENOENT;
}


void resourceInit(faodel::Configuration *config) {

  string dirman_type;
  config->GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") {
    config->Append("dirman.type", "centralized");
  }

  modifyConfigLogging(config, {"dirman"}, {"dirman.cache.mine", "dirman.cache.others"});

  faodel::bootstrap::Start(*config, dirman::bootstrap);

}

void resourceFinish() {
  if(faodel::bootstrap::IsStarted()) {
    faodel::bootstrap::Finish();
  }
}


int resourceList(const vector <string> &args) {

  auto paths = args;
  if(paths.size()==0) paths.push_back("/");

  faodel::Configuration config;
  resourceInit(&config);

  int rc=0;
  bool ok;
  string dir_path;

  for(auto p : paths) {
    DirectoryInfo dir;
    ResourceURL url;
    try {

      url = ResourceURL(p);
      ok = dirman::GetDirectoryInfo(url, &dir);
      if(ok) {
        cout<<"Located: "<<p<<endl
            <<"     Full URL: "<<dir.url.GetFullURL()<<endl
            <<"      RefNode: "<<dir.url.reference_node.GetHex()<<" "<<dir.url.reference_node.GetHttpLink()<<endl
            <<"         Info: "<<dir.info<<endl;

        //Check Behavior
        string behaviors=dir.url.GetOption("behavior");
        if(!behaviors.empty()) {
          auto b = kelpie::PoolBehavior::ParseString(behaviors);
          cout<<"     Behavior: "<< kelpie::PoolBehavior::GetString(b) <<"\n";
        }

        //Check attached iom
        string iom_name=dir.url.GetOption("iom");
        if(!iom_name.empty()) {
          cout<<"          IOM: "<<iom_name<<endl;
        }

        //List the members
        cout<<"  Min Members: "<<dir.min_members<<endl
            <<"      Members: "<<dir.members.size()<<endl;
        for(size_t i=0; i<dir.members.size(); i++) {
            cout<<"      "<<dir.members[i].name <<"  "<<dir.members[i].node.GetHex()<<" "<<dir.members[i].node.GetHttpLink()<<endl;
        }
      } else {
        warn("Missing: '"+p+"'");
        //rc=-1; //Missing isn't necessarily a failure
      }
    } catch(const std::exception &e) {
      //cout << "Caught std exception\n" << e.what() << endl;
      warn("Could not parse '"+p+"'");
      rc=-1;
    }
  }
  resourceFinish();
  return rc;

}

int resourceListRecursive(const vector <string> &args) {

  auto paths = args;
  if(paths.size()==0) paths.push_back("/"); //Show default root

  faodel::Configuration config;
  resourceInit(&config);

  int rc=0;
  bool ok;
  string dir_path;

  map<string,string> results;
  size_t max_slen=0;

  while(!paths.empty()) {

    //Pop front
    string p=paths[0];
    paths.erase(paths.begin());

    //Skip any we've already looked up
    if(results.find(p) != results.end()) continue;

    DirectoryInfo dir;
    ResourceURL url;

    try {
      url = ResourceURL(p);
      dir = DirectoryInfo();
      ok = dirman::GetDirectoryInfo(url, &dir);
      if(ok) {

        //Store the answer
        results[p] = dir.url.GetFullURL();

        //Generate all the kids
        string base = dir.url.GetBucketPathName();
        if(dir.url.IsRoot()) base.pop_back(); //So we can just add "/child"

        for(auto child : dir.members) {
          string target = base+"/"+child.name;
          if(target.size()>max_slen) max_slen = target.size();
          paths.push_back(target);
        }

      } else {
        warn("Missing: '"+p+"'");
        //rc=-1; //Missing isn't necessrily a failure.
      }
    } catch(const std::exception &e) {
      warn("Could not parse '"+p+"'");
      rc=-1;
    }
  }

  //Dump out all results
  for(auto &path : results)
    cout << setw(max_slen) << std::left << path.first<<" : " << path.second<<endl;


  resourceFinish();
  return rc;

}

int resourceDefine(const vector <string> &args) {
  int rc=0;
  bool ok;
  if(args.empty()){
    cout <<"No resources provided. Done.\n";
    return 0;
  }

  faodel::Configuration config;
  resourceInit(&config);

  for(auto r : args) {

    ResourceURL url;
    try {
      url = ResourceURL(r);

      //Sanity check
      string behaviors=url.GetOption("behavior");
      if(!behaviors.empty()) {
        kelpie::PoolBehavior::ParseString(behaviors);
      }

      //Issue the definition
      ok = dirman::DefineNewDir(url);
      cout <<"Resource '"<<url.GetFullURL()<< ((ok)?"' Created ok" :"' Could not be created.")<<endl;
      if(!ok) rc=-1;

    } catch(const std::exception &e) {
      warn("Resource '"+r+"' was not a valud url. "+e.what());
      rc=-1;
    }
  }

  resourceFinish();
  return rc;
}

int resourceDrop(const vector <string> &args) {

  int rc=0;
  bool ok;
  faodel::Configuration config;
  resourceInit(&config);

  for(auto r : args) {

    ResourceURL url;
    try {
      url = ResourceURL(r);
      ok = dirman::DropDir(url);
      info("Drop issued for: '"+url.GetFullURL()+"'");
      if(!ok) rc=-1;

    } catch(const std::exception &e) {
      warn("Resource '"+r+"' was not a valud url");
      rc=-1;
    }
  }


  resourceFinish();
  return rc;
}

//Note: Original plan was to do a drop and then send kills to pool members
//      This is a combination of kstop and rdrop now.
int resourceKill(const vector <string> &args) {
  int rc=0;
  KTODO("resourceKill");
  return rc;
}
