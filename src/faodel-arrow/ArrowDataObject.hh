// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_ARROWDATAOBJECT_HH
#define FAODEL_ARROWDATAOBJECT_HH

#include <cstdint>
#include <vector>

#include "lunasa/DataObject.hh"
#include <memory>

#include <arrow/api.h>
#include <arrow/table.h>
#include <arrow/io/api.h> //for Compression::type
#include <arrow/ipc/api.h>

namespace faodel {

/**
 * @brief A wrapper class to make it easy to pack/unpack one or more Apache Arrow tables in a Lunasa Data Object
 *
 * Apache Arrow is a popular library for passing tabular data between different data science applications and
 * data stores. Arrow provides convenient mechanisms for serializing in-memory data into a contiguous
 * "IPC format" that can be transported over the network. The Faodel ArrowDataObject (or FADO) is a class
 * that takes care of all the work of converting one or more tables to a Lunasa Data Object (or LDO) that
 * can be moved about in libraries like Kelpie.
 *
 * FADO allows multiple tables to be packed into one LDO. Each table is accessed by its "chunk id". While
 * it is expected that most people will store tables with the same schema, FADO appends/extracts each
 * table individually.
 *
 * @note This class is intended to be a thin wrapper around an LDO. You can have multiple FADOs that
 *       read from the same LDO, but there are no locks to protect you if multiple FADOS try to modify
 *       the same FADO.
 */
class ArrowDataObject {

public:
  explicit ArrowDataObject() = default;
  explicit ArrowDataObject(uint32_t max_arrow_capacity);
  explicit ArrowDataObject(lunasa::DataObject import_ldo);
  explicit ArrowDataObject(const std::shared_ptr<arrow::Table> &table, arrow::Compression::type codec=arrow::Compression::UNCOMPRESSED);


  bool Valid() const;
  bool ValidChunk(int chunk_id) const;


  arrow::Status Append(const std::shared_ptr<arrow::Table> &table, arrow::Compression::type codec = arrow::Compression::UNCOMPRESSED);
  arrow::Status Append(const ArrowDataObject src_fado);

  void Wipe(faodel::internal_use_only_t iot, bool zero_out_data=false);

  bool dbgAllChunksValid() const;
  std::string str(bool show_details=false) const;

  const static uint16_t object_type_id;  //!< Universal lunasa data object id to mark this as a FADO
  const static std::string object_type_name; //!< String to identify this as a FADO

  uint32_t GetObjectStatus() const;
  void SetObjectStatus(uint32_t status);
  int NumberOfTables() const;
  uint64_t NumberOfRows() const;
  arrow::Result<std::shared_ptr<arrow::Table>> ExtractTable(int chunk_id, arrow::ipc::IpcReadOptions options=arrow::ipc::IpcReadOptions::Defaults()) const;

  uint32_t GetPackedRecordSize(int chunk_id) const;

  lunasa::DataObject ExportDataObject() { return ldo; }

  //Stats about storage space from ldos
  uint32_t GetDataSize() const { return ldo.GetDataSize(); } //!< Amount of serialized data in this object
  uint32_t GetCapacity() const { return ldo.GetUserCapacity() - sizeof(fado_meta_t); } //!< Total capacity for storing serialized tables
  uint32_t GetAvailableCapacity() const { return GetCapacity() - GetDataSize()  - sizeof(fado_chunk_t);} //!< Current space left for storing a serialized table

  double CurrentUtilizationRatio() const {
     return ((double)ldo.GetUserSize())/((double)ldo.GetUserCapacity());
  }

  arrow::Status ToParquet(const std::string &full_path_filename, int chunk_id, arrow::ipc::IpcReadOptions options=arrow::ipc::IpcReadOptions::Defaults()) const;

  //Static functions for safely generating different kinds of objects. Use these instead of ctors
  static arrow::Result<ArrowDataObject> Make(const std::shared_ptr<arrow::Table> &table, arrow::Compression::type codec=arrow::Compression::UNCOMPRESSED);
  static arrow::Result<ArrowDataObject> Make(const std::vector<std::shared_ptr<arrow::Table>> &tables, arrow::Compression::type codec=arrow::Compression::UNCOMPRESSED);
  static arrow::Result<ArrowDataObject> Make(const std::vector<faodel::ArrowDataObject> &fados);
  static arrow::Result<ArrowDataObject> MakeMerged(const std::vector<std::shared_ptr<arrow::Table>> &tables, arrow::Compression::type codec=arrow::Compression::UNCOMPRESSED);

  //Also, this function is useful for getting the size of serialized arrow tables
  static arrow::Result<int64_t> GetSerializedTableSize(const std::shared_ptr<arrow::Table> &table, const arrow::ipc::IpcWriteOptions &options);


  static void RegisterDataObjectType();

private:

   /// @brief Internal meta data that goes in the LDO meta section
   struct fado_meta_t {
      uint32_t num_chunks;
      uint32_t object_status;
      void wipe() { num_chunks=0; object_status=0;}
   };

  /// @brief Internal header that goes in front of each serialized table chunk
  struct fado_chunk_t {
    uint32_t magic;         //Special identifier to ensure we're on track
    uint32_t data_length;   //How many bytes this segment is
    uint64_t num_rows;      //Number of faodel-arrow rows in this table
    char     data[0];

    void setMagic() { magic=0xf4D02112; }
    bool valid() const { return (magic == 0xf4D02112);}
  };


  lunasa::DataObject ldo; //!< The underlying object that holds the serialized data

  uint32_t roundupChunkSize(uint32_t size) const;
  fado_chunk_t *locateChunk(int chunk_id) const;
  void validChunkOrDie(fado_chunk_t *chunk, int i, const std::string &function) const;

  int doAppendChunkStrip(const fado_chunk_t *strip_start, uint32_t strip_bytes, uint32_t strip_chunks);

   arrow::Status doAppendTable(const std::shared_ptr<arrow::Table> &table,
                              int64_t table_size_in_bytes,
                              const arrow::ipc::IpcWriteOptions &options);
  arrow::Status doAppendTables(const std::vector<std::shared_ptr<arrow::Table>> &tables,
                               int64_t tables_size_in_bytes,
                               const arrow::ipc::IpcWriteOptions &options);
};

} // namespace faodel
#endif //FAODEL_ARROWDATAOBJECT_HH
