// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file DataObject.hh
 *
 * @brief DataObject.hh
 * 
 */

#ifndef LUNASA_DATAOBJECT_HH
#define LUNASA_DATAOBJECT_HH

#include <stddef.h>
#include <exception>
#include <string>
#include <queue>

#include "faodel-common/NodeID.hh"
#include "faodel-common/InfoInterface.hh"
#include "lunasa/common/Types.hh"

//forward refs
namespace lunasa { namespace internal { struct Allocation; } }
namespace lunasa { namespace internal { class AllocatorBase; } }



namespace lunasa {

class DataObject
        : public faodel::InfoInterface {

public:
  enum class AllocatorType { eager, lazy };

  //Constructors
  DataObject(); // Create an empty LDO to be filled in later (nullptr impl)
  DataObject(uint32_t user_capacity,
             uint16_t meta_capacity,  uint32_t data_capacity,
             AllocatorType allocator,
             dataobject_type_t tag); // Allocate memory for a new LDO

  DataObject(uint16_t meta_capacity, uint32_t data_capacity,
             AllocatorType allocator, dataobject_type_t tag=0)
          : DataObject(meta_capacity+data_capacity,
                       meta_capacity, data_capacity,
                       allocator, tag) {}



  DataObject(uint32_t dataCapacity); // Shortcut for creating a new LDO
  DataObject(void *userMemory,
             uint16_t metaCapacity, uint32_t dataCapacity,
             void (*userCleanupFunc)(void *)); // Create an LDO from pre-allocated memory

  ~DataObject() override;
  

  DataObject(const DataObject &); // shallow copies
  DataObject(DataObject &&); // move

  lunasa::internal::AllocatorBase* Allocator();
  void DropReference(lunasa::internal::AllocatorBase *allocator);

  class InternalUseOnly {
    public:
      InternalUseOnly(DataObject *ldo_) : ldo(ldo_) {}

      /* METHODS that are intended for internal development use ONLY */
      int GetRefCount() const;
      void *GetLocalHeaderPtr() const;
      void *GetHeaderPtr() const;

  private:
      DataObject *ldo;
  };

  friend class InternalUseOnly;

  InternalUseOnly internal_use_only; //Place for internal functions to live

  // shallow copies decrefing whatever was previously there
  DataObject& operator=(const DataObject&);

  // move
  DataObject& operator=(DataObject&&);

  // deep copies into the existing pointer, only capacities must match
  DataObject& deepcopy(const DataObject&);

  uint32_t writeToFile(const char *filename) const; //Write header/meta/data to file
  uint32_t readFromFile(const char *filename); //Read header/meta/data from file


  /*! @brief Determine whether memory is registered
   * 
   *  Returns TRUE if the memory represented by this data object has been registered with a
   *  network transport, and FALSE otherwise. */
  bool isPinned() const;

  /**
   * @brief Determine if this object hasn't been assigned an allocation yet
   * @retval TRUE When it is not associated with an allocation
   * @retval FALSE When there is an allocation associated with this data object
   */
  bool isNull() const { return impl==nullptr; }

  /* GET heap address pointers */
  void *GetBasePtr() const;
  template <class T>
  T GetBasePtr() const {
    return static_cast<T>(GetBasePtr());
  }

  void *GetMetaPtr() const;
  template <class T>
  T GetMetaPtr() const {
    return static_cast<T>(GetMetaPtr());
  }

  void *GetDataPtr() const;
  template <class T>
  T GetDataPtr() const {
    return static_cast<T>(GetDataPtr());
  }

  dataobject_type_t GetTypeID() const;
  void              SetTypeID(dataobject_type_t type_id);


  /* GET size of LDO segments */
  uint32_t GetLocalHeaderSize() const;   //Size of local bookkeeping
  uint32_t GetHeaderSize() const;        //Header (contains tag, meta, and data lengths)
  uint32_t GetMetaSize() const;          //Meta size reported in header
  uint32_t GetDataSize() const;          //Data size reported in header
  uint32_t GetUserSize() const;          //Meta + Data
  uint32_t GetWireSize() const;          //Header+Meta+Data
  uint32_t GetRawAllocationSize() const; //Local bookkeeping's allocated bytes for header+meta+data (rare)
  uint32_t GetUserCapacity() const;      //Maximum amount of space that user's Meta+Data can be

  int ModifyUserSizes(uint16_t new_meta_size, uint32_t new_data_size); // Adjust values in header (iff fits)

  /* Get RDMA handles */

  /*! @brief Class describing the characterisitics of a memory segment
   * 
   *  LDOs that are created from memory that was not allocated by Lunasa will be comprised
   *  of two (likely) discontinous memory regions.  An instance of this class will be used
   *  to describe each. 
   *
   *  \sa DataObject::GetXXXXRdmaHandles() */
  class rdma_segment_desc
  {
    public:
      rdma_segment_desc(void *net_buffer_handle_, uint32_t net_buffer_offset_, uint32_t size_) :
        net_buffer_handle(net_buffer_handle_), 
        net_buffer_offset(net_buffer_offset_),
        size(size_)
      { }
    
      /*! @brief Handle to registered memory
       * 
       *  This is the value of the handle returned by the network transport when the associated
       *  memory was registered.  */
      void *net_buffer_handle;
    
      /*! @brief Offset in registered memory
       * 
       *  This is the number of bytes from the beginning of the block of registered memory to 
       *  the data object section of interest.  In some cases, a large block of memory has been
       *  registered and this offset indicates which portion of the larger block should be sent
       *  across the network. */ 
      uint32_t net_buffer_offset;

      /*! @brief Size of memory segment.
       * 
       *  This is the number of bytes that constitute this memory segment. */
      uint32_t size;
  };

  int GetBaseRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const;
  int GetBaseRdmaHandle(void **rdma_handle, uint32_t &offset) const; 

  int GetLocalHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const;
  int GetLocalHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const;

  int GetHeaderRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const;
  int GetHeaderRdmaHandle(void **rdma_handle, uint32_t &offset) const; 

  int GetMetaRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const;
  int GetMetaRdmaHandle(void **rdma_handle, uint32_t &offset) const; 

  int GetDataRdmaHandles(std::queue<rdma_segment_desc> &rdma_segments) const;
  int GetDataRdmaHandle(void **rdma_handle, uint32_t &offset) const; 

  //Comparisons for checking whether this ldo points to the same thing as another
  bool operator==(const DataObject &other) const {
    return (this->impl == other.impl);
  }
  bool operator!=(const DataObject &other) const {
    return (this->impl != other.impl);
  }

  int DeepCompare(const DataObject &other) const;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  // Pointer to STRUCTURE that represents the "beginning" of the data object.
  // Currently, this is at least the headers and the user metadata.
  internal::Allocation *impl;

  void AddUserDataSegment(void *userMemory, uint32_t metaCapacity, uint32_t dataCapacity, 
                          void (*userCleanupFunc)(void *));
};

class InstanceUninitialized : public std::exception  {
  const char* what() const throw() override {
    return "Tried to instantiate lunasa::DataObject without a Lunasa instance";
  }
};

class SizeMismatch : public std::exception {
  virtual const char* what() const throw() {
    return "Array sizes do not match";
  }
};

class UserLDOAccessError : public std::exception {
  const char* what() const throw() override {
    return "Improper access of USER LDO";
  }
};

class InvalidArgument : public std::exception {
  const char* what() const throw() override {
    return "Invalid argument";
  }
};

} // namespace lunasa

#endif // LUNASA_DATAOBJECT_HH 
