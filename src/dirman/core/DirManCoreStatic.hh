// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef DIRMAN_DIRECTORYMANAGERCOREStatic_HH
#define DIRMAN_DIRECTORYMANAGERCOREStatic_HH


#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/DirectoryInfo.hh"

#include "dirman/common/DirectoryCache.hh"
#include "dirman/common/DirectoryOwnerCache.hh"

#include "dirman/core/DirManCoreBase.hh"

namespace dirman {
namespace internal {

/**
 * @brief A Static implementation of the DirManCore
 *
 * This DirManCore simplifies the directory management service to a single,
 * Static node that is responsible for storing all DirMan entries.
 */
class DirManCoreStatic :
    public DirManCoreBase {

public:
  DirManCoreStatic() = delete;
  DirManCoreStatic(const DirManCoreStatic &d) = delete;

  explicit DirManCoreStatic(const faodel::Configuration &conf);
  ~DirManCoreStatic() override;

  //Bootstrap Internal API: unconfigured calls these to start/finish
  void start() override;
  void finish() override;


  //DirMan Exposed API
  std::string GetType() const override { return "Static"; }
  bool Locate(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node= nullptr) override;
  bool GetDirectoryInfo(const faodel::ResourceURL &url, bool check_local, bool check_remote, faodel::DirectoryInfo *dir_info) override;
  bool DefineNewDir(const faodel::DirectoryInfo &dir_info) override;
  bool HostNewDir(const faodel::DirectoryInfo &dir_info) override;
  bool JoinDirWithName(const faodel::ResourceURL &url, std::string name, faodel::DirectoryInfo *dir_info= nullptr) override;
  bool LeaveDir(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info= nullptr) override;
  bool DropDir(const faodel::ResourceURL &url) override;

  faodel::nodeid_t GetAuthorityNode() const { return my_node; }

  //Internal API for implementing Exposed API
  bool discoverParent(const faodel::ResourceURL &resource_url, faodel::nodeid_t *parent_node) override;
  bool cacheForeignDir(const faodel::DirectoryInfo &dir_info) override;
  bool lookupRemote(faodel::nodeid_t nodeid, const faodel::ResourceURL &resource_url, faodel::DirectoryInfo *dir_info= nullptr) override;
  bool joinRemote(faodel::nodeid_t parent_node, const faodel::ResourceURL &child_url, bool send_detailed_reply=false) override;

  void appendWhookieParameterTable(faodel::ReplyStream *rs) override;
  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;


  faodel::ResourceURL localizeURL(const faodel::ResourceURL &url, bool change_node=false);

};

} //namespace internal
} //namespace dirman

#endif // DIRMAN_DIRECTORYMANAGERCOREStatic_HH
