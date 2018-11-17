// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef DIRMAN_DIRECTORYMANAGERCORECENTRALIZED_HH
#define DIRMAN_DIRECTORYMANAGERCORECENTRALIZED_HH


#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/DirectoryInfo.hh"

#include "dirman/common/DirectoryCache.hh"
#include "dirman/common/DirectoryOwnerCache.hh"

#include "dirman/core/DirManCoreBase.hh"

namespace dirman {
namespace internal {

/**
 * @brief A centralized implementation of the DirManCore
 *
 * This DirManCore simplifies the directory management service to a single,
 * centralized node that is responsible for storing all DirMan entries.
 */
class DirManCoreCentralized :
    public DirManCoreBase {

public:
  DirManCoreCentralized() = delete;
  DirManCoreCentralized(const DirManCoreCentralized &d) = delete;

  explicit DirManCoreCentralized(const faodel::Configuration &conf);
  ~DirManCoreCentralized() override;

  //Bootstrap Internal API: unconfigured calls these to start/finish
  void start() override;
  void finish() override;

  //DirMan Exposed API extras
  faodel::nodeid_t GetRootNode() const { return root_id; }
  bool AmRoot() const {  return am_root; }

  //DirMan Exposed API
  std::string GetType() const override { return "centralized"; }
  bool Locate(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node= nullptr) override;
  bool GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, faodel::DirectoryInfo *dir_info) override;
  bool HostNewDir(const faodel::DirectoryInfo &dir_info) override;
  bool JoinDirWithName(const faodel::ResourceURL &url, std::string name, faodel::DirectoryInfo *dir_info= nullptr) override;
  bool LeaveDir(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info= nullptr) override;

  faodel::nodeid_t GetAuthorityNode() const { return root_id; }

  //Internal API for implementing Exposed API
  bool discoverParent(const faodel::ResourceURL &resource_url, faodel::nodeid_t *parent_node) override;
  bool cacheForeignDir(const faodel::DirectoryInfo &dir_info) override;
  bool lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, faodel::DirectoryInfo *dir_info= nullptr) override;
  bool joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply=false) override;

  void appendWebhookParameterTable(faodel::ReplyStream *rs) override;
  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  faodel::nodeid_t root_id; //Where the absolute root is
  bool am_root;

  faodel::ResourceURL localizeURL(const faodel::ResourceURL &url, bool change_node=false);

};

} //namespace internal
} //namespace dirman

#endif // DIRMAN_DIRECTORYMANAGERCORECENTRALIZED_HH
