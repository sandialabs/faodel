// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef DIRMAN_DIRECTORYMANAGERCOREUNCONFIGURED_HH
#define DIRMAN_DIRECTORYMANAGERCOREUNCONFIGURED_HH

#include "faodel-common/Common.hh"
#include "dirman/core/DirManCoreBase.hh"


namespace dirman {
namespace internal {

/**
 * @brief A dummy DirManCore implementation for handling an Unconfigured state
 */
class DirManCoreUnconfigured :
    public DirManCoreBase {

public:
  DirManCoreUnconfigured();
  ~DirManCoreUnconfigured() override;

  //Bootstrap Internal API: unconfigured calls these to start/finish
  void start() override;
  void finish() override;

  //DirMan Exposed API. Most of these will call Panic() to catch an unconfigured system
  std::string GetType() const override { return "unconfigured"; };
  bool Locate(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node=nullptr) override;
  bool GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, faodel::DirectoryInfo *dir_info) override;
  bool HostNewDir(const faodel::DirectoryInfo &dir_info) override;
  bool JoinDirWithName(const faodel::ResourceURL &url, std::string name, faodel::DirectoryInfo *dir_info=nullptr) override;
  bool LeaveDir(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info=nullptr) override;

  faodel::nodeid_t GetAuthorityNode() const { return faodel::NODE_UNSPECIFIED; }

  //Internal API for implementing Exposed API. Most of these call Panic() to catch unconfigured system
  bool discoverParent(const faodel::ResourceURL &resource_url, faodel::nodeid_t *parent_node) override;
  bool cacheForeignDir(const faodel::DirectoryInfo &dir_info) override;
  bool lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, faodel::DirectoryInfo *dir_info=nullptr) override;
  bool joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply=false) override;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  bool Panic(std::string fname) const;
  bool dirman_service_none;


};

} // namespace internal
} // namespace dirman

#endif // DIRMAN_DIRECTORYMANAGERCOREUNCONFIGURED_HH
