// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_DATAOBJECTTYPEREGISTRY_HH
#define FAODEL_DATAOBJECTTYPEREGISTRY_HH

#include <map>
#include <string>

#include "faodel-common/InfoInterface.hh"
#include "faodel-common/MutexWrapper.hh"
#include "faodel-common/StringHelpers.hh"

#include "lunasa/common/Types.hh"
#include "lunasa/DataObject.hh"

namespace lunasa {


class DataObjectTypeRegistry
        : public faodel::InfoInterface {

public:
  DataObjectTypeRegistry();
  ~DataObjectTypeRegistry() override { if(mutex) delete mutex; }


  void RegisterDataObjectType(dataobject_type_t type_id, std::string name, fn_DataObjectDump_t dump_func);
  void DeregisterDataObjectType(dataobject_type_t type_id);

  bool DumpDataObject(const DataObject &ldo, faodel::ReplyStream &rs);

  void DumpRegistryStatus(faodel::ReplyStream &rs);

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

private:

  faodel::MutexWrapper *mutex;

  std::map<lunasa::dataobject_type_t, std::string> ldo_names;
  std::map<lunasa::dataobject_type_t, fn_DataObjectDump_t> ldo_dump_functions;

};


namespace internal {
void fn_hexdump_dataobject(const DataObject &ldo, faodel::ReplyStream &rs);
}

} // namespace lunasa

#endif //FAODEL_DATAOBJECTTYPEREGISTRY_HH
