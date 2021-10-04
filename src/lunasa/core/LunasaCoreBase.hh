// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef LUNASA_LUNASACOREBASE_HH
#define LUNASA_LUNASACOREBASE_HH

#include "faodel-common/Common.hh"
//#include "lunasa/DataObject.hh"
#include "lunasa/common/Allocation.hh"
#include "lunasa/Lunasa.hh"

#include "lunasa/allocators/Allocators.hh"

namespace lunasa {
namespace internal {

//Forward references
struct Allocation;


class LunasaCoreBase
        : public faodel::InfoInterface,
          public faodel::LoggingInterface {

public:
  LunasaCoreBase(std::string subcomponent_name);

  ~LunasaCoreBase() override;
  void init(const faodel::Configuration &config);
  virtual void start()=0;
  virtual void finish()=0;
  
  virtual void init(std::string lmm_name, std::string emm_name, bool use_whookie,
                    const faodel::Configuration &config) = 0;
  
  virtual void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) = 0;

  virtual Allocation *AllocateEager(uint32_t user_capacity) = 0;
  virtual Allocation *AllocateLazy(uint32_t user_capacity) = 0;

  virtual size_t TotalAllocated() const = 0;
  virtual size_t TotalManaged() const = 0;
  virtual size_t TotalUsed() const = 0;
  virtual size_t TotalFree() const = 0;
  
  virtual bool SanityCheck()  = 0;
  virtual void PrintState(std::ostream& stream)  = 0;

  virtual Lunasa GetLunasaInstance() = 0;

  virtual std::string GetType() const = 0;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override = 0;
  
private:
  bool configured;
  
};
} // namespace internal
} // namespace lunasa

#endif // LUNASA_LUNASACOREBASE_HH
