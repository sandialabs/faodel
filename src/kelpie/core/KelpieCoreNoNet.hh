// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_KELPIECORENONET_HH
#define KELPIE_KELPIECORENONET_HH

#include <iostream>

#include "kelpie/ioms/IomRegistry.hh"
#include "kelpie/core/KelpieCoreBase.hh"

namespace kelpie {
namespace internal {

/**
 * @brief A basic KelpieCore implementation with no network capabilities
 *
 * Developers sometimes want a minimal KelpieCore implementation that doesn't
 * have any network functionality. This NoNet implementation is a simple
 * wrapper around the LocalKV and does not have any network capabilities.
 */
class KelpieCoreNoNet : public KelpieCoreBase {

public:

  KelpieCoreNoNet();
  ~KelpieCoreNoNet() override;
  KelpieCoreNoNet(const KelpieCoreNoNet &)             = delete;  //don't copy ourselves
  KelpieCoreNoNet(KelpieCoreNoNet &&)                  = default;
  KelpieCoreNoNet & operator=(const KelpieCoreNoNet &) = default;
  KelpieCoreNoNet & operator=(KelpieCoreNoNet &&)      = default;

  //KelpieCoreBase functions
  void init(const faodel::Configuration &config) override;
  void start() override;
  void finish() override;

  std::string GetType() const override { return "nonet"; }
  void getLKV(LocalKV **localkv_ptr) override { *localkv_ptr = &lkv; }

  //Pool Management
  void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) override;
  Pool Connect(const faodel::ResourceURL &resource_url) override;

  //Pool Server
  int JoinServerPool(const faodel::ResourceURL &url, const std::string &optional_node_name) override;
  std::vector<std::string> GetRegisteredPoolTypes() const override;

  //Whookie handling
  void HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results);


  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  LocalKV lkv;
  PoolRegistry pool_registry;

};

}  // internal
}  // kelpie


#endif  // KELPIE_KELPIECORENONET_HH
