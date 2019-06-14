// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "faodel-common/StringHelpers.hh"

#include "kelpie/ioms/IomPosixIndividualObjects.hh"


using namespace std;

namespace kelpie {
namespace internal {

  constexpr char IomPosixIndividualObjects::type_str[];
  
IomPosixIndividualObjects::IomPosixIndividualObjects(std::string name, const map<string,string> &new_settings)
  : IomBase(name, new_settings, {"path"}) {

  //Set debug info
  SetSubcomponentName("-pio-"+name);

  auto ii = settings.find("path");
  if(ii == settings.end()) {
    throw std::runtime_error("Iom "+name+" lacked a setting for 'path'");
  }
  path = ii->second;
  if(path.back()!='/') path=path+"/";
  
  //TODO Replace this C nastiness with c++17 stuff, or throw in a lib
  //TODO Add a directory traversal in case iom is in writable space

  stringstream ss;
  struct stat sb;
  for(int retries=0; retries<3; retries++) {

    //See if the directory already exists
    if (stat(path.c_str(), &sb) == 0) {

      //If this isn't a directory for us,
      if (!S_ISDIR(sb.st_mode)) {
        ss << "IOM PosixIndividualObjects Failed. Path '" << path << "' exists but is not a directory";
        throw std::runtime_error(ss.str());
      }

      //Directory exists, but can user write to it?
      if (access(path.c_str(), W_OK) != 0) {
        ss << "IOM PosixIndividualObjects Failed. User cannot access path '" << path << "'";
        throw std::runtime_error(ss.str());
      }
      //All ok
      return;
    }

    //Path didn't exist. Try creating the directory (note: not a recursive thing)
    int rc=mkdir(path.c_str(), S_IRWXU |S_IRWXG );
    if(rc == 0) return;

    //Bad news: we failed to create the directory. Try again, in case race condition
    sleep(1);
  }
  
  //We tried and failed to create the directory a few times. Bail out
  ss << "IOM PosixIndividualObjects failed. User cannot create directory '"<<path<<"'";
  throw std::runtime_error(ss.str());
}

rc_t IomPosixIndividualObjects::GetInfo(faodel::bucket_t bucket, const Key &key, kv_col_info_t *col_info) {
  rc_t rc;
  dbg("GetInfo for "+key.str());
  if(col_info)
    col_info->Wipe();

  string fname = genBucketPathFile(bucket, key);
  struct stat sb;
  if((stat(fname.c_str(), &sb)==0) && (S_ISREG(sb.st_mode))){
    if(col_info){
      col_info->num_bytes = sb.st_size;
      col_info->availability = Availability::InDisk;
    }
    rc = KELPIE_OK;
  } else {
    if(col_info) {
      col_info->availability = Availability::Unavailable;
    }
    rc = KELPIE_ENOENT;
  }
  return rc;
}

void IomPosixIndividualObjects::WriteObject(faodel::bucket_t bucket, const Key &key, const lunasa::DataObject &ldo) {
  dbg("WriteObject "+key.str());
  string fname = genBucketPathFile(bucket, key);
  ldo.writeToFile(fname.c_str());
  stat_wr_requests++;
  stat_wr_bytes+=ldo.GetWireSize();
}


rc_t IomPosixIndividualObjects::ReadObject(faodel::bucket_t bucket, const Key &key, lunasa::DataObject *ldo) {

  dbg("ReadObject "+key.str());
  string fname = genBucketPathFile(bucket, key);
  struct stat sb;

  stat_rd_requests++;

  if((stat(fname.c_str(), &sb)==0) && (S_ISREG(sb.st_mode))) {
    if(ldo!=nullptr) {
      *ldo = lunasa::DataObject(0, sb.st_size, lunasa::DataObject::AllocatorType::eager);
      ldo->readFromFile(fname.c_str());
      stat_rd_bytes+=sb.st_size;
    }
    return KELPIE_OK;
  } else {
    return KELPIE_ENOENT;
  }
}


/**
 * @brief Generate a file path down to the bucket identifier (w/ trailing slash)
 *
 * @param[in] bucket The bucket for this data
 * @return string The full-path name down to the data, ending in a slash
 */
string IomPosixIndividualObjects::genBucketPath(faodel::bucket_t bucket) {
  string bucket_path = path+bucket.GetHex() +"/";

  //See if we can get the path
  for(int retries=0; retries<5; retries++) {
    if(access(bucket_path.c_str(), W_OK) == 0) {
      return bucket_path; //Path already here
    } else if(mkdir(bucket_path.c_str(), S_IRWXU | S_IRWXG) == 0) {
      return bucket_path; //We created path
    }
    //Either a race condition, or path actually owned by someone else
    sleep(1); //Delay some in case another rank is creating this directory
  }
  //Couldn't resolve. Bail out
  stringstream ss;
  ss <<"Could not write to '"<<bucket_path<<"'";
  throw std::runtime_error(ss.str());
}
string IomPosixIndividualObjects::genBucketPathFile(faodel::bucket_t bucket, const Key &key){
  return genBucketPath(bucket) + faodel::MakePunycode(key.pup());
}

vector<faodel::bucket_t> IomPosixIndividualObjects::getBucketNames(){
  vector<faodel::bucket_t> buckets;
  DIR *dp;
  struct dirent *ep;
  dp = opendir(path.c_str());
  if(dp!=NULL) {
    while((ep=readdir(dp)) != NULL) {
      string name(ep->d_name);
      if(! ((name==".") || (name==".."))) {
        buckets.push_back( faodel::bucket_t(string(ep->d_name)) );
      }
    }
    closedir(dp);
  }
  std::sort(buckets.begin(), buckets.end());
  return buckets;
}
vector<pair<string,string>> IomPosixIndividualObjects::getBucketContents(string bucket) {

  vector<pair<string,string>> files;
  string bucket_path = genBucketPath(faodel::bucket_t(bucket));

  DIR *dp;
  struct dirent *ep;
  dp = opendir(bucket_path.c_str());
  if(dp!=NULL) {
    while((ep=readdir(dp)) != NULL) {
      string name = string(ep->d_name);
      if(! ((name==".")||(name=="..")) ) {
        string pname = bucket_path+"/"+name;
        struct stat sb;
        if((stat(pname.c_str(), &sb)==0) && (S_ISREG(sb.st_mode))) {      
          Key key;
          key.pup(faodel::ExpandPunycode(name));

          files.push_back( { key.str(), std::to_string(sb.st_size) } );
        } else {
          files.push_back( {name, "NODATA?"} );
        }
      }
    }
    closedir(dp);
  } 
  std::sort(files.begin(), files.end());
  return files;
  
}
void IomPosixIndividualObjects::AppendWebInfo(faodel::ReplyStream rs, string reference_link, const map<string,string> &args) {

  vector<vector<string>> items=
    { {"Setting", "Value"},
      {"Name", name   },
      {"Type", Type() },
      {"Path", path },
      {"Write Requests", to_string(stat_wr_requests)},
      {"Read Requests", to_string(stat_rd_requests)},
      {"Write Bytes", to_string(stat_wr_bytes)},
      {"Read Bytes", to_string(stat_rd_bytes)}
    };
  rs.mkTable( items,   "Basic Information" );

  rs.tableBegin("Initial Configuration Parameters");
  rs.tableTop({"Setting","Value"});
  for(auto &name_val : settings) {
    rs.tableRow({name_val.first, name_val.second});
  }
  rs.tableEnd();
  

  auto myargs = args;
  if(myargs["details"]=="true") {

    string bucket = myargs["bucket"];

    if(bucket=="") {
      //We need to get the list of buckets
      auto buckets = getBucketNames();
      vector<string> links;
      for(auto &b : buckets) {
        links.push_back( "<a href=\""+reference_link+"&details=true&iom_name="+name+"&bucket="+b.GetHex()+"\">"+b.GetHex()+"</a>");
      }
      rs.mkList(links, "On-Disk Buckets");

      //If only one bucket, automatically dump its contents
      //if(buckets.size()==1) {
      //  bucket = buckets[0].GetHex();
      //}
    }
        
    //See if they want the contents of the bucket
    if(bucket!="") {
      auto files = getBucketContents(bucket);
      files.insert(files.begin(), {"Key","Size"});
      rs.mkTable(files, "Objects in Bucket "+bucket);   
    }   
  }  
}

void IomPosixIndividualObjects::sstr(stringstream &ss, int depth, int indent) const {
  ss << string(indent,' ') + "IomPosixIndividualObjects Path: "<<path<<endl;
}

} // namespace internal
} // namespace kelpie
