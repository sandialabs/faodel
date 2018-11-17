// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_DIRECTORYMANAGERCOREUNCONFIGURED_HH
#define OPBOX_DIRECTORYMANAGERCOREUNCONFIGURED_HH

#include "common/Common.hh"
#include "opbox/services/dirman/core/DirManCoreBase.hh"

namespace opbox {
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
  bool GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, DirectoryInfo *dir_info) override;
  bool HostNewDir(const DirectoryInfo &dir_info) override;
  bool JoinDirWithName(const faodel::ResourceURL &url, std::string name, DirectoryInfo *dir_info=nullptr) override;
  bool LeaveDir(const faodel::ResourceURL &url, DirectoryInfo *dir_info=nullptr) override;

  //Internal API for implementing Exposed API. Most of these call Panic() to catch unconfigured system
  bool discoverParent(const faodel::ResourceURL &resource_url, faodel::nodeid_t *parent_node) override;
  bool cacheForeignDir(const DirectoryInfo &dir_info) override;
  bool lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, DirectoryInfo *dir_info=nullptr) override;
  bool joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply=false) override;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  bool Panic(std::string fname) const;
  bool dirman_service_none;


};

} // namespace internal
} // namespace dirman
} // namespace opbox

#endif // OPBOX_DIRECTORYMANAGERCOREUNCONFIGURED_HH
