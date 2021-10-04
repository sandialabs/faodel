// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "faodel-common/ResourceURL.hh"
#include "faodel-common/StringHelpers.hh"

// Format: resource_type:<node>[bucket]/my/path/name&option1=x&option2=y

//Example URLs
// 1. peer:/my/path/mynodename
//    You can use the peer type to label a specific node in the system. This
//    is handy if you want to register ranks from one sim and use them in another
//
// 2. ref:/my/path/mydht
//    If you know the name of a resource but not how its defined, you can
//    issue a lookup to DirMan to retrieve its contents
//
// 3. local:
//    In kelpie you can talk directly to your local kv. You can append options
//    to customize how you want interface with it.
//
// 4. dht:[mybucket]/my/path/mydht&min_members=4&behavior=writetoremote_writetoiom_readtolocal&iom=myiom
//    You can define a resource and add options to specify how it behaves. In
//    this example we have a dht named /my/path/mydht that
//    needs four member nodes to work. A Publish operation will write data to the
//    remote node's memory and an associated iom named myiom. Wants/Needs will only
//    cache the data at the requesting node. This example does not include
//    the actual list of nodes in the pool.
//
// Note: a root level item like "/myroot" has path="/" and name "myroot"
//       the same way "/a/b/c" has path "/a/b" and name "c"


using namespace std;

namespace faodel {


/**
 * @brief Parse a string version of a url and break it into ResourceURL fields
 *
 * @param[in] url The string version of a url
 * @return ResourceURL
 */
ResourceURL::ResourceURL(const string &url)
  : reference_node(NODE_UNSPECIFIED), bucket(BUCKET_UNSPECIFIED) {

  string tmp_bucket, tmp_nodeid;
  ParseURL(url, &resource_type, &tmp_bucket, &tmp_nodeid, &path, &name, &options);

  //Convert bucket and node strings into real values
  if(!tmp_bucket.empty()) bucket = bucket_t(tmp_bucket);
  if(!tmp_nodeid.empty()) reference_node = nodeid_t(tmp_nodeid);

}

/**
 * @brief Sets the URL components using the given URL string.
 *
 * @param[in] url -  The orifginal URL in the form "resourcetype:<nodeid>[bucketid]/the/path/to/resource:option1=A:option2=B"
 * @retval 0 - Parsed fine
 * @retval EINVAL - todo- other error codes
 */
rc_t ResourceURL::SetURL(const std::string &url) {
  std::string temp_bucket, temp_nodeid;
  auto rc = ParseURL( url, &resource_type, &temp_bucket, &temp_nodeid, &path, &name, &options );
  if( rc == 0 ) {
    if( not temp_bucket.empty() ) bucket = bucket_t( temp_bucket );
    if( not temp_nodeid.empty() ) reference_node = nodeid_t( temp_nodeid );
  }
  return rc;
}

/**
 * @brief Verifies this object has a bucket and reference node defined, and does not have errors
 *
 * @retval TRUE All fields defined and no errors
 * @retval FALSE Missing fields or had errors when parsed
 */
bool ResourceURL::IsFullURL() const {
  return Valid() && (bucket.Valid()) && (reference_node.Valid());
}

/**
 * @brief Convert this object to a valid string url
 *
 * @param[in] include_type - Include resource type (eg, dht)
 * @param[in] include_node - Include reference node (eg "<cabed>")
 * @param[in] include_bucket - Include bucket identifier
 * @param[in] include_options - Include the resource's option fields
 * @return string The result of the encoding
 */
string ResourceURL::GetURL(bool include_type, bool include_node, bool include_bucket, bool include_options) const {
  stringstream ss;
  //if(include_type)         ss<<resource_type<<":"; //It's ok for RT to be blank. Useful for requests where we don't know type yet.
  if(include_type) {
    ss << ((resource_type.empty()) ? "ref" : resource_type) << ":";
  }
  if(include_node)         ss<<"<"<<reference_node.GetHex()<<dec<<">";
  if(include_bucket)       ss<<"["<<bucket.GetHex()<<"]";
  if(path!="/")            ss<<path;
  if(true)                 ss<<"/"<<name;
  if((include_options) &&
     (!options.empty()))   ss<<"&"<<GetSortedOptions();
  return ss.str();
}

/**
 * @brief Determine if this URL has any fields that have been set to anything
 * @return TRUE If reference_node, bucker, path, name, or options are non-default values
 * @return FALSE if any value has been set
 */
bool ResourceURL::IsEmpty() const {
  return (reference_node==NODE_UNSPECIFIED) &&
         (bucket ==BUCKET_UNSPECIFIED) &&
         (path.empty()) && (name.empty()) && (options.empty());
}

/**
 * @brief Convert the full path name to a version using dashes instead of slashes
 * @return string With dashes eg, "-my-full-path-name"
 */
string ResourceURL::Dashify() const {
  string s=GetPathName();
  std::replace(s.begin(), s.end(), '/','-');
  return s;
}


/**
 * @brief Append one or more directories to the end of the full path
 *
 * @param[in] next_dir The directory to append to the back of the path
 */
void ResourceURL::PushDir(string next_dir) {
  if(next_dir.empty()) return;

  stringstream ss;
  if(path!="/") ss<<path;
  ss<<"/"<<name;
  if(next_dir.at(0)!='/') ss<<"/";
  ss<<next_dir;

  vector<string> all_parts = SplitPath(ss.str());
  path = JoinPath(all_parts, all_parts.size()-1);
  name = all_parts.back();
}


/**
 * @brief Move up one directory level (removes name and modifies path)
 *
 * @return string The name that is being removed
 */
string ResourceURL::PopDir() {
  string out_dir = name;
  if(path=="/") {
    name="";
    return out_dir;
  }
  vector<string> all_parts = SplitPath(path);
  switch(all_parts.size()) {
  case 0: path="/"; name=""; break;
  case 1: path="/"; name=all_parts.at(0); break;
  default:
    path = JoinPath(all_parts, all_parts.size()-1);
    name = all_parts.back();
  }
  return out_dir;
}


/**
 * @brief Generate a new url that is steps_back generations older than the current url
 *
 * @param[in] steps_back How many generations to go back
 * @param[in] default_bucket An alrernate bucket to use
 * @param[in] default_node An alternate refernece node to use
 * @return ResourceURL for the ancestor
 */
ResourceURL ResourceURL::GetLineageReference(int steps_back, bucket_t default_bucket, nodeid_t default_node) const {

  bucket_t new_bucket = (default_bucket.Unspecified()) ? bucket : default_bucket;
  nodeid_t new_node = (default_node.Unspecified()) ? reference_node : default_node;
  string new_path;
  string new_name;

  if(steps_back<=0) {
    new_path = path;
    new_name = name;

  } else {

    vector<string> path_parts = SplitPath(path);
    if((size_t)steps_back >= path_parts.size()) {
      //Went too far back, return the base
      new_path="/";
      new_name = (path_parts.size()==0) ? name : path_parts[0];
    } else {

      new_path = JoinPath(path_parts, path_parts.size()-steps_back);
      new_name = path_parts[path_parts.size()-steps_back];
    }
  }

  return ResourceURL(string("ref"), new_node, new_bucket, new_path, new_name, string(""));

}


/**
 * @brief Determine how deep the path is ("<dht>/mydht" = 0, "dht:/a/mydht" = 1, "dht:/a/b/mydht" = 2, etc)
 *
 * @return int The number of parents in the path
 */
int ResourceURL::GetPathDepth() const {
  int count=0;
  for(size_t i=1; i<path.size(); i++) {
    if(path.at(i)=='/') count++;
  }
  return count;
}



/**
 * @brief Parse the given string and extract out specific fields. Normally, not for use outside this class
 *
 * @param[in] url -  The orifginal url in the form "resourcetype:<nodeid>[bucketid]/the/path/to/resource:option1=A:option2=B"
 * @param[out] resource_type - Resource type string (eg dht). Can be empty.
 * @param[out] bucket -  The bucket identifier
 * @param[out] nodeid - The node responsible for this resource
 * @param[out] path -  The path to the resource (excluding its name)
 * @param[out] name -  The name of the resource (exclusing its path)
 * @param[out] options -  Additional options, sorted by name
 * @retval 0 - Parsed fine
 * @throw invalid_argument On anything that cannot be parsed correctly
 */
rc_t ResourceURL::ParseURL(const string url,
                           string *resource_type, string *bucket,
                           string *nodeid, string *path, string *name, string *options) {

  size_t j,i=0;
  string tmp_type, tmp_bucket, tmp_nodeid, tmp_path_and_name, tmp_options;

  while(i<url.size()) {

    switch(url.at(i)) {
    case '[': //Bucket id
      {
        j = url.find_first_of("[]<>/:&",i+1); //Scan for delims
        if((j==string::npos)||(url.at(j)!=']')) //but only accept the closing id for bucket
          throw std::invalid_argument("ResourceURL parse problem with bucket in url '"+url+"'");

        tmp_bucket = url.substr(i+1,j-i-1);
        i=j+1;
      }
      break;
    case '<': //Node id
      {
        //j = url.find_first_of("[]<>/:",i+1);
        j = url.find_first_of(">",i+1); //allow some other delims, in case nodeid is its own url, like mpi://1
        if((j==string::npos)||(url.at(j)!='>'))
          throw std::invalid_argument("ResourceURL parse problem with node id in url '"+url+"'");

        tmp_nodeid = url.substr(i+1,j-i-1);
        i=j+1;
      }
      break;
    case '/': //Path-and-name
      {
        j = url.find_first_of("&",i+1); //All options start with & (formerly used :, but made for more parse problems)
        if(j==string::npos) j=url.size();
        tmp_path_and_name=url.substr(i,j-i);
        i=j;
      }
      break;
    case '&': //options
      {
        tmp_options=url.substr(i+1);
        i=url.size(); //All done
        //cout << "option: "<<"tmp_options: "<<tmp_options<<endl;
      }
      break;
    default:
      //Not a delimiter? That's fine, so long as we're at the beginning, working on ref type
      //We must be starting at offset 0 though to get the name
      if(i!=0)
        throw std::invalid_argument("ResourceURL parse problem (missing delimiter?) in middle of url '"+url+"'");

      //Grab the type from the front of the url
      if(url.at(0)==':') {
        //Anonymous reference type! move on
        tmp_type="";
        i=1;
      } else {
        //Some text. We need to find the end of the type. Only work if no delims between
        //this point and the next : in file.
        j=url.find_first_of("[]<>/:&",i);
        if((j==string::npos)||(url.at(j) != ':'))
          throw std::invalid_argument("ResourceURL parse problem (missing delimiter?) in url '"+url+"'");

        //Convert the type to lowercase to avoid problems
        tmp_type = url.substr(i,j);
        ToLowercaseInPlace(tmp_type);
        i=j+1;
      }
    }
  }

  //User may mark type as ref. Normalize that to ""
  if(tmp_type=="ref") {
    tmp_type="";
  }

  //Normalize type for local references
  if((tmp_type=="local") || (tmp_type=="localkv")||(tmp_type=="lkv")) {
    tmp_type = "local";
  }

  //Split path and name
  string tmp_path, tmp_name;
  tmp_path="/";
  tmp_name="";


  //Split the path and name
  if(!tmp_path_and_name.empty()) {

    //Check for root path
    if(tmp_path_and_name == "/") {
      tmp_path = "/";
      tmp_name = "";

    } else {
      //All others need to separate path and name

      //Chop off last /
      if(tmp_path_and_name.back() == '/') {
        tmp_path_and_name.pop_back();
      }

      //Find the separator between path and name
      j = tmp_path_and_name.find_last_of("/");
      if(j != string::npos) {
        //Has a slash somewhere. It still could be at the beginning or end
        tmp_path = (j == 0) ? "/" : tmp_path_and_name.substr(0, j);
        tmp_name = tmp_path_and_name.substr(j + 1);
      } else {
        //Did not have / in it. Assume it's root level
        tmp_path = "/";
        tmp_name = tmp_path_and_name;
      }

      //Validate. Should be /x/y  and z
      if(tmp_path.at(0) != '/')
        throw std::invalid_argument("ResourceURL parse problem: Path did not start with '/' in url '" + url + "'");

        //Some resource names are special and permit no path
      else if((tmp_name.empty()) && (tmp_type != "local") && (tmp_type !="null") && (tmp_type != "unconfigured"))
        throw std::invalid_argument("ResourceURL parse problem: Had zero-length name in url '" + url + "'");
    }

  } else {

    //Didn't get a path/name. Only time this is ok is when we do local and null
    if(!((tmp_type=="local") || (tmp_type=="null")))
      throw std::invalid_argument("ResourceURL parse problem: Pathname missing '/' in url '"+url+"'");

  }

  //Patch local references. If we are given a reference and the path starts
  //with local, the type gets set to local.
  //TODO: This option is a kludge and should be done away with.
  if(tmp_type.empty()) {

    if((tmp_path=="/local") ||
       ((tmp_path=="/") && (tmp_name=="local")) ||
       (StringBeginsWith(tmp_path,"/local/"))) {
      tmp_type="local";
    }
  }

  //Pass back all parts
  if(path)          *path = tmp_path;
  if(name)          *name = tmp_name;
  if(resource_type) *resource_type = tmp_type;
  if(bucket)        *bucket = tmp_bucket;
  if(nodeid)        *nodeid = tmp_nodeid;
  if(options)       *options = tmp_options;
  return 0;
}


bool ResourceURL::operator<( const ResourceURL &x) const {
  if( bucket < x.bucket) return true;
  if( path < x.path) return true;
  return (name < x.name);
}
bool ResourceURL::operator==(const ResourceURL &x) const {
  return
    (bucket == x.bucket) &&
    (path == x.path) &&
    (name == x.name);
}
bool ResourceURL::operator!=(const ResourceURL &x) const {
  return !(*this == x);
}


/**
 * @brief Set a particular option for a url
 *
 * @param[in] option_name The new option name
 * @param[in] value String value for the option
 */
void ResourceURL::SetOption(const string &option_name, const string &value) {
  string new_option = option_name+"="+value;
  //Look for the easy way first
  if(options.empty()) {
    options=new_option;
    return;
  }

  //Need to split up now.
  vector<string> ops;
  string sname=option_name+"=";
  bool found=false;
  Split(ops, options, '&', true);
  for(size_t i=0; i<ops.size(); i++) {
    if(ops[i].compare(0,sname.size(), sname)==0) {
      ops[i]=new_option;
      found=true;
      break;
    }
  }
  if(!found) ops.push_back(new_option);

  options = Join(ops, '&');
}

/**
 * @brief Look for a particular option and return its value or ""
 *
 * @param[in] option_name - The option name to fetch
 * @param[in] default_value - The string to return if not found (defaults to "")
 * @return string - The option value, or "" for not found
 */
string ResourceURL::GetOption(const string &option_name, const string &default_value) const {
  vector<string> ops;
  string sname=option_name+"=";
  Split(ops, options, '&', true);
  for(auto &full_op : ops) {
    if(full_op.compare(0, sname.size(), sname)==0) {
      return full_op.substr(sname.size());
    }
  }
  return default_value;
}

/**
 * @brief Generate a sorted string of options, joined by '&'
 *
 * @return string  (eg, "alpha=a&beta=b&op1=c")
 */
string ResourceURL::GetSortedOptions() const {
  vector<string> ops;
  Split(ops, options, '&', true);
  std::sort(ops.begin(), ops.end());
  return Join(ops,'&');
}

/**
 * @brief Convert the options into a vector of string pairs
 *
 * @return vector of string pairs
 *
 * @note This version allows user to pass same option in multiple times
 */
vector<std::pair<std::string,std::string>> ResourceURL::GetOptions() const {

  vector<pair<string,string>> op_pairs;

  vector <string> ops;
  Split(ops, options, '&', true);
  for(auto &full_op : ops) {
    vector<string> spair = Split(full_op, '=', true);
    pair<string,string> p(spair[0], spair[1]);
    op_pairs.push_back(p);
  }

  return op_pairs;
}

/**
 * @brief Remove all occurances of a particular option
 *
 * @param[in] option_name - The option name to remove
 * @return string - The value for the last option removed or ""
 */
string ResourceURL::RemoveOption(const string &option_name) {
  vector<string> ops;
  string removed_val="";
  string sname=option_name+"=";
  Split(ops, options, '&', true);
  for(size_t i=0; i<ops.size(); i++) {
    if(ops[i].compare(0, sname.size(), sname)==0) {
      removed_val = ops[i].substr(sname.size());
      ops.erase(ops.begin()+i);
      i--; //Everyone slided up by one
    }
  }
  options = Join(ops, '&');
  return removed_val;
}


} // namespace faodel


