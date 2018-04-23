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

#include "common/NodeID.hh"
#include "common/InfoInterface.hh"
#include "lunasa/common/Types.hh"

namespace lunasa {

//forward refs
struct allocation_s;
typedef allocation_s allocation_t;
class AllocatorBase;


class DataObject : public faodel::InfoInterface {

public:
  enum class AllocatorType { eager, lazy };

  // Create an empty LDO to be filled in later (nullptr impl)
  DataObject();                         

  // Allocate memory for a new LDO
  DataObject(uint16_t metaCapacity, uint32_t dataCapacity, AllocatorType allocator);

  // Shortcut for creating a new LDO
  DataObject(uint32_t dataCapacity);

  // Create an LDO from pre-allocated memory
  DataObject(void *userMemory, uint16_t metaCapacity, uint32_t dataCapacity, void (*userCleanupFunc)(void *));

  ~DataObject();
  
  // shallow copies 
  DataObject(const DataObject &);

  // move
  DataObject(DataObject &&);

  // Establishes a new data object from an existing ptr
  // rawLength should reflect the reality of the rawPtr,
  // allocator should define where the DO will free the memory
  //DataObject(void *rawPtr, uint32_t rawLength, lunasa::AllocatorBase* allocator);

  lunasa::AllocatorBase* Allocator();
  void DropReference(lunasa::AllocatorBase* allocator);

  class InternalUseOnly {
    public:
      InternalUseOnly(DataObject *ldo_) : ldo(ldo_) {}

      /* METHODS that are intended for internal development use ONLY */
      int GetRefCount() const;

    private:
      DataObject *ldo;
  };

  friend class InternalUseOnly;

  InternalUseOnly internal_use_only;

  // shallow copies decrefing whatever was previously there
  DataObject& operator=(const DataObject&);

  // move
  DataObject& operator=(DataObject&&);

  // deep copies into the existing pointer, only capacities must match
  DataObject& deepcopy(const DataObject&);

  /*! @brief Write the contents of the LDO to a file.  */
  uint32_t writeToFile(const char *filename) const;

  /*! @brief Read the contents of the LDO from a file.  */
  uint32_t readFromFile(const char *filename);

#if 0
  // NOT clear that there's a real use case for this...  [sll]
  std::string meta() const;
  // replaces meta with newMeta, throws an exception if the meta sizes don't match
  void meta(const std::string& newMeta);
#endif

  /*! @brief Determine whether memory is registered
   * 
   *  Returns TRUE if the memory represented by this data object has been registered with a
   *  network transport, and FALSE otherwise. */
  bool isPinned() const;

  faodel::nodeid_t origin() const;
  void fixOrigin(faodel::nodeid_t node);

  /* GET heap address pointers */
  void *GetBasePtr() const;
  template <class T>
  T GetBasePtr() const
  { 
    return static_cast<T>(GetBasePtr());
  }

  void *GetLocalHeaderPtr() const;
  template <class T>
  T GetLocalHeaderPtr() const
  { 
    return static_cast<T>(GetLocalHeaderPtr());
  }

  void *GetHeaderPtr() const;
  template <class T>
  T GetHeaderPtr() const
  { 
    return static_cast<T>(GetHeaderPtr());
  }

  void *GetMetaPtr() const;
  template <class T>
  T GetMetaPtr() const
  { 
    return static_cast<T>(GetMetaPtr());
  }

  void *GetDataPtr() const;
  template <class T>
  T GetDataPtr() const
  { 
    return static_cast<T>(GetDataPtr());
  }

  /* GET size of LDO segments */
  uint32_t GetLocalHeaderSize() const;
  uint32_t GetHeaderSize() const;
  uint32_t GetMetaSize() const;
  uint32_t GetDataSize() const;
  uint32_t GetUserSize() const;         //Meta + Data
  uint32_t GetTotalSize() const;        //Header + Meta + Data

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
  
  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  // Pointer to STRUCTURE that represents the "beginning" of the data object.
  // Currently, this is at least the headers and the user metadata.
  allocation_t *impl;

  void AddUserDataSegment(void *userMemory, uint32_t metaCapacity, uint32_t dataCapacity, 
                          void (*userCleanupFunc)(void *));
};

class InstanceUninitialized : public std::exception  {
  virtual const char* what() const throw() {
    return "Tried to instantiate lunasa::DataObject without a Lunasa instance";
  }
};

class SizeMismatch : public std::exception {
  virtual const char* what() const throw() {
    return "Array sizes do not match";
  }
};

class UserLDOAccessError : public std::exception {
  virtual const char* what() const throw() {
    return "Improper access of USER LDO";
  }
};

class InvalidArgument : public std::exception {
  virtual const char* what() const throw() {
    return "Invalid argument";
  }
};

} // namespace lunasa

#endif // LUNASA_DATAOBJECT_HH 
