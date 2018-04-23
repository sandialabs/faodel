// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_DIRECTORYCACHE_HH
#define OPBOX_DIRECTORYCACHE_HH

#include <string>
#include <vector>
#include <map>

#include "common/Common.hh"
#include "webhook/common/ReplyStream.hh"
#include "opbox/services/dirman/DirectoryInfo.hh"


namespace opbox {

/**
 * @brief A unit for caching DirMan directory information
 *
 * The DC is used to cache DirMan director resources. It stores the actual
 * DirectoryInfo resources.
 */
class DirectoryCache
        : public faodel::InfoInterface {

public:

  const static std::string auto_generate_option_label;

  DirectoryCache() : dc_name("UNINITIALIZED"), mutex(nullptr) {};
  ~DirectoryCache() override;

  void Init(const faodel::Configuration &conf, std::string dc_name, std::string threading_model, std::string mutex_type);

  bool Create(const DirectoryInfo &resource);
  bool Create(const std::vector<DirectoryInfo> &resources, int *num_created=nullptr);

  bool CreateAndLinkParents(const DirectoryInfo &resource);


  bool Remove(const faodel::ResourceURL &dir_url);

  bool Update(const DirectoryInfo &resource);
  bool Update(const std::vector<DirectoryInfo> &resources, int *items_modified=nullptr);

  bool Join(const faodel::ResourceURL &search_url, DirectoryInfo *resource_info=nullptr);
  bool Leave(const faodel::ResourceURL &search_url, DirectoryInfo *resource_info=nullptr);



  bool Lookup(const faodel::ResourceURL &search_url, DirectoryInfo *resource_info=nullptr, faodel::nodeid_t *reference_node=nullptr);
  bool Lookup(const std::vector<faodel::ResourceURL> &resource_urls, std::vector<DirectoryInfo> *resource_infos=nullptr);

  //bool FindLastKnownParent(const faodel::ResourceURL &search_url, faodel::nodeid_t *node=nullptr, int *parent_steps_up=nullptr);
  void GetAllURLs(std::vector<faodel::ResourceURL> *urls=nullptr);
  std::vector<faodel::ResourceURL> GetAllURLs();

  size_t NumberOfResources() const { return known_resources.size(); }

  void webhookInfo(webhook::ReplyStream &rs);
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:

  bool write(const DirectoryInfo &resource, bool overwrite_existing);
  bool write(const std::vector<DirectoryInfo> &resources, int *num_created, bool overwrite_existing);

  bool _write(const DirectoryInfo &resource_info, bool overwrite_existing);
  bool _lookup(const faodel::ResourceURL &url, DirectoryInfo **resource_info, faodel::nodeid_t *reference_node);
  bool _removeSingleDir(const faodel::ResourceURL &url, std::vector<faodel::ResourceURL> *children=nullptr);


  std::string dc_name;
  std::map<std::string, DirectoryInfo *> known_resources;
  faodel::MutexWrapper *mutex;

  void log(std::string s);
  bool debug;

};


} // namespace opbox

#endif //OPBOX_DIRECTORYCACHE_HH
