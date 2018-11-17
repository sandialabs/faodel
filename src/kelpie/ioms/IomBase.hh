// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMBASE_HH
#define KELPIE_IOMBASE_HH

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/ReplyStream.hh"

#include "lunasa/DataObject.hh"

#include "kelpie/Key.hh"
#include "kelpie/common/Types.hh"
#include "kelpie/localkv/LocalKV.hh"


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
class IomBase
        : public faodel::LoggingInterface {
public:

  IomBase()
          : LoggingInterface("kelpie.iom"),
            stat_wr_requests(0), stat_wr_bytes(0),
            stat_rd_requests(0), stat_rd_bytes(0) {}
  IomBase(std::string name,
	  const std::map< std::string, std::string >& new_settings,
	  const std::vector< std::string >& valid_settings )
          : LoggingInterface("kelpie.iom"), name(name),
            stat_wr_requests(0), stat_wr_bytes(0),
            stat_rd_requests(0), stat_rd_bytes(0)
  {
    // Only keep settings that are valid
    for( auto&& s : valid_settings ) {
      auto found = new_settings.find( s );
      settings[s] = found == new_settings.end() ? "" : found->second;
    }
  };
    

  ~IomBase() override { }

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

  virtual void AppendWebInfo(faodel::ReplyStream rs, std::string reference_link, const std::map<std::string,std::string> &args) = 0;

protected:
  std::map<std::string,std::string> settings;
  std::string name;

  int stat_wr_requests, stat_wr_bytes;
  int stat_rd_requests, stat_rd_bytes;
  
};

} // namespace internal
} // namespace kelpie

#endif // KELPIE_IOM_HH
