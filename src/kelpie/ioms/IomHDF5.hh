// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMHDF_HH
#define KELPIE_IOMHDF_HH

#include "hdf5.h"

#include "kelpie/ioms/IomBase.hh"

namespace kelpie {
namespace internal {

class IomHDF5
        : public IomBase,
          public faodel::InfoInterface {

public:

  using kvpair = std::pair<kelpie::Key, lunasa::DataObject>;

  IomHDF5() = delete;

  IomHDF5(const std::string &name, const std::map<std::string, std::string> &new_settings);

  ~IomHDF5();

  rc_t GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, kv_col_info_t *col_info) override;

  void WriteObject(faodel::bucket_t, const kelpie::Key &key, const lunasa::DataObject &ldo) override;

  void WriteObjects(faodel::bucket_t, const std::vector<kvpair> &kvpairs);

  rc_t ReadObject(faodel::bucket_t, const kelpie::Key &key, lunasa::DataObject *ldo) override;

  rc_t ReadObjects(faodel::bucket_t, std::vector<kelpie::Key> &keys,
                   std::vector<kvpair> *found_keys,
                   std::vector<kelpie::Key> *missing_keys);

  constexpr static char type_str[] = "HDF5";

  std::string Type() const override { return IomHDF5::type_str; };

  void AppendWebInfo(faodel::ReplyStream rs, std::string reference_link,
                     const std::map<std::string, std::string> &args) override;

  void sstr(std::stringstream &ss, int depth = 0, int indent = 0) const override;

protected:

  hid_t hfile_, ldo_payload_ht_, ldo_payload_hs_, ldo_attr_space_;

  std::string path_;

  std::map<const std::string, hid_t> bmap_;


};

} // namespace internal
} // namespace kelpie

#endif // KELPIE_IOMHDF_HH


  
