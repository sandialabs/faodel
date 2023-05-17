// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef DIRMAN_DIRECTORYCACHE_HH
#define DIRMAN_DIRECTORYCACHE_HH

#include <string>
#include <vector>
#include <map>

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/DirectoryInfo.hh"
#include "faodel-common/ReplyStream.hh"

namespace dirman {

/**
 * @brief A unit for caching DirMan directory information
 *
 * The DC is used to cache DirMan director resources. It stores the actual
 * DirectoryInfo resources.
 */
class DirectoryCache
        : public faodel::InfoInterface,
          public faodel::LoggingInterface {

public:

  const static std::string auto_generate_option_label;

  DirectoryCache() = delete; //Need to specify the name of the component
  explicit DirectoryCache(const std::string &full_name);  //Should be something like dirman.cache.mine

  ~DirectoryCache() override;

  void Init(const faodel::Configuration &conf, std::string threading_model, std::string mutex_type);

  bool Create(const faodel::DirectoryInfo &resource);
  bool Create(const std::vector<faodel::DirectoryInfo> &resources, int *num_created=nullptr);

  bool CreateAndLinkParents(const faodel::DirectoryInfo &resource);


  bool Remove(const faodel::ResourceURL &dir_url);

  bool Update(const faodel::DirectoryInfo &resource);
  bool Update(const std::vector<faodel::DirectoryInfo> &resources, int *items_modified=nullptr);

  bool Join(const faodel::ResourceURL &child_url, faodel::DirectoryInfo *resource_info=nullptr);
  bool Leave(const faodel::ResourceURL &child_url, faodel::DirectoryInfo *resource_info=nullptr);



  bool Lookup(const faodel::ResourceURL &search_url, faodel::DirectoryInfo *resource_info=nullptr, faodel::nodeid_t *reference_node=nullptr);
  bool Lookup(const std::vector<faodel::ResourceURL> &resource_urls, std::vector<faodel::DirectoryInfo> *resource_infos=nullptr);

  //bool FindLastKnownParent(const faodel::ResourceURL &search_url, faodel::nodeid_t *node=nullptr, int *parent_steps_up=nullptr);
  void GetAllURLs(std::vector<faodel::ResourceURL> *urls=nullptr);
  std::vector<faodel::ResourceURL> GetAllURLs();
  void GetAllNames(std::vector<std::string> *names=nullptr);

  size_t NumberOfResources() const { mutex->ReaderLock(); size_t num = known_resources.size(); mutex->Unlock();return num; }

  void whookieInfo(faodel::ReplyStream &rs);
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:

  bool write(const faodel::DirectoryInfo &resource, bool overwrite_existing);
  bool write(const std::vector<faodel::DirectoryInfo> &resources, int *num_created, bool overwrite_existing);

  bool _write(const faodel::DirectoryInfo &resource_info, bool overwrite_existing);
  bool _lookup(const faodel::ResourceURL &url, faodel::DirectoryInfo **resource_info, faodel::nodeid_t *reference_node);
  bool _lookupRootDir(faodel::bucket_t, faodel::DirectoryInfo *resource_info);
  bool _removeSingleDir(const faodel::ResourceURL &url, std::vector<faodel::ResourceURL> *members=nullptr);

  std::map<std::string, faodel::DirectoryInfo *> known_resources;
  faodel::MutexWrapper *mutex;

};


} // namespace dirman

#endif //DIRMAN_DIRECTORYCACHE_HH
