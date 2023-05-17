// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "ArrowDataObject.hh"

#include "faodel-common/StringHelpers.hh"
#include <iostream>

#include <arrow/buffer.h>
#include <arrow/ipc/api.h>
#include <arrow/filesystem/filesystem.h>
#include <parquet/arrow/writer.h>

#include "faodel-common/Debug.hh"
#include "lunasa/Lunasa.hh"


using namespace std;


namespace faodel {

const string ArrowDataObject::object_type_name = "ArrowRecordBatch";
const uint16_t ArrowDataObject::object_type_id = faodel::const_hash16("ArrowRecordBatch");


/// @brief Constructor for creating an empty FADO that can hold max_capacity bytes
/// @param max_arrow_capacity Space available for serialized data. Includes chunk overheads and serialized IPC data, but NOT the fado_meta_t size
ArrowDataObject::ArrowDataObject(uint32_t max_arrow_capacity) {
  ldo = lunasa::DataObject(sizeof(fado_meta_t) + max_arrow_capacity, //Include meta here, so caller doesn't think about it
                           sizeof(fado_meta_t),
                           0,
                           lunasa::DataObject::AllocatorType::eager,
                           object_type_id);
  auto *meta = ldo.GetMetaPtr<fado_meta_t *>();
  meta->wipe(); //Make sure we start with no items
}

/// @prief Wrap a FADO Class around an LDO that was previously used to hold FADO data
/// @param import_ldo Existing ldo to use
/// @note This checks the ldo's type id field. If it isn't support's type, it rejects it
ArrowDataObject::ArrowDataObject(lunasa::DataObject import_ldo) : ldo(import_ldo) {
  if(!Valid()) {
    ldo=lunasa::DataObject();
    return;
  }
}

/// @brief Construct a FADO object that contains the given table (with no additional capacity for data)
/// @param table Arrow table to be serialized
/// @param codec Arrow Compression codec to use
/// @note May error out if unable to allocate space
ArrowDataObject::ArrowDataObject(const std::shared_ptr<arrow::Table> &table,
                                 arrow::Compression::type codec) {
  // Set the compression options
  auto options = arrow::ipc::IpcWriteOptions::Defaults();
  options.codec = arrow::util::Codec::Create(codec).ValueOrDie();

  //Run serialization w/ null out to get the packed size of the table
  int64_t table_size_in_bytes = GetSerializedTableSize(table, options).ValueOrDie();

  // Allocate an LDO big enough to hold this
  ldo = lunasa::DataObject(
      sizeof(fado_meta_t) + sizeof(fado_chunk_t) + table_size_in_bytes,
      sizeof(fado_meta_t),
      0,  // append will take care of this
      lunasa::DataObject::AllocatorType::eager, object_type_id);

  // Wipe out the meta so we start at the beginning
  auto *meta = ldo.GetMetaPtr<fado_meta_t *>();
  meta->wipe();  // Make sure we start with no items

  // Serialize data directly into the LDO
  auto status = doAppendTable(table, table_size_in_bytes, options);
  if (!status.ok()) ldo = lunasa::DataObject();  // Error: bail out
}

/// @brief Safety check that verifies we have an LDO allocation and the LDO's type id matches FADO
bool ArrowDataObject::Valid() const {
  if(ldo.isNull()) return false;
  return (ldo.GetTypeID() == object_type_id);
}

/// @brief Inspects a chunk id and determines if it's in the range of valid chunks for this object
bool ArrowDataObject::ValidChunk(int chunk_id) const {
   return ( (!ldo.isNull()) &&
            (ldo.GetTypeID() == object_type_id) &&
            (chunk_id>=0) &&
            (chunk_id<NumberOfTables()));
}

/// @brief Round a size up to the next 32b value
uint32_t ArrowDataObject::roundupChunkSize(uint32_t size) const {
  if(size & 0x03) {
    size = ((size & ~0x03) + 4); //Round up to 32b words
  }
  return size;
}

/// @brief Serialize and append an Apache Arrow table, if it fits in the available capacity
/// @param table Arrow table to be serialized
/// @param codec Arrow Compression codec to use
/// @retval Invalid - ADO not initialized with a capacity
/// @retval OutOfMemory - Not enough capacity to hold new table
/// @retval Ok - Added table
arrow::Status ArrowDataObject::Append(const std::shared_ptr<arrow::Table>& table,
                                      arrow::Compression::type codec) {
  if (!Valid())
    return arrow::Status::Invalid(
        "Attempted to append invalid Faodel ArrowDataObject");

  // Set the compression options
  auto options = arrow::ipc::IpcWriteOptions::Defaults();
  ARROW_ASSIGN_OR_RAISE(options.codec, arrow::util::Codec::Create(codec));

  ARROW_ASSIGN_OR_RAISE(auto table_size_in_bytes,
                        GetSerializedTableSize(table, options));

  ARROW_RETURN_NOT_OK(doAppendTable(table, table_size_in_bytes, options));
  return arrow::Status::OK();
}

/// @brief Copy all the chunks from one FADO into another, as space permits
/// @param src_fado An existing object to read chunks from
/// @retval Invalid - ADO not initialized with a capacity
/// @retval OutOfMemory - Not enough capacity to hold new tables
/// @retval Ok - Added tables (or skipped if empty)
/// @note This blindly copies serialized tables (ie chunks) from one FADO to another. No Schema checks are made
arrow::Status ArrowDataObject::Append(ArrowDataObject src_fado) {

   if(!src_fado.Valid()) return arrow::Status::OK(); //Skip empty FADOs

   int src_tables = src_fado.NumberOfTables();
   if(src_tables==0) return arrow::Status::OK(); //No work

   auto src_ldo = src_fado.ExportDataObject();

   int rc = doAppendChunkStrip(src_ldo.GetDataPtr<fado_chunk_t *>(),
                               src_ldo.GetDataSize(),
                               src_tables);
   switch(rc){
      case 0:      return arrow::Status::OK(); break;
      case ENOMEM: return arrow::Status::OutOfMemory("Not enough capacity to append existing ArrowDataObject"); break;
      case EINVAL: return arrow::Status::Invalid("Source ArrowDataObject did not have data to copy"); break;
      default: ;
   }
   return arrow::Status::UnknownError("ArrowDataObject Append received an unknown error condition");
}

/// @brief Wipe all book keeping from this support so it can be used for a new purpose
/// @param iot Internal Use Only. This could be dangerous
/// @param zero_out_data When true, also zero out the data in the ldo
void ArrowDataObject::Wipe(faodel::internal_use_only_t iot, bool zero_out_data) {
  if(zero_out_data) {
    memset(ldo.GetDataPtr(), 0, ldo.GetDataSize());
  }

  ldo.GetMetaPtr<fado_meta_t *>()->wipe();
  ldo.ModifyUserSizes(ldo.GetMetaSize(), 0);
}

/// @brief Find the starting address of the specified chunk
/// @param chunk_id Which serialized table chunk to access
/// @return Pointer to chunk, or nullptr on error
ArrowDataObject::fado_chunk_t * ArrowDataObject::locateChunk(int chunk_id) const {
  if((chunk_id < 0) || (chunk_id >= ldo.GetMetaPtr<fado_meta_t *>()->num_chunks)) {
    return nullptr;
  }
  auto *ptr = ldo.GetDataPtr<char *>();
  auto *chunk = reinterpret_cast<fado_chunk_t*>(ptr);
  for(int i=0; i < chunk_id; i++) {
    ptr += roundupChunkSize(sizeof(fado_chunk_t) + chunk->data_length);
    chunk = reinterpret_cast<fado_chunk_t*>(ptr);
    //cout<<"Chunk "<<i<<" is at "<<hex<<((uint64_t) chunk) << " and length "<<chunk->data_length<<endl;
  }
  return chunk;
}

/// @brief Report the Object Status value stored in the metadata portion of this object
/// @return user-defined value
/// @note This is intended to be used as a way to pass a return code when a filtering operation happens on a query
uint32_t ArrowDataObject::GetObjectStatus() const {
   if(!Valid()) return 0;
   return ldo.GetMetaPtr<fado_meta_t *>()->object_status;
}

/// @brief Set the Object Status value stored in the metadata portion of this object
void ArrowDataObject::SetObjectStatus(uint32_t status) {
   if(!Valid()) return;
   ldo.GetMetaPtr<fado_meta_t *>()->object_status = status;
}

/// @brief Report the number of tables currently housed in this object
/// @return Number of tables (ie, chunks)
int ArrowDataObject::NumberOfTables() const {
  if(!Valid()) return 0;
  return ldo.GetMetaPtr<fado_meta_t *>()->num_chunks;
}

/// @brief Report the number of rows currently housed in this object
/// @return Number of rows
uint64_t ArrowDataObject::NumberOfRows() const {
   if(!Valid()) return 0;
   int num_chunks = NumberOfTables();

   auto *ptr = ldo.GetDataPtr<char *>();
   auto *chunk = reinterpret_cast<fado_chunk_t*>(ptr);

   uint64_t num_rows=0;
   for(int i=0; i<num_chunks; i++) {
     validChunkOrDie(chunk, i, "NumberOfRows");
     num_rows += chunk->num_rows;
     ptr += roundupChunkSize(sizeof(fado_chunk_t) + chunk->data_length);
     chunk = reinterpret_cast<fado_chunk_t*>(ptr);
   }

   return num_rows;
}

/// @brief Revive one of the tables serialized in this object
/// @param chunk_id The table to retrieve
/// @param options Any additional Arrow read options
/// @return The table (or dies!)
/// @note This makes a shared buffer out of the serialized data. Do not destroy the FADO while table is valid
arrow::Result<std::shared_ptr<arrow::Table>> ArrowDataObject::ExtractTable(int chunk_id, arrow::ipc::IpcReadOptions options) const {

  if(!ValidChunk(chunk_id))
     return arrow::Status::IndexError("chunk id outside of range of this Faodel ArrowDataObject");

  auto *chunk = locateChunk(chunk_id);
  if((!chunk) || (!chunk->valid()))
     return arrow::Status::IndexError("Could not locate a valid chunk in this Faodel ArrowDataObject");


  #if 1
    //Normal version: create a buffer based on the pointers to the real data
    auto buf = std::make_shared<arrow::Buffer>((const uint8_t *)chunk->data, chunk->data_length);
  #else
    //Extra copy: This version allocates a side buffer for unpacking the data. Shouldn't be used- included for paranoia
    std::shared_ptr<faodel-arrow::Buffer> buf = *faodel-arrow::AllocateBuffer(chunk->data_length); //Normally a unique ptr
    memcpy(buf->mutable_data(), chunk->data, chunk->data_length);
  #endif

  auto buffer_reader = std::make_shared<arrow::io::BufferReader>(buf);

  options.use_threads = true;
  ARROW_ASSIGN_OR_RAISE(auto reader,
                        arrow::ipc::RecordBatchStreamReader::Open(buffer_reader, options));

  return reader->ToTable();

}

/// @brief Walk through all chunks and make sure each one has a non-zero length
bool ArrowDataObject::dbgAllChunksValid() const {
   if(!Valid()) return false;
   int num_bundles = ldo.GetMetaPtr<fado_meta_t *>()->num_chunks;
   for(int i=0; i<num_bundles; i++) {
      auto *chunk = locateChunk(i);
      if((chunk->data_length==0) || (!chunk->valid())) return false;
   }
   return true;
}

/// @brief Get a string containing internal info for this object
/// @param show_details When true, append the one line status with additional details,
/// @return String containing info
std::string ArrowDataObject::str(bool show_details) const {
   stringstream ss;
   ss<<"support : NumTables:" << NumberOfTables() <<" Valid: "<<Valid()<<" UtilizationRatio: "<<CurrentUtilizationRatio()<<endl;
   if(show_details) {
      auto *data = ldo.GetDataPtr<char *>();
      int num_bundles = ldo.GetMetaPtr<fado_meta_t *>()->num_chunks;
      uint32_t offset = 0;
      for (int i = 0; i < num_bundles; i++) {
         auto *chunk = reinterpret_cast<fado_chunk_t *>(data + offset);
         ss << "   [" << i << "] Size: " << chunk->data_length << " Rows: "<<chunk->num_rows <<" Magic: "<<std::hex<<chunk->magic <<endl;
         offset += roundupChunkSize(sizeof(fado_chunk_t) + chunk->data_length);
      }
   }
   return ss.str();
}

/// @brief Get the serialized size of the table for a particultar chunk
/// @param chunk_id The integer id of the chunk in this object
/// @return Size of the serialized data in this chunk
uint32_t ArrowDataObject::GetPackedRecordSize(int chunk_id) const {
  if(!Valid()) return 0;
  auto *chunk = locateChunk(chunk_id);
  if(!chunk) return 0;
  validChunkOrDie(chunk, chunk_id, "GetPackedRecordSize");
  return chunk->data_length;
}

/// @brief Write one chunk from this object to a Parquet file
/// @param full_path_filename Parquet file to create
/// @param chunk_id Which chunk in this object to write
/// @param options Any IPC options you want to pass parquet when it writes the data
/// @return Arrow Status for the operation
arrow::Status ArrowDataObject::ToParquet(const std::string &full_path_filename, int chunk_id, arrow::ipc::IpcReadOptions options) const {

   string uri="file:/";
   string root_path;

   if(!ValidChunk(chunk_id))
      return arrow::Status::IndexError("chunk id outside of range of this Faodel ArrowDataObject");


   // The do-or-die version of this would be:
   //   auto fs = faodel-arrow::fs::FileSystemFromUri(uri, &root_path).ValueOrDie();
   //   auto outfile = fs->OpenOutputStream(root_path + full_path_filename).ValueOrDie();
   //   auto table = ExtractTable(chunk_id, options).ValueOrDie()
   //

   ARROW_ASSIGN_OR_RAISE(auto fs,
                         arrow::fs::FileSystemFromUri(uri, &root_path));

   ARROW_ASSIGN_OR_RAISE(auto outfile,
                         fs->OpenOutputStream(root_path + full_path_filename));

   ARROW_ASSIGN_OR_RAISE(auto table,
                         ExtractTable(chunk_id, options));

   return parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 2048);

}

/// @brief Internal function for appending a new table into this object. Requires knowing serialized size of table
/// @param table Source table
/// @param table_size_in_bytes  Serialized size of table
/// @param options Any IPC options (eg compression) to pass in when serializing
/// @return Arrow Status for the operation
arrow::Status ArrowDataObject::doAppendTable(
    const std::shared_ptr<arrow::Table> &table, int64_t table_size_in_bytes,
    const arrow::ipc::IpcWriteOptions &options) {
   return doAppendTables(vector<std::shared_ptr<arrow::Table>>{table},
                         table_size_in_bytes, options);
}

/// @brief Internal function for appending multiple tables of same schema into an object
/// @param tables Tables to be appended (must be same schema)
/// @param tables_size_in_bytes Total size of the serialized tables
/// @param options Any IPC options (eg compression) to pass in when serializing
/// @return Arrow Status for the operation
/// @note This picks the next chunk in the LDO and streams the serialization into it
arrow::Status ArrowDataObject::doAppendTables(
    const vector<std::shared_ptr<arrow::Table>> &tables,
    int64_t tables_size_in_bytes, const arrow::ipc::IpcWriteOptions &options) {

   // Make sure we have enough capacity
   auto capacity = ldo.GetUserCapacity();
   auto user_size = ldo.GetUserSize();
   auto available_capacity = capacity - user_size - sizeof(fado_chunk_t);
   if (available_capacity < tables_size_in_bytes) {
      return arrow::Status::CapacityError(
          "Not enough capacity to append tables");
   }

   // Write data to chunk
   auto *chunk = reinterpret_cast<fado_chunk_t *>(ldo.GetDataPtr<char *>() +
                                                  ldo.GetDataSize());
   chunk->setMagic();
   chunk->data_length = tables_size_in_bytes;
   chunk->num_rows = 0;

   auto sink = std::make_unique<arrow::io::FixedSizeBufferWriter>(
       std::make_shared<arrow::MutableBuffer>(
           reinterpret_cast<uint8_t *>(chunk->data), tables_size_in_bytes));
   sink->set_memcopy_threads(2);
   ARROW_ASSIGN_OR_RAISE(
       auto writer,
       arrow::ipc::MakeStreamWriter(sink.get(), tables[0]->schema(), options));

   for (const auto &table : tables) {
      ARROW_RETURN_NOT_OK(writer->WriteTable(*table));
      chunk->num_rows += table->num_rows();
   }
   ARROW_RETURN_NOT_OK(writer->Close());
   ARROW_RETURN_NOT_OK(sink->Close());

   // Update the ldo's sizes and stats
   uint32_t new_chunk_size =
       roundupChunkSize(sizeof(fado_chunk_t) + tables_size_in_bytes);
   ldo.ModifyUserSizes(ldo.GetMetaSize(), ldo.GetDataSize() + new_chunk_size);
   ldo.GetMetaPtr<fado_meta_t *>()->num_chunks++;

   return arrow::Status::OK();
}

/// @brief Append a strip of chunks on to this object, provided they fit within the capacity (all or none)
/// @param strip_start Starting address for the first chunk in the strip
/// @param strip_bytes Total bytes for all the chunks in this strip
/// @param strip_chunks Number of chunks (tables) in this strip
/// @retval EINVAL bad input
/// @retval ENOMEM not enough space for the strips
int ArrowDataObject::doAppendChunkStrip(const faodel::ArrowDataObject::fado_chunk_t *strip_start, uint32_t strip_bytes,
                                        uint32_t strip_chunks) {

   if(!strip_start) return EINVAL;
   if((!strip_bytes)||(!strip_chunks)) return 0;

   auto capacity = ldo.GetUserCapacity();
   auto user_size = ldo.GetUserSize();
   auto available_capacity = capacity - user_size;
   if(available_capacity < strip_bytes) return ENOMEM;

   //Simple check to make sure the first entry is at least a valid chunk
   if(!strip_start->valid()) return EINVAL;

   auto *chunk_start = reinterpret_cast<fado_chunk_t *>(ldo.GetDataPtr<char *>() + ldo.GetDataSize());

   memcpy(chunk_start, strip_start, strip_bytes);
   ldo.ModifyUserSizes(ldo.GetMetaSize(), ldo.GetDataSize() + strip_bytes);

   ldo.GetMetaPtr<fado_meta_t *>()->num_chunks += strip_chunks;

   return 0;
}

/// @brief Debug function that checks to make sure a chunk is valid.
void ArrowDataObject::validChunkOrDie(fado_chunk_t *chunk, int i, const std::string &function) const {
   if((chunk) && (chunk->valid())) return;
   F_HALT("Invalid FADO chunk detected during "+function+", at offset "+std::to_string(i)+"\n"+str(true));
}

/// @brief Create a FADO that contains the serialize version of the provided table
/// @param table The table that will live in this object
/// @param codec Optional compression specification
/// @return An faodel-arrow result indicating success/failure, and the resulting FADO if successful
arrow::Result<ArrowDataObject> ArrowDataObject::Make(
    const std::shared_ptr<arrow::Table> &table,
    arrow::Compression::type codec) {
   // Set the compression options
   auto options = arrow::ipc::IpcWriteOptions::Defaults();
   ARROW_ASSIGN_OR_RAISE(options.codec, arrow::util::Codec::Create(codec));

   ARROW_ASSIGN_OR_RAISE(auto table_size_in_bytes,
                         GetSerializedTableSize(table, options));
   ArrowDataObject fado(sizeof(fado_chunk_t) + table_size_in_bytes);
   ARROW_RETURN_NOT_OK(fado.doAppendTable(table, table_size_in_bytes, options));
   return fado;
}

/// @brief Create a FADO that contains the serialized versions of the provided tables. Do NOT merge tables
/// @param tables A vector of tables that will live in this object
/// @param codec Optional compression specification
/// @return An faodel-arrow result indicating success/failure, and the resulting FADO if successful
arrow::Result<ArrowDataObject> ArrowDataObject::Make(
    const std::vector<std::shared_ptr<arrow::Table>> &tables,
    arrow::Compression::type codec) {
   // Set the compression options
   auto options = arrow::ipc::IpcWriteOptions::Defaults();
   ARROW_ASSIGN_OR_RAISE(options.codec, arrow::util::Codec::Create(codec));

   vector<int64_t> table_sizes;
   size_t total_size = 0;
   for (const auto &table : tables) {
      ARROW_ASSIGN_OR_RAISE(auto table_size_in_bytes,
                            GetSerializedTableSize(table, options));
      table_sizes.push_back(table_size_in_bytes);
      total_size += sizeof(fado_chunk_t) + table_size_in_bytes;
   }

   // Pack the tables into a single fado
   ArrowDataObject fado(total_size);
   for (int i = 0; i < tables.size(); i++) {
      ARROW_RETURN_NOT_OK(fado.doAppendTable(tables[i], table_sizes[i], options));
   }
   return fado;
}

/// @brief Copy the contents of a collection of existing FADOs into a new FADO
/// @param fados List of existing FADOs
/// @returns New fado containing everything, or an error
/// @note This is all-or-nothing. All input FADOs need to have valid tables. If any are empty, this fails
arrow::Result<ArrowDataObject> ArrowDataObject::Make(const std::vector<faodel::ArrowDataObject> &fados) {

   //Figure out how big to make the object
   uint32_t size = 0;
   for(auto src_fado: fados) {
      size+=src_fado.GetDataSize();
   }
   //No data? return an empty object
   if(size==0) {
      return ArrowDataObject();
   }
   //Allocate the space
   ArrowDataObject fado(size);

   //Append each object
   for(auto src_fado: fados) {
      auto stat = fado.Append(src_fado);
      if(!stat.ok()) return stat;
   }
   return fado;
}

/// @brief Concatenate a list of tables into one table, and store in a FADO
/// @param tables A vector of tables that will be concatenated into FADO
/// @param codec Optional compression specification
/// @return An faodel-arrow result indicating success/failure, and the resulting FADO if successful
arrow::Result<ArrowDataObject> ArrowDataObject::MakeMerged(
    const std::vector<std::shared_ptr<arrow::Table>> &tables,
    arrow::Compression::type codec) {
   if (tables.empty()) return ArrowDataObject();

   // Set the compression options
   auto options = arrow::ipc::IpcWriteOptions::Defaults();
   ARROW_ASSIGN_OR_RAISE(options.codec, arrow::util::Codec::Create(codec));

   size_t total_size = 0;
   for (const auto &table : tables) {
      ARROW_ASSIGN_OR_RAISE(auto table_size_in_bytes,
                            GetSerializedTableSize(table, options));
      total_size += table_size_in_bytes;
   }

   // Pack the tables into a single fado
   ArrowDataObject fado(sizeof(fado_chunk_t) + total_size);
   ARROW_RETURN_NOT_OK(fado.doAppendTables(tables, total_size, options));
   return fado;
}

/// @brief Serialize an arrow table to a mock stream to calculate how big it is
/// @param table The Apache Arrow table to serialize
/// @param options Serialization options
/// @return An Arrow result with error conditions or the size in bytes of the serialized data
/// @note This walks through serialization, but does not write anything. Since Arrow's serialization
///       process is fast, it's faster to run serialization twice for big tables than it is to
///       serialize to a tmp buffer and copy it somewhere
arrow::Result<int64_t> ArrowDataObject::GetSerializedTableSize(
        const std::shared_ptr<arrow::Table> &table,
        const arrow::ipc::IpcWriteOptions &options) {
   arrow::io::MockOutputStream sink;
   ARROW_ASSIGN_OR_RAISE(auto writer, arrow::ipc::MakeStreamWriter(
           &sink, table->schema(), options));
   ARROW_RETURN_NOT_OK(writer->WriteTable(*table));
   ARROW_RETURN_NOT_OK(writer->Close());
   ARROW_RETURN_NOT_OK(sink.Close());
   return sink.GetExtentBytesWritten();
}

/// @brief Whookie function for dumping this object to html/txt
/// @param ldo The data object
/// @param rs The output ReplyStream
void fn_dumpArrowDataObject(const lunasa::DataObject &ldo, faodel::ReplyStream &rs) {

   auto fado = ArrowDataObject(ldo);

   rs.mkSection("ArrowDataObject Dump");

   rs.tableBegin("Stats", 2);
   rs.tableTop({"Parameter", "Value"});
   rs.tableRow({"Number Tables", std::to_string(fado.NumberOfTables())});
   rs.tableRow({"Total Rows", std::to_string(fado.NumberOfRows())});
   rs.tableRow({"Data Size", std::to_string(fado.GetDataSize())});
   rs.tableRow({"User Capacity", std::to_string(fado.GetCapacity())});
   rs.tableEnd();

   int max_tables=fado.NumberOfTables();
   if(max_tables>10) max_tables=10;

   for(int t=0; t<max_tables; t++) {

      auto res = fado.ExtractTable(t);
      if(!res.ok()) {
         rs.mkSection("Error: could not extract table", 2);
      } else {
         auto table = res.ValueOrDie();
         auto schema = table->schema();
         int num_field = schema->num_fields();
         rs.tableBegin("Table "+std::to_string(t), 2);
         rs.tableTop( schema->field_names());

         int rows_left=100;
         arrow::TableBatchReader tbr(table);
         std::shared_ptr<arrow::RecordBatch> record_batch;
         while( (rows_left>0) && (tbr.ReadNext(&record_batch).ok()) && (record_batch)) {

            for(int brow=0; (rows_left>0) && (brow<record_batch->num_rows()); brow++) {
               vector<string> col_strings;

               for(int i=0; i<schema->num_fields(); i++) {
                  auto col = record_batch->column(i);
                  if(schema->field(i)->type() == arrow::int32()) {
                     //col_strings.push_back(std::to_string( std::static_pointer_cast<arrow::Int32Array>(col)->Value(brow) ));
                     stringstream ss;
                     ss<<std::hex<<std::static_pointer_cast<arrow::Int32Array>(col)->Value(brow);
                     col_strings.push_back(ss.str());
                  } else if (schema->field(i)->type() == arrow::int64())   {
                     //col_strings.push_back(std::to_string( std::static_pointer_cast<arrow::Int64Array>(col)->Value(brow) ));
                     stringstream ss;
                     ss<<std::hex<<std::static_pointer_cast<arrow::Int64Array>(col)->Value(brow);
                     col_strings.push_back(ss.str());
                  } else if (schema->field(i)->type() == arrow::float32()) { col_strings.push_back(std::to_string(std::static_pointer_cast<arrow::FloatArray>(col)->Value(brow)));
                  } else if (schema->field(i)->type() == arrow::float64()) { col_strings.push_back(std::to_string(std::static_pointer_cast<arrow::DoubleArray>(col)->Value(brow)));
                  } else if (schema->field(i)->type() == arrow::utf8())    { col_strings.push_back(string(std::static_pointer_cast<arrow::StringArray>(col)->Value(brow))); //Ack! string_view!
                  } else if (schema->field(i)->type() == arrow::binary())  {
                     std::stringstream ss;
                     ss<<std::static_pointer_cast<arrow::BinaryArray>(col)->Value(brow);
                     col_strings.push_back(ss.str());
                  }else {
                     col_strings.push_back("?");
                  }
               }
               rs.tableRow(col_strings);
               rows_left--;
            }
         }
         rs.tableEnd();
      }
   }
   rs.tableEnd();
}

/// @brief function for registering this object's whookie dumper with lunasa
void ArrowDataObject::RegisterDataObjectType() {
  lunasa::RegisterDataObjectType(object_type_id, object_type_name, fn_dumpArrowDataObject);
}



} // namespace faodel
