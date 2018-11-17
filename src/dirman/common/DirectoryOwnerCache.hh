// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_DIRECTORYOWNERCACHE_HH
#define OPBOX_DIRECTORYOWNERCACHE_HH

#include <string>
#include <vector>
#include <map>

#include "common/Common.hh"
#include "webhook/common/ReplyStream.hh"

namespace opbox {

/**
 * @brief A simple lookup table for finding which node is responsible for a resource
 *
 * The DOC provides a way for DirMan to remember which node is the point of contact
 * for a Resource.
 */
class DirectoryOwnerCache
        : public faodel::InfoInterface {

public:
  DirectoryOwnerCache() : mutex(nullptr) {};
  ~DirectoryOwnerCache() override;

  void Init(const faodel::Configuration &conf, std::string threading_model, std::string mutex_type);

  bool Register(const faodel::ResourceURL &url);
  bool Register(const std::vector<faodel::ResourceURL> &urls);

  bool Lookup(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node=nullptr);
  bool Lookup(const std::vector<faodel::ResourceURL> &resource_urls, std::vector<faodel::nodeid_t> *reference_nodes=nullptr);

  //bool FindLastKnownParent(const faodel::ResourceURL &search_url, faodel::nodeid_t *node=nullptr, int *parent_steps_up=nullptr);

  size_t NumberOfResources() const { return known_resource_owners.size(); }

  void webhookInfo(webhook::ReplyStream &rs);
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:

  bool _Register(const faodel::ResourceURL &url);
  bool _Lookup(const faodel::ResourceURL &url, faodel::nodeid_t *node);

  std::map<std::string, faodel::nodeid_t> known_resource_owners;

  faodel::MutexWrapper *mutex;

  void log(std::string s);
  bool debug;

};


} // namespace opbox

#endif
