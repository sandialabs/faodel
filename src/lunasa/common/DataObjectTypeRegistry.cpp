// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

#include "lunasa/common/DataObjectTypeRegistry.hh"


using namespace std;

namespace lunasa {


DataObjectTypeRegistry::DataObjectTypeRegistry() {
  mutex = faodel::GenerateMutex();
}

/**
 * @brief Function for registering a new DataObject dump function
 * @param type_id The ID for the DataObject type (const_hash16(name))
 * @param name A string label for the function
 * @param dump_func Function for updating ReplyStream
 * @throw runtime_error if function is a handler has already registered a function for this type_id
 */
void DataObjectTypeRegistry::RegisterDataObjectType(dataobject_type_t type_id, string name, lunasa::fn_DataObjectDump_t dump_func) {
  mutex->WriterLock();
  auto search = ldo_names.find(type_id);
  if(search != ldo_names.end()) {
    if(search->second == name) {
      mutex->Unlock();
      throw std::runtime_error("Lunasa Data Object Type: collision between names '" + name + "' and '" + search->second + "'");
    }
  }
  ldo_names[type_id] = name;
  ldo_dump_functions[type_id] = dump_func;
  mutex->Unlock();
}

/**
 * @brief Deregister a dump function registered to a particular type_id
 * @param type_id 
 */
void DataObjectTypeRegistry::DeregisterDataObjectType(dataobject_type_t type_id) {
  mutex->WriterLock();
  ldo_names.erase(type_id);
  ldo_dump_functions.erase(type_id);
  mutex->Unlock();
}

namespace internal {

/**
 * @brief Internal function for dumping a generic LDO
 * @param ldo The input DataObject that needs dumping
 * @param rs The ReplyStream to append
 */
void fn_hexdump_dataobject(const DataObject &ldo, faodel::ReplyStream &rs) {

  const int chars_per_line=32;

  uint16_t msize = ldo.GetMetaSize();
  uint32_t dsize = ldo.GetDataSize();

  rs.mkSection("Data Object Dump");

  if(msize > 0) {
    vector<string> offset_lines, hex_lines, txt_lines;

    ssize_t size = (msize < 256) ? msize : 256;
    faodel::ConvertToHexDump(ldo.GetMetaPtr<char *>(), size,
                             chars_per_line, 8,
                             "<span class=\"HEXE\">", "</span>",
                             "<span class=\"HEXO\">", "</span>",
                             &offset_lines, &hex_lines, &txt_lines);

    vector<vector<string>> rows;
    rows.push_back({"Offset","Hex Data","Text"});
    for(size_t i=0; i<offset_lines.size(); i++){
      vector<string> row;
      row.push_back(offset_lines[i]);
      row.push_back(hex_lines[i]);
      row.push_back(txt_lines[i]);
      rows.push_back(row);
    }
    rs.mkTable(rows, "Meta Section");
  }

  if(dsize > 0) {
    vector<string> offset_lines, hex_lines, txt_lines;

    ssize_t size = (dsize < 2048) ? dsize : 2048;
    faodel::ConvertToHexDump(ldo.GetDataPtr<char *>(), size,
                             chars_per_line, 8,
                             "<span class=\"HEXE\">", "</span>",
                             "<span class=\"HEXO\">", "</span>",
                             &offset_lines, &hex_lines, &txt_lines);

    vector<vector<string>> rows;
    rows.push_back({"Offset","Hex Data","Text"});
    for(size_t i=0; i<offset_lines.size(); i++){
      vector<string> row;
      row.push_back(offset_lines[i]);
      row.push_back(hex_lines[i]);
      row.push_back(txt_lines[i]);
      rows.push_back(row);
    }
    rs.mkTable(rows, "Data Section");

  }
}
} // namespace internal

/**
 * @brief Dump an LDO to a ReplyStream. Use a user-supplied dump function if Type ID is known
 * @param ldo The DataObject to dump
 * @param rs The ReplyStream to append
 * @retval TRUE A custom format function was known and used
 * @retval FALSE The ID was not known. Default LDO dumper used.
 */
bool DataObjectTypeRegistry::DumpDataObject(const DataObject &ldo, faodel::ReplyStream &rs) {

  bool found=false;
  fn_DataObjectDump_t dump_func;

  mutex->ReaderLock();
  dataobject_type_t tag = ldo.GetTypeID();
  auto search = ldo_dump_functions.find(tag);
  if(search == ldo_dump_functions.end()) {
    //Didn't find, use default dump function
    dump_func = internal::fn_hexdump_dataobject;
  } else {
    dump_func = search->second;
    found=true;
  }
  mutex->Unlock();

  dump_func(ldo, rs);
  return found;
}

/**
 * @brief Whookie function for dumping out information about the registry to a replystream
 * @param rs The ReplyStream to append
 * @note This does not finish the reply stream
 */
void DataObjectTypeRegistry::DumpRegistryStatus(faodel::ReplyStream &rs) {

  rs.tableBegin("Lunasa DataObject Type Registry");
  rs.tableTop({"Type Name", "Type ID"});

  mutex->Lock();
  for(auto &kv : ldo_names) {
    stringstream ss_hex;
    ss_hex<<"0x"<<std::setw(4)<<kv.first;
    rs.tableRow({ kv.second, ss_hex.str()});
  }
  mutex->Unlock();

  rs.tableEnd();
}

void DataObjectTypeRegistry::sstr(std::stringstream &ss, int depth, int indent) const {
  if(depth<0) return;
  ss << string(indent,' ') << "[DataObjectTypeRegistry]"
     << " KnownTypes: "<< ldo_dump_functions.size()
     << endl;
  if(depth>0){
    indent++;

    mutex->Lock();
    for(auto x : ldo_names){
      ss<< string(indent,' ') << "["<<hex<<x.first<<"] "<<x.second<<endl;
    }
    mutex->Unlock();
  }
}




} // namespace lunasa
