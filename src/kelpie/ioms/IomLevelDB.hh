// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_IOMLEVELDB_HH
#define KELPIE_IOMLEVELDB_HH

#include "leveldb/db.h"

#include "kelpie/ioms/IomBase.hh"

namespace kelpie {
namespace internal {

class IomLevelDB
        : public IomBase,
          public faodel::InfoInterface {

public:

  using kvpair = std::pair<kelpie::Key, lunasa::DataObject>;

  IomLevelDB() = delete;

  IomLevelDB(const std::string &name, const std::map<std::string, std::string> &new_settings);

  ~IomLevelDB();

  rc_t GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, object_info_t *info) override;

  rc_t WriteObject(faodel::bucket_t, const kelpie::Key &key, const lunasa::DataObject &ldo) override;
  void internal_WriteObject(faodel::bucket_t, const std::vector<kvpair> &kvpairs);

  rc_t ReadObject(faodel::bucket_t, const kelpie::Key &key, lunasa::DataObject *ldo) override;

//  rc_t ReadObjects(faodel::bucket_t, std::vector<kelpie::Key> &keys,
//                   std::vector<kvpair> *found_keys,
//                   std::vector<kelpie::Key> *missing_keys);

  constexpr static char type_str[] = "LevelDB";

  std::string Type() const override { return IomLevelDB::type_str; }

  void AppendWebInfo(faodel::ReplyStream rs, std::string reference_link,
                     const std::map<std::string, std::string> &args) override;

  /// Return a list of all the setting names this IOM accepts at construction and provide a brief description for each
  static const std::vector<std::pair<std::string,std::string>> ValidSettingNamesAndDescriptions() {
    return {
          {"path",   "The path that the IOM writer should use for storing data"},
          {"unique", "An additional marker appended to path to make this instance unique"}
    };
  }
  void sstr(std::stringstream &ss, int depth = 0, int indent = 0) const override;

protected:

  leveldb::DB *bucketToDB(faodel::bucket_t &);

  std::map<const std::string, leveldb::DB *> bmap_;
  std::string path_;

  leveldb::DB *db_;
  leveldb::Options leveldb_opts_;

};

} // namespace internal
} // namespace kelpie

#endif // KELPIE_IOMLEVELDB_HH


  
