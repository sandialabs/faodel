// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMBASE_HH
#define KELPIE_IOMBASE_HH

#include "common/FaodelTypes.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/localkv/LocalKV.hh"
#include "webhook/common/ReplyStream.hh"

namespace kelpie {
namespace internal {

/**
 * @brief An I/O Module for managing how data is exchanged with persistent storage devices
 *
 * An IOM is essentially a device driver that is responsible for exchanging data with a storage system. For example,
 * the IomPosixIndividualObjects (IOM-PIO) is a driver that writes each Lunasa Data Object handed to it as a file
 * in a POSIX filesystem directory. An IOM is intended to have a simple API that higher level classes can call
 * to manipulate data as they need.
 *
 */
class IomBase {
public:

  IomBase() {}
  IomBase(std::string name) : name(name) {};
  virtual ~IomBase() { }

  std::string Name() const { return name; }

  virtual void finish();

  virtual rc_t GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, kv_col_info_t *col_info) = 0;
  virtual rc_t GetInfo(faodel::bucket_t bucket, const std::vector<kelpie::Key> &keys, std::vector<kv_col_info_t> *col_infos);
  
  virtual void WriteObject(faodel::bucket_t bucket, const kelpie::Key &key, const lunasa::DataObject &ldo) = 0; 
  virtual void WriteObjects(faodel::bucket_t bucket, const std::vector<std::pair<kelpie::Key,lunasa::DataObject>> &items);

  virtual rc_t ReadObject(faodel::bucket_t bucket, const kelpie::Key &key, lunasa::DataObject *ldo) = 0;
  virtual rc_t ReadObjects(faodel::bucket_t bucket, const std::vector<kelpie::Key> &keys,
                           std::vector<std::pair<kelpie::Key, lunasa::DataObject>> *found_objects,
                           std::vector<kelpie::Key> *missing_keys);
  
  virtual std::string Type() const = 0;
  virtual std::map<std::string,std::string> Settings() const { return settings; }
  virtual std::string Setting(std::string setting_name) const;

  virtual void AppendWebInfo(webhook::ReplyStream rs, std::string reference_link, const std::map<std::string,std::string> &args) = 0;

protected:
  std::map<std::string,std::string> settings;
  std::string name;
  
};

} // namespace internal
} // namespace kelpie

#endif // KELPIE_IOM_HH
