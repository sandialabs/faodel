// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_LUNASACOREUNCONFIGURED_HH
#define LUNASA_LUNASACOREUNCONFIGURED_HH

#include "faodel-common/Common.hh"
#include "lunasa/core/LunasaCoreBase.hh"

namespace lunasa {
namespace internal {

class LunasaCoreUnconfigured
        : public LunasaCoreBase {
  
public:
  LunasaCoreUnconfigured();
  ~LunasaCoreUnconfigured() override;
  
  void init(std::string lmm_name, std::string emm_name, bool use_webhook,
            const faodel::Configuration &config) override;
  void start() override {}
  void finish() override {}

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) override;

  Allocation *AllocateEager(uint32_t user_capacity) override;
  Allocation *AllocateLazy(uint32_t user_capacity) override;
  
  size_t TotalAllocated() const;
  size_t TotalManaged() const override;
  size_t TotalUsed() const override;
  size_t TotalFree() const override;

  bool SanityCheck() override;
  void PrintState(std::ostream& stream) override;

  Lunasa GetLunasaInstance() override;

  std::string GetType() const override { return "unconfigured"; };

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  void Panic(std::string fname) const;
  
};
} // namespace internal
} // namespace lunasa

#endif // LUNASA_LUNASACOREUNCONFIGURED_HH
