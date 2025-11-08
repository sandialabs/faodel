// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <glob.h>

#include "faodel-common/StringHelpers.hh"

#include "kelpie/ioms/IomPosixIndividualObjects.hh"


using namespace std;

namespace kelpie {
namespace internal {

  constexpr char IomPosixIndividualObjects::type_str[];
  
IomPosixIndividualObjects::IomPosixIndividualObjects(std::string name, const map<string,string> &new_settings)
  : IomBase(name, new_settings, {"path","path.env_name"}) {

  //Set debug info
  SetSubcomponentName("-pio-"+name);

  //Resolve the path from config settings. May go out to env var is "path.env_name" used.
  path = faodel::GetPathFromComponentSettings(settings);
  if(path.empty()){
    throw std::runtime_error("Iom "+name+" lacked a setting for 'path'");
  }

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

rc_t IomPosixIndividualObjects::GetInfo(faodel::bucket_t bucket, const Key &key, object_info_t *info) {
  rc_t rc;
  dbg("GetInfo for "+key.str());
  if(info) info->Wipe();

  string fname = genBucketPathFile(bucket, key);
  struct stat sb;
  if((stat(fname.c_str(), &sb)==0) && (S_ISREG(sb.st_mode))){
    if(info){
      info->col_user_bytes = sb.st_size - lunasa::DataObject::GetHeaderSize();
      info->col_availability = Availability::InDisk;
    }
    rc = KELPIE_OK;
  } else {
    if(info) {
      info->col_availability = Availability::Unavailable;
    }
    rc = KELPIE_ENOENT;
  }
  return rc;
}

rc_t IomPosixIndividualObjects::WriteObject(faodel::bucket_t bucket, const Key &key, const lunasa::DataObject &ldo) {
  dbg("WriteObject "+key.str());
  string fname = genBucketPathFile(bucket, key);
  uint32_t rc = ldo.writeToFile(fname.c_str());
  stat_wr_requests++;
  stat_wr_bytes+=ldo.GetWireSize();
  return (rc==0) ? KELPIE_OK : KELPIE_EIO;
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
    }
    stat_rd_bytes+=sb.st_size;
    stat_rd_hits++;
    return KELPIE_OK;

  } else {
    stat_rd_misses;
    return KELPIE_ENOENT;
  }
}

rc_t IomPosixIndividualObjects::ListObjects(faodel::bucket_t bucket, Key key, ObjectCapacities *oc) {

  // NOTES: 
  // * Only currently supported wildcard is a glob ('*') suffix
  // * Wildcards are only currently supported for the column of the Key

  // HANDLE wildcard keys (wildcard characters, e.g., '*', should not be encoded into the path name)
  bool k1_is_wild=key.IsRowWildcard();
  bool k2_is_wild=key.IsColWildcard();
  string k1 = key.K1();
  string k2 = key.K2();
  if(k1_is_wild) k1=k1.substr(0, k1.size()-1);
  if(k2_is_wild) k2=k2.substr(0, k2.size()-1);


  string bucket_path = genBucketPath(bucket);

  DIR *dp;
  struct dirent *ep;
  dp = opendir(bucket_path.c_str());
  if(dp!=NULL) {
    while((ep = readdir(dp)) != NULL) {
      string name = string(ep->d_name);
      if(!((name == ".") || (name == ".."))) {
        string pname = bucket_path + "/" + name;
        struct stat sb;
        if((stat(pname.c_str(), &sb) == 0) && (S_ISREG(sb.st_mode))) {
          Key key;
          key.pup(faodel::ExpandPunycode(name));

          //See if actually matches
          if(key.matchesPrefixString(k1_is_wild, k1, k2_is_wild, k2)) {
            oc->keys.push_back(key);
            oc->capacities.push_back(sb.st_size);
          }
        }
      }
    }
    closedir(dp);
  }

#if 0
  bool wildcard = key.IsColWildcard();

  cout<<"PIO-LIST: is wildcard: "<<wildcard<<endl;

  // HANDLE wildcard keys (wildcard characters, e.g., '*', should not be encoded into the path name)
  string k2 = key.K2();
  if( wildcard ) {
    // Drop the wildcard before we pack the key below
    key.K2(k2.substr(0, k2.size()-1));
  }
    
  string bucketfilestring = genBucketPathFile(bucket, key);

  cout<<"PIO-LIST: bucketfilstring is "<<bucketfilestring<<endl;

  // DROP the last six characters, they encode two bytes which contain size information.
  string globstring = bucketfilestring.substr(0, bucketfilestring.size()-6); 
  if( wildcard ) globstring += "*";

  cout <<"PIO-LIST: globstr is "<<globstring<<endl;
  glob_t g;
  int rc = glob(globstring.c_str(), 0, NULL, &g);
  if( rc != 0 ) return KELPIE_ENOENT;
  
  for( int i = 0; i < g.gl_pathc; i++ ) { 
    // RECOVER the key
    Key k;
    getKeyFromBucketPathFile(g.gl_pathv[i], &k);
    oc->keys.push_back(k);

    cout <<"PIO-LIST: recovered key "<<k.str()<<endl;

    // USE the size of the file as the capacity
    struct stat buffer;
    int rc = stat(g.gl_pathv[i], &buffer);
    oc->capacities.push_back(buffer.st_size);
  }
#endif

  return KELPIE_OK;
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

int IomPosixIndividualObjects::getKeyFromBucketPathFile(const string path, Key *key){
  const char *basename = path.c_str();
  const char *tmp;

  // ACQUIRE the filename (i.e., exclude the path information)
  while (1) {
    tmp = strstr(basename, "/");
    if( tmp == 0 ) break;
    basename = tmp + 1;
  }

  // RECOVER the key value from the filename
  string s = faodel::ExpandPunycode(basename);
  key->pup(s);
  return 0;
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
      {"Read Request Hits", to_string(stat_rd_hits)},
      {"Read Request Misses", to_string(stat_rd_misses)},
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
