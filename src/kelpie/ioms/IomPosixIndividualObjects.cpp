// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include <sys/stat.h>
#include <unistd.h>
#include <string.h> //memset
#include <dirent.h>

#include "common/StringHelpers.hh"

#include "kelpie/ioms/IomPosixIndividualObjects.hh"


using namespace std;

namespace kelpie {
namespace internal {


IomPosixIndividualObjects::IomPosixIndividualObjects(std::string name, const map<string,string> &new_settings)
  : IomBase(name) {

  //Only keep settings that are valid
  for(auto s : { "path" }) {
    auto name_setting = new_settings.find(s);
    settings[s] = (name_setting == new_settings.end()) ? "" : name_setting->second;
  }


  
  auto ii = settings.find("path");
  if(ii == settings.end()) {
    throw std::runtime_error("Iom "+name+" lacked a setting for 'path'");
  }
  path = ii->second;
  if(path.back()!='/') path=path+"/";
  
  //TODO Replace this C nastiness with c++17 stuff, or throw in a lib
  
  stringstream ss;
  struct stat sb;
  if(stat(path.c_str(), &sb)==0){

    //If this isn't a directory for us,
    if(!S_ISDIR(sb.st_mode)){
      ss <<"IOM PIO Failed. Path '"<<path<<"' exists but is not a directory";
      throw std::runtime_error(ss.str());
    }

    //Directory exists, but can user write to it?
    if(access(path.c_str(), W_OK)!=0) { 
      ss <<"IOM PIO Failed. User cannot access path '"<<path<<"'";
      throw std::runtime_error(ss.str());
    }
    //All ok
    return;
  }
  //Path didn't exist. Try creating the directory (note: not a recursive thing)
  
  
  //See if we can create the directory
  int rc=mkdir(path.c_str(), S_IRWXU |S_IRWXG );
  if(rc != 0) {
    ss << "IOM PIO failed. User cannot create directory '"<<path<<"'";                                                          
    throw std::runtime_error(ss.str());          
  }
  return;
}

rc_t IomPosixIndividualObjects::GetInfo(faodel::bucket_t bucket, const Key &key, kv_col_info_t *col_info) {
  rc_t rc;
  //cout<<"PIO::GetInfo.. current col_info is "<<col_info->str()<<endl;
  if(col_info)
    memset(col_info, 0, sizeof(col_info));
  
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
  string fname = genBucketPathFile(bucket, key);
  ldo.writeToFile(fname.c_str());
}


rc_t IomPosixIndividualObjects::ReadObject(faodel::bucket_t bucket, const Key &key, lunasa::DataObject *ldo) {

  string fname = genBucketPathFile(bucket, key);
  struct stat sb;
  
  if((stat(fname.c_str(), &sb)==0) && (S_ISREG(sb.st_mode))) {
    if(ldo!=nullptr) {
      *ldo = lunasa::DataObject(0, sb.st_size, lunasa::DataObject::AllocatorType::eager);
      ldo->readFromFile(fname.c_str());
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

  //See if we can write the path
  if(access(bucket_path.c_str(), W_OK)!=0) {
    if(mkdir(bucket_path.c_str(), S_IRWXU |S_IRWXG ) != 0) {
      stringstream ss;
      ss <<"Could not write to '"<<bucket_path<<"'";
      throw std::runtime_error(ss.str());
    }
  }
  return bucket_path;
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
void IomPosixIndividualObjects::AppendWebInfo(webhook::ReplyStream rs, string reference_link, const map<string,string> &args) {

  vector<vector<string>> items=
    { {"Setting", "Value"},
      {"Name", name   },
      {"Type", Type() },
      {"Path", path}  };
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
      rs.mkList(links, "Buckets");

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
