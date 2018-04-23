// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_KELPIECORESTANDARD_HH
#define KELPIE_KELPIECORESTANDARD_HH

#include <iostream>

#include "kelpie/core/KelpieCoreBase.hh"

namespace kelpie {
namespace internal {

/**
 * @brief This KelpieCore provides basic communication functionality
 *
 * This KelpieCore provides support for both local and remote operations,
 * and should be used for most Kelpie work.
 */
class KelpieCoreStandard : public KelpieCoreBase {

public:


  KelpieCoreStandard();
  ~KelpieCoreStandard();
  KelpieCoreStandard(const KelpieCoreStandard &)             = delete;  //don't copy ourselves
  KelpieCoreStandard(KelpieCoreStandard &&)                  = default;
  KelpieCoreStandard & operator=(const KelpieCoreStandard &) = default;
  KelpieCoreStandard & operator=(KelpieCoreStandard &&)      = default;


  void init(const faodel::Configuration &config) override;
  void start() override;
  void finish() override;

  //Pool Management
  void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) override;
  Pool Connect(const faodel::ResourceURL &resource_url) override;
 
  //IOM Management
  void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) override;
  IomBase * FindIOM(uint32_t iom_hash) override;

  std::string GetType() const override { return "standard"; }
  void getLKV(LocalKV **localkv_ptr) override { *localkv_ptr = &lkv; }

  void HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results);

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  LocalKV lkv;
  IomRegistry iom_registry;
  PoolRegistry pool_registry;
  
};

}  // internal
}  // kelpie


#endif  // KELPIE_KELPIECORESTANDARD_HH
