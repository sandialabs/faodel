// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_KELPIECOREUNCONFIGURED_HH
#define KELPIE_KELPIECOREUNCONFIGURED_HH


#include "kelpie/core/KelpieCoreBase.hh"
#include "kelpie/pools/PoolRegistry.hh"

namespace kelpie {
namespace internal {

/**
 * @brief This KelpieCore is for debugging. It throws exceptions on functions
 *
 * This KelpieCore is used to catch instances where a user attempts to perform
 * Kelpie functions before the node is initialized. The singleton plugs this
 * core in before init is called and after finish is called.
 */
class KelpieCoreUnconfigured :
    public KelpieCoreBase {

public:
  KelpieCoreUnconfigured();
  ~KelpieCoreUnconfigured();

  //KelpieCoreBase API
  void start() override;
  void finish() override;
  void init(const faodel::Configuration &config) override;
  std::string GetType() const override { return "unconfigured"; }
  void getLKV(LocalKV **localkv_ptr) override;

  //Pool Management
  void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) override;
  Pool Connect(const faodel::ResourceURL &resource_url) override;

  //IOM Management
  void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) override;
  IomBase * FindIOM(uint32_t iom_hash) override;
  
  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:

  void Panic(const std::string fname) const;
  // Not implemented
  KelpieCoreUnconfigured(KelpieCoreUnconfigured &);
  void operator= (KelpieCoreUnconfigured &);
};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_KELPIECOREUNCONFIGURED_HH
