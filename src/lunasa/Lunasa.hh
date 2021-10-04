// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file Lunasa.hh
 *
 * @brief Lunasa.hh
 * 
 */

#ifndef LUNASA_LUNASA_HH
#define LUNASA_LUNASA_HH


#include <iostream>

#include "faodel-common/Common.hh"
#include "lunasa/common/Types.hh"

namespace lunasa {


std::string bootstrap();

typedef std::function<void(void *base_addr, size_t length, void *&pinned)> net_pin_fn;
typedef std::function<void(void *&pinned)> net_unpin_fn; 

class LunasaConfigurationException: public std::exception
{
  public:
    LunasaConfigurationException(std::string msg)
    {
      std::string prefix("[LUNASA] ");
      mMsg = prefix + msg;
    }

  const char* what() const throw() override {
      return mMsg.c_str();
    }

  private:
    std::string mMsg;
};

//Forward references that are defined in private files
namespace internal { class LunasaCoreBase; }
namespace internal { class AllocatorBase; }


class Lunasa : public faodel::InfoInterface {
public:

  Lunasa();
  Lunasa(faodel::internal_use_only_t iuo,
         internal::AllocatorBase *lazy_allocator, internal::AllocatorBase *eager_allocator);

  Lunasa(const Lunasa&);
  ~Lunasa() override;

  Lunasa& operator=(const Lunasa&);

  static size_t TotalAllocated();
  static size_t TotalManaged();
  static size_t TotalUsed();
  static size_t TotalFree();

  static bool SanityCheck ();

  void PrintState (std::ostream& stream) ;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:
  internal::AllocatorBase *lazyImpl;
  internal::AllocatorBase *eagerImpl;
};

void Init(const faodel::Configuration &config);
void Finish();


void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);

void RegisterDataObjectType(dataobject_type_t tag, std::string name, fn_DataObjectDump_t dump_func);
void DeregisterDataObjectType(dataobject_type_t tag);
bool DumpDataObject(const DataObject &ldo, faodel::ReplyStream &rs);


DataObject LoadDataObjectFromFile(std::string filename);


std::vector<std::string> AvailableAllocators();
std::vector<std::string> AvailableCores();

} // namespace lunasa

#endif // LUNASA_LUNASA_HH
