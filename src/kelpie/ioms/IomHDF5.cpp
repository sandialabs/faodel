// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstring>
#include <string>
#include <map>
#include <vector>

#include "kelpie/ioms/IomHDF5.hh"

namespace kelpie {
namespace internal {

constexpr char IomHDF5::type_str[];

IomHDF5::IomHDF5(const std::string &name, const std::map<std::string, std::string> &new_settings)
        : IomBase(name, new_settings, {"path", "unique"}) {

  auto pi = settings.find("path");
  if(pi == settings.end()) {
    throw std::runtime_error("IOM" + name + " was not given a setting for 'path'");
  }
  path_ = pi->second + '/';

  pi = settings.find("unique");
  if(pi != settings.end()) {
    path_ += pi->second + '/';
  }


  hfile_ = H5Fcreate((path_ + "/iom.h5").c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  // Define the H5 data type / data space
  ldo_payload_ht_ = H5Tvlen_create(H5T_NATIVE_B8);
  if(ldo_payload_ht_<0)
    throw std::runtime_error("IOM " + name + "couldn't create HDF5 ldo_payload_ht H5 datatype");

  ldo_payload_hs_ = H5Screate(H5S_SCALAR);
  if(ldo_payload_hs_<0)
    throw std::runtime_error("IOM " + name + "couldn't create HDF5 ldo_payload_hs H5 dataspace");

  ldo_attr_space_ = H5Screate(H5S_SCALAR);
  if(ldo_attr_space_<0)
    throw std::runtime_error("IOM " + name + "couldn't create HDF5 ldo_attr_space dataspace");

}

IomHDF5::~IomHDF5() {
  H5Tclose(ldo_payload_ht_);
  H5Fclose(hfile_);
  H5Sclose(ldo_attr_space_);
  H5Sclose(ldo_payload_hs_);
}


rc_t IomHDF5::WriteObject(faodel::bucket_t bucket, const Key &key, const lunasa::DataObject &ldo) {
  try {
    internal_WriteObject(bucket, {kvpair(key, ldo)});
    return 0;
  }
  catch (std::runtime_error &e) {
    return 1;
  }
}

void IomHDF5::internal_WriteObject(faodel::bucket_t bucket,
                                   const std::vector<kvpair> &kvpairs) {
  hid_t ldo_dset;
  hvl_t dset_descriptor;

  // First create the group (~= bucket) if it's not there already
  std::string grp_name = "/" + bucket.GetHex();
  if(H5Lexists(hfile_, grp_name.c_str(), H5P_DEFAULT)<=0) {
    /* The group wasn't found, so we have to create it before we move on */
    hid_t gid = H5Gcreate(hfile_, grp_name.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if(gid<0) {
      throw std::runtime_error("IomHDF5::WriteObjects couldn't create group " + grp_name);
    }
    H5Gclose(gid);
  }

  for(auto &&kv : kvpairs) {
    const kelpie::Key &key = kv.first;
    const lunasa::DataObject &ldo = kv.second;

    /*
     * Create the dataset (blob for the LDO) if it doesn't already exist, else open it
     * Write the LDO data into the dataset
     * Store LDO metadata as attributes on the dataset
     */

    std::string dset_name = grp_name + "/" + key.str();
    if(H5Lexists(hfile_, dset_name.c_str(), H5P_DEFAULT)<=0) {
      ldo_dset = H5Dcreate(hfile_,
                           dset_name.c_str(), // dataset name
                           ldo_payload_ht_, // HDF5 type ID
                           ldo_payload_hs_, // HDF5 space ID
                           H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      if(ldo_dset<0) {
        throw std::runtime_error("IomHDF5::WriteObjects can't create dataset " + dset_name);
      }
    } else {
      ldo_dset = H5Dopen(hfile_, dset_name.c_str(), H5P_DEFAULT);
      if(ldo_dset<0) {
        throw std::runtime_error("IomHDF5::WriteObjects can't open dataset " + dset_name);
      }
    }

    // HDF5 wants a descriptor for variable-length data
    dset_descriptor.len = ldo.GetUserSize();
    dset_descriptor.p = ldo.GetMetaPtr();

    // Write the dataset
    H5Dwrite(ldo_dset, ldo_payload_ht_, H5S_ALL, H5S_ALL, H5P_DEFAULT, &dset_descriptor);
    stat_wr_requests++;
    stat_wr_bytes += dset_descriptor.len;

    // Store the LDO meta information as attributes
    hid_t attr_id;
    unsigned short int ldo_type = ldo.GetTypeID();
    attr_id = H5Acreate(ldo_dset, "ldo-type", H5T_NATIVE_USHORT, ldo_attr_space_, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, H5T_NATIVE_USHORT, &ldo_type);
    H5Aclose(attr_id);
    stat_wr_bytes += sizeof ldo_type;

    unsigned short int ldo_meta_size = ldo.GetMetaSize();
    attr_id = H5Acreate(ldo_dset, "ldo-meta-size", H5T_NATIVE_USHORT, ldo_attr_space_, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, H5T_NATIVE_USHORT, &ldo_meta_size);
    H5Aclose(attr_id);
    stat_wr_bytes += sizeof ldo_meta_size;

    unsigned long int ldo_data_size = ldo.GetDataSize();
    attr_id = H5Acreate(ldo_dset, "ldo-data-size", H5T_NATIVE_ULONG, ldo_attr_space_, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr_id, H5T_NATIVE_USHORT, &ldo_data_size);
    H5Aclose(attr_id);
    stat_wr_bytes += sizeof ldo_data_size;

    H5Dclose(ldo_dset);
  }
}


//rc_t IomHDF5::ReadObjects(faodel::bucket_t bucket,
//                     std::vector<kelpie::Key> &keys,
//                     std::vector<kvpair> *found_keys,
//                     std::vector<kelpie::Key> *missing_keys) {
//  for(auto &&k : keys) {
//    lunasa::DataObject ldo;
//    rc_t each_rc = ReadObject(bucket, k, &ldo);
//    if(each_rc == KELPIE_OK) {
//      found_keys->push_back({kvpair(k, ldo)});
//    } else {
//      missing_keys->push_back(k);
//    }
//  }
//
//  return KELPIE_OK;
//}

rc_t IomHDF5::ReadObject(faodel::bucket_t bucket,
                    const kelpie::Key &key,
                    lunasa::DataObject *ldo) {
  rc_t rc = KELPIE_OK;
  std::string dset_name(bucket.GetHex() + '/' + key.str());
  auto ldo_dset = H5Dopen(hfile_, dset_name.c_str(), H5P_DEFAULT);
  if(ldo_dset>=0) {
    hvl_t dset_descriptor;
    auto read_rc = H5Dread(ldo_dset, ldo_payload_ht_, H5S_ALL, H5S_ALL, H5P_DEFAULT, &dset_descriptor);
    stat_rd_requests++;
    stat_rd_bytes += dset_descriptor.len;

    // payload is now pointed to by the dset_descriptor

    // We need to pull the attributes off the dataset so we can create the LDO
    unsigned short int ldo_type;
    unsigned short int ldo_meta_size;
    unsigned long int ldo_data_size;

    auto meta_attr_id = H5Aopen_by_name(ldo_dset, ".", "ldo-meta-size", H5P_DEFAULT, H5P_DEFAULT);
    if(meta_attr_id<0)
      throw std::runtime_error("IomHDF5: group_visit_cb can't get ldo-meta-size attribute using key " + dset_name);
    auto data_attr_id = H5Aopen_by_name(ldo_dset, ".", "ldo-data-size", H5P_DEFAULT, H5P_DEFAULT);
    if(data_attr_id<0)
      throw std::runtime_error("IomHDF5: group_visit_cb can't get ldo-data-size attribute using key " + dset_name);
    auto type_attr_id = H5Aopen_by_name(ldo_dset, ".", "ldo-type", H5P_DEFAULT, H5P_DEFAULT);
    if(type_attr_id<0)
      throw std::runtime_error("IomHDF5 can't read ldo-type attribute from dset " + dset_name);

    H5Aread(meta_attr_id, H5T_NATIVE_USHORT, &ldo_meta_size);
    H5Aread(data_attr_id, H5T_NATIVE_ULONG, &ldo_data_size);
    H5Aread(type_attr_id, H5T_NATIVE_USHORT, &ldo_type);
    stat_rd_bytes += sizeof ldo_meta_size + sizeof ldo_data_size + sizeof ldo_type;

    *ldo = lunasa::DataObject(ldo_meta_size, ldo_data_size, lunasa::DataObject::AllocatorType::eager);
    std::memcpy(ldo->GetMetaPtr(), dset_descriptor.p, dset_descriptor.len);
    ldo->SetTypeID(ldo_type);

    H5Dvlen_reclaim(ldo_payload_ht_, ldo_payload_hs_, H5P_DEFAULT, &dset_descriptor);
    H5Aclose(meta_attr_id);
    H5Aclose(data_attr_id);
    H5Aclose(type_attr_id);
    H5Dclose(ldo_dset);

  } else {
    rc = KELPIE_ENOENT;
  }

  return rc;
}

void IomHDF5::sstr(std::stringstream &ss, int depth, int index) const {
  ss << std::string(index, ' ') + "IomHDF5 path: " << path_ << std::endl;
}

/*
 * A visit-function for use in iterating buckets stored in the HDF structure
 */
struct group_visit_cb_data {
  std::vector<std::string> *link_map_p;
  const std::string *reference_link_p;
  std::string *name_p;
};

herr_t group_visit_cb(hid_t group_id, const char *name, const H5L_info_t *info, void *op_data) {
  /*
   * the name parameter should point to the "name" of the object being visited
   * relative to the current group. Since for this purpose the current group is the
   * root group, the name should be the whole "name" of each dataset (dataset == LDO).
   * These names are constructed (cf. WriteObject above) as bucket.GetHex + '/' + key.str().
   * So finding the '/' should section the bucket hex value, which is what we need here.
   */
  std::string bucket_hex(name, std::strpbrk(name, "/") - name);

  auto grp_data = reinterpret_cast< group_visit_cb_data * > (op_data);
  grp_data->link_map_p->push_back("<a href=\"" +
                                  *(grp_data->reference_link_p) +
                                  "&details=true&iom_name=" +
                                  *(grp_data->name_p) +
                                  "&bucket=" + bucket_hex +
                                  "\">" + bucket_hex +
                                  "</a>");
  return 0; // indicates that the iteration can continue
}

/*
 * a visit-function for iterating the contents of a bucket stored in the HDF structure
 */
struct ldo_visit_cb_data {
  std::vector<std::pair<std::string, std::string> > *blobs_p;
};

herr_t ldo_visit_cb(hid_t group_id, const char *name, const H5L_info_t *info, void *op_data) {
  /*
   * Here we have to not only name each LDO we encounter, but we have to get its size as well.
   * We have that information as attributes on the dataset containing each LDO.
   */
  unsigned short int ldo_meta_size;
  unsigned long int ldo_data_size;

  auto meta_attr_id = H5Aopen_by_name(group_id, name, "ldo-meta-size", H5P_DEFAULT, H5P_DEFAULT);
  if(meta_attr_id<0)
    std::cerr << "IomHDF5: group_visit_cb can't get ldo-meta-size attribute using key " << name << std::endl;
  auto data_attr_id = H5Aopen_by_name(group_id, name, "ldo-data-size", H5P_DEFAULT, H5P_DEFAULT);
  if(data_attr_id<0)
    std::cerr << "IomHDF5: group_visit_cb can't get ldo-data-size attribute using key " << name << std::endl;

  H5Aread(meta_attr_id, H5T_NATIVE_USHORT, &ldo_meta_size);
  H5Aread(data_attr_id, H5T_NATIVE_ULONG, &ldo_data_size);

  auto ldo_visit_data = reinterpret_cast< ldo_visit_cb_data * >( op_data );
  ldo_visit_data->blobs_p->push_back(std::make_pair(name, std::to_string(ldo_meta_size + ldo_data_size)));

  H5Aclose(meta_attr_id);
  H5Aclose(data_attr_id);

  return 0;
}


void IomHDF5::AppendWebInfo(faodel::ReplyStream rs,
                       const std::string reference_link,
                       const std::map<std::string, std::string> &args) {
  std::vector<std::vector<std::string> > items =
          {
                  {"Setting", "Value"},
                  {"Name",    name},
                  {"Path",    path_},
          };

  rs.mkTable(items, "Basic Information");
  rs.tableBegin("Initial Configuration Parameters");
  rs.tableTop({"Setting", "Value"});
  for(auto &&nvpair : settings) {
    rs.tableRow({nvpair.first, nvpair.second});
  }
  rs.tableEnd();

  hsize_t idx;
  auto ai = args.find("details");
  if(ai not_eq args.end() and ai->second == "true") {
    auto ai2 = args.find("bucket");
    if(ai2 == args.end() or ai2->second.empty()) {
      /*
       * no bucket given
       * we have to iterate every dataset in the file
       */
      std::vector<std::string> links;
      hid_t group_id = H5Gopen(hfile_, "/", H5P_DEFAULT); // open the root
      if(group_id<0) {
        std::cerr << "IomHDF5::AppendWebInfo can't open root HDF group" << std::endl;
        return;
      }

      group_visit_cb_data cbdata;
      cbdata.link_map_p = &links;
      cbdata.reference_link_p = &reference_link;
      cbdata.name_p = &name;

      // This iterates the whole group, calling the callback function for each entry
      // When this returns, we'll have the entire set of links inserted into links
      H5Literate(group_id, H5_INDEX_CRT_ORDER, H5_ITER_NATIVE, &idx, group_visit_cb, &cbdata);

      rs.mkList(links, "On-disk buckets");
      H5Gclose(group_id);

    } else {

      /*
       * We were given a bucket, so we need to grab everything in the group belonging to this bucket.
       * Here we can get a little benefit from the way HDF names things. They use '/' as a hierarchy separator
       * much as POSIX filesystems do. So by separating the bucket hex from the key string using /, we implicitly
       * create a subtree in the HDF hierarchy space rooted at a node corresponding to the bucket hex string.
       * The upshot is that we can use the bucket hex to open this group directly.
       */

      hid_t group_id = H5Gopen(hfile_, ("/" + ai2->second).c_str(), H5P_DEFAULT);
      if(group_id<0) {
        std::cerr << "IomHDF5::AppendWebInfo can't open bucket group " + ai2->second << std::endl;
        return;
      }

      std::vector<std::pair<std::string, std::string> > blobs;
      blobs.push_back(std::make_pair("Key", "Size"));
      ldo_visit_cb_data cbdata;
      cbdata.blobs_p = &blobs;
      H5Literate(group_id, H5_INDEX_CRT_ORDER, H5_ITER_NATIVE, &idx, ldo_visit_cb, &cbdata);

      rs.mkTable(blobs, "Objets d'Bucket " + ai2->second);
      H5Gclose(group_id);
    }
  }
}


rc_t IomHDF5::GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, object_info_t *info) {
  rc_t rc;

  if(info) info->Wipe();

  std::string target = "/" + bucket.GetHex() + "/" + key.str();

  auto meta_attr_id = H5Aopen_by_name(hfile_, target.c_str(), "ldo-meta-size", H5P_DEFAULT, H5P_DEFAULT);
  auto data_attr_id = H5Aopen_by_name(hfile_, target.c_str(), "ldo-data-size", H5P_DEFAULT, H5P_DEFAULT);

  if(meta_attr_id<0 || data_attr_id<0) {

    if(info not_eq nullptr) {
      info->col_availability = kelpie::Availability::Unavailable;
    }
    rc = KELPIE_ENOENT;

  } else {

    unsigned short int ldo_meta_size;
    unsigned long int ldo_data_size;

    H5Aread(meta_attr_id, H5T_NATIVE_USHORT, &ldo_meta_size);
    H5Aread(data_attr_id, H5T_NATIVE_ULONG, &ldo_data_size);

    if(info) {
      info->col_user_bytes = ldo_meta_size + ldo_data_size;
      info->col_availability = kelpie::Availability::InDisk;
    }
    rc = KELPIE_OK;

  }

  H5Aclose(meta_attr_id);
  H5Aclose(data_attr_id);

  return rc;

}


} // namespace internal
} // namespace kelpie





	
			      



      
      
      
