// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <sstream>

#include "common/MutexWrapper.hh"
#include "common/Debug.hh"

using namespace std;

namespace faodel {


/**
 * @brief NOP MutexWrapper does NOT do any locking
 *
 * This wrapper if for performance comparisons where a user wants to
 * know how much overheads locks are causing. It is NOT expected that
 * users will use this in real code.
 */
class MutexWrapperNone : public MutexWrapper {
public:
  MutexWrapperNone() {}
  void Lock() {}
  void ReaderLock() {}
  void WriterLock() {}
  void Unlock() {}
  void yield() {}
  std::string GetType() const { return "none"; }
  MutexWrapperTypeID GetTypeID() const { return MutexWrapperTypeID::NONE; }
  MutexWrapperNone(const MutexWrapperNone&) {  }
  MutexWrapperNone& operator= (const MutexWrapperNone& ) { return *this; }
};


//=============================================================================
// MutexWrapperOMP: Use openMP's locks
#if defined(_OPENMP)
#include <omp.h>
class MutexWrapperOMP : public MutexWrapper {
public:
  MutexWrapperOMP()   { debug=0; omp_init_lock(&lock); }
  ~MutexWrapperOMP()  { omp_destroy_lock(&lock); }
  void Lock()   { if(debug) {printf("lock %s\n",name.c_str());} omp_set_lock(&lock);  }
  void ReaderLock() { Lock(); }
  void WriterLock() { Lock(); }
  void Unlock() { if(debug) {printf("unlock %s\n",name.c_str());} omp_unset_lock(&lock);  }
  void yield()  {
#pragma omp taskyield
  }
  std::string GetType() const { return "openmp"; }
  MutexWrapperTypeID GetTypeID() const { return OMP_LOCK; }
  MutexWrapperOMP(const MutexWrapperOMP&) { omp_init_lock(&lock); }
  MutexWrapperOMP& operator= (const MutexWrapperOMP& ) {return *this; }
private:
  omp_lock_t lock;
};
#endif


//=============================================================================
// MutexWrapperPT: PThreads wrapper
#ifdef _PTHREADS
#include <pthread.h>
#ifdef __APPLE__
#include <sched.h>
#endif

class MutexWrapperPT : public MutexWrapper {

public:
  MutexWrapperPT()   { pthread_mutex_init(&lock_mutex, NULL); }
  ~MutexWrapperPT()  { pthread_mutex_destroy(&lock_mutex); }

  void Lock()   { pthread_mutex_lock(&lock_mutex); }
  void ReaderLock() { Lock(); }
  void WriterLock() { Lock(); }

  void Unlock() { pthread_mutex_unlock(&lock_mutex); }
#ifndef __APPLE__
  void yield()  { pthread_yield(); }
#else
  void yield()  { sched_yield(); }
#endif
  std::string GetType() const { return "pthreads-lock"; }
  MutexWrapperTypeID GetTypeID() const { return  MutexWrapperTypeID::PTHREADS_LOCK; }
  MutexWrapperPT(const MutexWrapperPT&) { pthread_mutex_init(&lock_mutex, NULL); }
  MutexWrapperPT& operator= (const MutexWrapperPT& ) { return *this; }

private:
  pthread_mutex_t lock_mutex;

};


class MutexWrapperRWPT : public MutexWrapper {

public:
  MutexWrapperRWPT()   { pthread_rwlock_init(&rwlock, NULL); }
  ~MutexWrapperRWPT()  { pthread_rwlock_destroy(&rwlock); }

  void Lock()       { WriterLock(); } //Assume worst-case
  void ReaderLock() { pthread_rwlock_rdlock(&rwlock); }
  void WriterLock() { pthread_rwlock_wrlock(&rwlock); }

  void Unlock() { pthread_rwlock_unlock(&rwlock); }
#ifndef __APPLE__
  void yield()  { pthread_yield(); }
#else
  void yield()  { sched_yield(); }
#endif
  std::string GetType() const { return "pthreads-rwlock"; }
  MutexWrapperTypeID GetTypeID() const { return  MutexWrapperTypeID::PTHREADS_RWLOCK; }
  MutexWrapperRWPT(const MutexWrapperPT&) { pthread_rwlock_init(&rwlock, NULL); }
  MutexWrapperRWPT& operator= (const MutexWrapperRWPT& ) { return *this; }

private:
  pthread_rwlock_t rwlock;

};

#endif



int GetMutexInfoByID(MutexWrapperTypeID id, string *threading_model, string *mutex_type) {
  string tm,mt;
  switch(id) {
  case MutexWrapperTypeID::DEFAULT:         tm=mt="default"; break;
  case MutexWrapperTypeID::NONE:            tm=mt="none"; break;
  case MutexWrapperTypeID::OMP_LOCK:        tm="openmp";   mt="lock"; break;
  case MutexWrapperTypeID::PTHREADS_LOCK:   tm="pthreads"; mt="lock"; break;
  case MutexWrapperTypeID::PTHREADS_RWLOCK: tm="pthreads"; mt="rwlock"; break;
  case MutexWrapperTypeID::UNSUPPORTED:     tm=mt="unsupported"; break;
  case MutexWrapperTypeID::ERROR:           tm=mt="error"; break;
  }
  if(threading_model) *threading_model = tm;
  if(mutex_type) *mutex_type =mt;
  return -((id==MutexWrapperTypeID::ERROR)||(id==MutexWrapperTypeID::UNSUPPORTED));
}
string to_string(const MutexWrapperTypeID &id) {
  string tm, mt;
  GetMutexInfoByID(id, &tm, &mt);
  return (tm!=mt) ? tm+"-"+mt : tm;
}

MutexWrapperTypeID GetMutexTypeID(std::string threading_model, std::string mutex_type) {

  if((threading_model=="none") || (mutex_type=="none")) return MutexWrapperTypeID::NONE;

  #ifdef _OPENMP
  if((threading_model=="omp") || (threading_model=="openmp")) return MutexWrapperTypeID::OMP_LOCK;
  #endif

  #ifdef _PTHREADS
  if((threading_model=="pthreads") || (threading_model=="default")) {

    //rwlock's are the thing to use in pthreads.
    if(mutex_type=="rwlock") return MutexWrapperTypeID::PTHREADS_RWLOCK;

    //Default: fall back to standard Mutex
    return MutexWrapperTypeID::PTHREADS_LOCK;
  }
  #endif

  kassert(false, "Unable to resolve Mutex Wrapper for threading model/type "+threading_model+"/"+mutex_type+".\n"
          "         library may not have right compile flags (eg, -lpthread)\n");

  return MutexWrapperTypeID::ERROR;
}

MutexWrapper * GenerateMutexByTypeID(MutexWrapperTypeID mutex_type_id) {

  switch(mutex_type_id) {
  case MutexWrapperTypeID::NONE: return static_cast<MutexWrapper *>(new MutexWrapperNone());

#ifdef _OPENMP
  case MutexWrapperTypeID::DEFAULT:
  case MutexWrapperTypeID::OMP_LOCK: return static_cast<MutexWrapper *>(new MutexWrapperOMP());
#endif

#ifdef _PTHREADS
  case MutexWrapperTypeID::DEFAULT: //Default to plain lock
  case MutexWrapperTypeID::PTHREADS_LOCK: return static_cast<MutexWrapper *>(new MutexWrapperPT());
  case MutexWrapperTypeID::PTHREADS_RWLOCK: return static_cast<MutexWrapper *>(new MutexWrapperRWPT());
#endif

 default: ;
  }
  //Here by mistake
  kassert(false, "Unable to resolve Mutex Wrapper "+to_string(mutex_type_id)+"\n"+
          "         library may not have right compile flags (eg, -lpthread)\n");
  return nullptr;

}

MutexWrapper * GenerateMutex(string threading_model, string mutex_type) {
  return GenerateMutexByTypeID(GetMutexTypeID(threading_model, mutex_type));
}


string MutexWrapperCompileTimeInfo() {
  stringstream ss;
  ss << "Guttie::MutexWrapper was compiled with support for : ";
  #ifdef _PTHREADS
    ss << "pthreads ";
  #endif
  #ifdef _OPENMP
    ss << "openmp ";
  #endif

  #if !(defined(_PTHREADS) || defined(_OPENMP))
    ss << "none";
  #endif

  ss<<"\n";
  return ss.str();
}


} // namespace kelpie
