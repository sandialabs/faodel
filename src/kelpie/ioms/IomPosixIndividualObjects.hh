// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMPOSIXINDIVIDUALOBJECTS_HH
#define KELPIE_IOMPOSIXINDIVIDUALOBJECTS_HH

#include "kelpie/ioms/IomBase.hh"

namespace kelpie {
namespace internal {

/**
 * @brief A basic IOM that stores objects as individual files in a posix filesystem
 *
 * The IomPosixIndividualObjects (IOM-PIO) driver is a minimal IOM that simply stores each object as
 * its own file in a POSIX directory. When handed a key/ldo to write, it uses a punycode version of
 * the key to name the file that is written. The data written out includes the header, meta, and data
 * sections of the object. Standard I/O operations are used to locate, read, and write files.
 */
class IomPosixIndividualObjects
  : public IomBase,
    public faodel::InfoInterface {
  
public:
  IomPosixIndividualObjects() = delete;
  IomPosixIndividualObjects(std::string name, const std::map<std::string,std::string> &new_settings);
  ~IomPosixIndividualObjects() override {}

  rc_t GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, kv_col_info_t *col_info) override;
  void WriteObject(faodel::bucket_t bucket, const kelpie::Key &key, const lunasa::DataObject &ldo) override;
  rc_t ReadObject(faodel::bucket_t bucket, const kelpie::Key &key, lunasa::DataObject *ldo) override;

  //WriteObjects and ReadObjects come from base class
  
  constexpr static char type_str[] = "PosixIndividualObjects";
  std::string Type() const override { return IomPosixIndividualObjects::type_str; };

  void AppendWebInfo(faodel::ReplyStream rs, std::string reference_link, const std::map<std::string,std::string> &args) override;
  
  //Info interface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  
private:
  std::string path;
  std::string genBucketPath(faodel::bucket_t bucket);
  std::string genBucketPathFile(faodel::bucket_t bucket, const kelpie::Key &key);
  std::vector<faodel::bucket_t> getBucketNames();
  std::vector<std::pair<std::string,std::string>> getBucketContents(std::string bucket);
};

} // namespace internal
} // namespace kelpie

#endif // KELPIE_IOMPOSIXINDIVIDUALOBJECTS_HH
