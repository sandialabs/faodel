// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef Faodel_COMMON_RESOURCEURL_HH
#define Faodel_COMMON_RESOURCEURL_HH

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
 * in the Faodel. It contains multiple components:
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
 *  - dht:[mybucket]<0xAABB90>/my/path&min_members=4&replication=2
 *  - dht:[mybucket]<0xAABB90>/my/dataset&min_members=2&num=2&ag0=0xAAB1&ag1=0xAAB2
 *  - peer:[mybucket]<0xAABB90>/nodes/my_server
 *  - local:
 */
class ResourceURL : public InfoInterface {

public:
  ResourceURL() = default; //Necessary for placeholders
  explicit ResourceURL(const std::string &url);
  ResourceURL(const std::string &resource_type,
              nodeid_t reference_node,
              bucket_t bucket,
              const std::string &path,
              const std::string &name,
              const std::string &options)
    : reference_node(reference_node),
      bucket(bucket), path((path.empty())?"/":path), name(name),
      options(options),
      resource_type(resource_type) {}

  ~ResourceURL() override = default;

  std::string Type() const { return ((IsReference()) ? "ref" : resource_type); }
  nodeid_t    reference_node;  //The node that is the PoC for this resource
  bucket_t    bucket;          //hashed version [bucket]
  std::string path;            //eg /root/rack0
  std::string name;            //eg mydht
  std::string options;         //eg min_members=16&replication=1

  bool Valid() const { return (((!path.empty()) && (!name.empty())) || IsRoot());} //!< True if there is at least both a path and a name, or is the root
  bool IsRootLevel() const { return (path=="/"); }                 //!< True if this lives in the root directory (eg "/mything")
  bool IsRoot() const { return ((path=="/") && (name=="")); }      //!< True if this is the root (ie, "/")
  bool IsFullURL() const;
  bool IsEmpty() const;
  bool IsReference() const { return resource_type.empty(); } //!< True if this is a reference to a resource (ie ref:)

  rc_t SetURL(const std::string &url);
  std::string GetURL(bool include_type=false, bool include_node=false, bool include_bucket=false, bool include_options=false) const;
  std::string GetPathName()       const { return GetURL(false,false,false,false); } //!< Get the path/name: /root/rack0/mydht
  std::string GetBucketPathName() const { return GetURL(false,false,true,false); }  //!< Get Bucket/path name: [a23]/root/rack0/mydht
  std::string GetFullURL()        const { return GetURL(true,true,true,true); }     //!< Get full encoding "<dht><e2d123>[a23]/root/rack0/mydht&min_members=2&thing=4"

  std::string Dashify() const;

  //Manipulate paths
  void PushDir(std::string next_dir);
  std::string PopDir();

  ResourceURL GetLineageReference(int steps_back=0, bucket_t default_bucket=BUCKET_UNSPECIFIED, nodeid_t default_node=NODE_UNSPECIFIED) const;
  ResourceURL GetParent() const { return GetLineageReference(1); } //!< Get resource that is the parent of this one

  int GetPathDepth() const;

  void        SetOption(const std::string &option_name, const std::string &value);
  std::string RemoveOption(const std::string &option_name);
  std::string GetOption(const std::string &option_name, const std::string &default_value="") const;
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

  std::string resource_type;   //eg ref, local, dht.. Empty means ref

};



} // namespace faodel

#endif // FAODEL_COMMON_RESOURCEURL_HH
