// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <string>
#include <map>
#include <vector>
#include <iostream>

#include "kelpie/ioms/IomLevelDB.hh"

#include "leveldb/write_batch.h"

namespace kelpie {
namespace internal {

constexpr char IomLevelDB::type_str[];

typedef struct {
  uint16_t ldo_type;
  uint16_t ldo_meta_size;
  uint32_t ldo_data_size;
} ldo_info_struct;


IomLevelDB::IomLevelDB(const std::string &name, const std::map<std::string, std::string> &new_settings)
        : IomBase(name, new_settings, {"path", "unique"}) {
  leveldb_opts_.create_if_missing = true;

  auto pi = settings.find("path");
  if(pi == settings.end()) {
    throw std::runtime_error("IOM " + name + " was not given a setting for 'path'");
  }
  path_ = pi->second +
          '/'; // no need to check; duplicate separators in path names ignored on POSIX-compliant filesystems

  pi = settings.find("unique");
  if(pi != settings.end()) {
    path_ += pi->second + '/';
  }

}


IomLevelDB::~IomLevelDB() {
  for(auto &&mdb : bmap_) {
    delete mdb.second;
  }
}

leveldb::DB * IomLevelDB::bucketToDB(faodel::bucket_t &bkt) {
  leveldb::DB *db;

  auto found = bmap_.find(bkt.GetHex());

  if(found == bmap_.end()) {
    // open a new levelDB for this bucket
    leveldb::Status rc = leveldb::DB::Open(leveldb_opts_, path_ + bkt.GetHex(), &db);
    if(not rc.ok()) {
      throw std::runtime_error("LevelDB open failed: " + rc.ToString());
    }
    bmap_[bkt.GetHex()] = db;
  } else {
    // We found it because we already opened it, return it
    db = found->second;
  }

  return db;
}


void IomLevelDB::WriteObject(faodel::bucket_t bucket,
                        const kelpie::Key &key,
                        const lunasa::DataObject &ldo) {
  WriteObjects(bucket, {kvpair(key, ldo)});
}

void IomLevelDB::WriteObjects(faodel::bucket_t bucket,
                         const std::vector<kvpair> &kvpairs) {
  leveldb::WriteBatch batch;
  size_t wr_amt = 0;

  leveldb::DB *db = bucketToDB(bucket);

  for(auto &&kv : kvpairs) {
    const kelpie::Key &key = kv.first;
    const lunasa::DataObject &ldo = kv.second;
    const std::string &base = key.str();

    // Naive packing
    // We don't have access to the LDO header ptr so we have to dance a little bit
    // Maybe DataObject should have a packForTransport() or similar to handle this more efficiently
    ldo_info_struct lis;
    lis.ldo_type = ldo.GetTypeID();
    lis.ldo_meta_size = ldo.GetMetaSize();
    lis.ldo_data_size = ldo.GetDataSize();
    leveldb::Slice ldo_slice(reinterpret_cast< const char * >( ldo.GetMetaPtr()), ldo.GetUserSize());
    leveldb::Slice ldo_info_slice(reinterpret_cast< const char * >( &lis ), sizeof lis);

    batch.Put(base + ".buffer", ldo_slice);
    batch.Put(base + ".info", ldo_info_slice);
    wr_amt += ldo_slice.size() + ldo_info_slice.size();
  }

  leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
  if(not status.ok()) {
    std::cerr << "leveldb write failed: " << status.ToString() << std::endl;
  } else {
    stat_wr_requests += kvpairs.size();
    stat_wr_bytes += wr_amt;
  }

}

rc_t IomLevelDB::ReadObject(faodel::bucket_t bucket,
                       const kelpie::Key &key,
                       lunasa::DataObject *ldo) {
  rc_t rc = KELPIE_OK;
  std::string buf, info;
  ldo_info_struct lis;
  leveldb::Status s1, s2;

  leveldb::DB *db = bucketToDB(bucket);

  s1 = db->Get(leveldb::ReadOptions(), key.str() + ".buffer", &buf);
  s2 = db->Get(leveldb::ReadOptions(), key.str() + ".info", &info);
  if(s1.ok() && s2.ok()) {
    const ldo_info_struct *lis = reinterpret_cast< const ldo_info_struct * >( info.data());
    *ldo = lunasa::DataObject(lis->ldo_meta_size, lis->ldo_data_size, lunasa::DataObject::AllocatorType::eager);
    std::memcpy(ldo->GetMetaPtr(), buf.data(), buf.size());
    ldo->SetTypeID(lis->ldo_type);
    stat_rd_requests++;
    stat_rd_bytes += sizeof lis + buf.size();
  } else {
    rc = KELPIE_ENOENT;
  }

  return rc;
}

rc_t IomLevelDB::ReadObjects(faodel::bucket_t bucket,
                        std::vector<kelpie::Key> &keys,
                        std::vector<kvpair> *found_keys,
                        std::vector<kelpie::Key> *missing_keys) {
  for(auto &&k : keys) {
    lunasa::DataObject ldo;
    rc_t each_rc = ReadObject(bucket, k, &ldo);
    if(each_rc == KELPIE_OK) {
      found_keys->push_back({kvpair(k, ldo)});
    } else {
      missing_keys->push_back(k);
    }
  }

  return KELPIE_OK;
}

void IomLevelDB::sstr(std::stringstream &ss, int depth, int index) const {
  ss << std::string(index, ' ') + "IomLevelDB Path: " << path_ << std::endl;
}

void IomLevelDB::AppendWebInfo(faodel::ReplyStream rs,
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

  auto ai = args.find("details");
  if(ai not_eq args.end() and ai->second == "true") {
    auto ai2 = args.find("bucket");
    if(ai2 == args.end() or ai2->second.empty()) {

      // we were not given a bucket, list them all
      std::vector<std::string> links;
      // Get the list of buckets
      for(auto &&bmpair : bmap_) {
        links.push_back("<a href=\"" +
                        reference_link +
                        "&details=true&iom_name=" +
                        name +
                        "&bucket=" + bmpair.first +
                        "\">" + bmpair.first +
                        "</a>");
      }
      rs.mkList(links, "On-disk buckets");

    } else {

      // iterate the DB belonging to the bucket
      const std::string &bucket = ai2->second;
      std::vector<std::pair<std::string, std::string> > blobs;
      leveldb::DB *db = bmap_[bucket];
      leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
      blobs.push_back(std::make_pair("Key", "Size"));
      for(it->SeekToFirst(); it->Valid(); it->Next()) {
        blobs.push_back(std::make_pair(it->key().ToString(), std::to_string(it->value().size())));
      }
      rs.mkTable(blobs, "Objects in Bucket " + bucket);

    }
  }
}


rc_t IomLevelDB::GetInfo(faodel::bucket_t bucket, const kelpie::Key &key, kv_col_info_t *col_info) {
  rc_t rc;
  std::string val;

  if(col_info not_eq nullptr)
    col_info->Wipe();

  leveldb::DB *db = bucketToDB(bucket);
  leveldb::Status s = db->Get(leveldb::ReadOptions(), key.str(), &val);
  if(s.ok()) {
    if(col_info not_eq nullptr) {
      col_info->num_bytes = val.size();
      col_info->availability = kelpie::Availability::InDisk;
    }
    rc = KELPIE_OK;
  } else {
    if(col_info not_eq nullptr) {
      col_info->availability = kelpie::Availability::Unavailable;
    }
    rc = KELPIE_ENOENT;
  }

  return rc;
}


} // namespace internal
} // namespace kelpie

    
	

    
