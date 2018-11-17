// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_RESOURCEURL_HH
#define FAODEL_COMMON_RESOURCEURL_HH

#include <string>
#include <vector>
#include <utility>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/InfoInterface.hh"
#include "faodel-common/Bucket.hh"
#include "faodel-common/NodeID.hh"

namespace faodel {

/**
 * @brief A general class for holding information (eg type, bucket, node, path, options) for a resource
 *
 * The ResourceURL class provides a way to describe different resources used
 * in the FAODEL. It contains multiple components:
 *
 * Format: "resource_type:<node>[bucket]/my/path/name&myop1=foo&myop2=bar"
 *
 *   - **Resource Type**:  A short prefix for standard types (eg, dht: ref:)
 *   - **Reference Node**: The NodeID that is responsible for this resource
 *   - **Bucket**:         A hash of a string to provide some namespace
 *                         isolation in a shared environment
 *   - **Path**:           A '/' separated path to the resource in the global
 *                         hierarchy
 *   - **Name**:           A name for the resource (placed at the end of path)
 *   - **Options**:        A string of additional "key=value" options
 *                         (separated by '&')
 *
 *
 * Valid Examples:
 *
 *  - dht:[mybucket]/my/path
 *  - dht:[mybucket]<0xAABB90>/my/path&num_nodes=4&replication=2
 *  - peer:[mybucket]<0xAABB90>/nodes/my_server
 *  - local:
 */
class ResourceURL : public InfoInterface {

public:
  ResourceURL() = default; //Necessary for placeholders
  explicit ResourceURL(std::string url);
  ResourceURL(std::string resource_type, nodeid_t reference_node,
              bucket_t bucket,
              std::string path, std::string name, std::string options)
    : resource_type(resource_type), reference_node(reference_node),
      bucket(bucket), path((path.empty())?"/":path), name(name),
      options(options) {}

  ~ResourceURL() override = default;

  std::string resource_type;   //eg ref, dht
  nodeid_t    reference_node;  //The node that is the PoC for this resource
  bucket_t    bucket;          //hashed version [bucket]
  std::string path;            //eg /root/rack0
  std::string name;            //eg mydht
  std::string options;         //eg member_count=25&replication=1

  bool Valid() const { return (!path.empty()) && (!name.empty());} //!< True if there is at least both a path and a name
  bool IsRootLevel() const { return (path=="/"); } //!< True if this lives in the root directory (eg "/mything")
  bool IsFullURL() const;
  bool IsEmpty() const;

  rc_t SetURL( const std::string& url );
  std::string GetURL(bool include_type=false, bool include_node=false, bool include_bucket=false, bool include_options=false) const;
  std::string GetPathName()       const { return GetURL(false,false,false,false); } //!< Get the path/name: /root/rack0/mydht
  std::string GetBucketPathName() const { return GetURL(false,false,true,false); }  //!< Get Bucket/path name: [a23]/root/rack0/mydht
  std::string GetFullURL()        const { return GetURL(true,true,true,true); }     //!< Get full encoding "<dht><e2d123>[a23]/root/rack0/mydht&num=2&thing=4"

  //Manipulate paths
  void PushDir(std::string next_dir);
  std::string PopDir();

  ResourceURL GetLineageReference(int steps_back=0, bucket_t default_bucket=BUCKET_UNSPECIFIED, nodeid_t default_node=NODE_UNSPECIFIED) const;
  ResourceURL GetParent() const { return GetLineageReference(1); } //!< Get resource that is the parent of this one

  int GetPathDepth() const;

  void        SetOption(std::string option_name, std::string value);
  std::string RemoveOption(std::string option_name);
  std::string GetOption(std::string option_name) const;
  std::string GetSortedOptions() const;
  std::vector< std::pair<std::string,std::string> > GetOptions() const;


  //Only compare on bucket, path, and name
  bool operator<( const ResourceURL &x) const;
  bool operator==( const ResourceURL &x) const;
  bool operator!=( const ResourceURL &x) const;

  //Serialization
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & resource_type;
    ar & reference_node;
    ar & bucket;
    ar & path;
    ar & name;
    ar & options;
  }

  //Info Interface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override {
    ss<<std::string(indent,' ')
      <<"ResourceURL: "  << GetURL() <<std::endl;

    if(depth>0) {
      ss<<std::string(indent+2,' ')
        <<" RefNode: "  << reference_node.GetHex()
        <<" Bucket: "   << bucket.GetHex()
        <<" Path: "     << path
        <<" Name: "     << name
        <<" Type: "     << resource_type
       <<std::endl;
    }
  }

protected:

  rc_t ParseURL(const std::string url,
                std::string *resource_type, std::string *bucket,
                std::string *nodeid, std::string *path, std::string *name, std::string *options);


private:

};



} // namespace faodel

#endif // FAODEL_COMMON_RESOURCEURL_HH
