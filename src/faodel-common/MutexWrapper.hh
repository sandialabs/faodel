// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_MUTEXWRAPPER_HH
#define FAODEL_COMMON_MUTEXWRAPPER_HH

#include <string>

// MutexWrapper : a simple wrapper for handling locks
//  Current types: "none", "pthread", or "openmp"

namespace faodel {

enum class MutexWrapperTypeID: int {
    DEFAULT=1,         //!< Use whatever the default is
    NONE=2,            //!< No Lock.
    OMP_LOCK=3,        //!< OpenMP plain lock
    PTHREADS_LOCK=4,   //!< Pthreads plain lock
    PTHREADS_RWLOCK=5, //1< Pthreads readers/writer lock
    UNSUPPORTED=6,     //!< Asked for lock we weren't compiled for
    ERROR=7            //!< parse problem
};


/**
 * @brief Provides a generic MutexWrapper (for portability)
 *
 * Some users may need to run in different thread environments. This wrapper
 * class is a way to insulate Faodel from changes to the assumed threading
 * behavior.
 */
class MutexWrapper {
public:
  virtual ~MutexWrapper() = default; //Ensure derived classes get called after cast
  void setName(std::string caller_name) {name=caller_name;}
  virtual void Lock()=0; //Alias to Writer Lock rw
  virtual void ReaderLock()=0;
  virtual void WriterLock()=0;
  virtual void Unlock()=0;
  virtual void yield()=0;
  virtual std::string GetType() const =0;
  virtual MutexWrapperTypeID GetTypeID() const=0;
  std::string name;
  int debug;
};

MutexWrapperTypeID GetMutexTypeID(std::string threading_model="default", std::string mutex_type="default");
MutexWrapper * GenerateMutexByTypeID(MutexWrapperTypeID id=MutexWrapperTypeID::DEFAULT);
MutexWrapper * GenerateMutex(std::string threading_model="default", std::string mutex_type="default");

std::string to_string(const MutexWrapperTypeID &id);


std::string MutexWrapperCompileTimeInfo();

} // namespace faodel

#endif // FAODEL_COMMON_MUTEXWRAPPER_HH
