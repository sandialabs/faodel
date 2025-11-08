// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_LUNASACORESPLIT_HH
#define LUNASA_LUNASACORESPLIT_HH

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/common/Allocation.hh"

namespace lunasa {
namespace internal {

class LunasaCoreSplit
        : public LunasaCoreBase {

public:
  LunasaCoreSplit();
  ~LunasaCoreSplit() override;
  
  void init(std::string lmm_name, std::string emm_name, bool use_whookie,
            const faodel::Configuration &config) override;
  void start() override {}
  void finish() override;

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) override;

  Allocation *AllocateEager(uint32_t user_capacity) override;
  Allocation *AllocateLazy(uint32_t user_capacity) override;
  
  size_t TotalAllocated() const override;
  size_t TotalManaged() const;
  size_t TotalUsed() const override;
  size_t TotalFree() const override;

  bool SanityCheck() override;
  void PrintState(std::ostream& stream) override;

  Lunasa GetLunasaInstance() override;

  std::string GetType() const override { return "split"; };

  //Whookie
  void HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieEagerDetails(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieLazyDetails(const std::map<std::string,std::string> &args, std::stringstream &results);
  
  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  
private:
  AllocatorBase *lazy_allocator;
  AllocatorBase *eager_allocator;
};

} // namespace internal
} // namespace lunasa

#endif // LUNASA_LUNASACORESPLIT_HH
