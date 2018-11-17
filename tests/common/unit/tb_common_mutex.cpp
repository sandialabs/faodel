// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <string>
#include <vector>
#include <map>
#define __STDC_FORMAT_MACROS

#include <sys/time.h>

#include "gtest/gtest.h"


#include "common/MutexWrapper.hh"


//TODO: This should have testing on the mutexwrapper id stuff

//=============================================================================
uint64_t getTimeUS(struct timeval *stop, struct timeval *start) {
  return (((uint64_t)(stop->tv_sec)  - (uint64_t)(start->tv_sec))*1000000) +
          ((uint64_t)(stop->tv_usec) - (uint64_t)(start->tv_usec));
}

//=============================================================================
double getMBps(uint64_t bytes, struct timeval *stop, struct timeval *start) {
  uint64_t us;
  us = getTimeUS(stop,start);
  return ((double)bytes)/((double)us);
}


//Compile time check to make sure one of these works
#if !(defined(_PTHREADS) || defined(_OPENMP))
static_assert(false, "Neither _PTHREADS nor _OPENMP were defined");
#endif


using namespace std;
using namespace faodel;

typedef struct {
  int                id;
  uint64_t           iterations;
  MutexWrapper      *mutex;
  uint64_t          *count; //When doing simple burn
  //For rwtests
  map<int,int>       *rw_map;
  float              rw_ratio;
  int                bad_count;
} args_t;

void * th_burn(void *args) {

  MutexWrapper *mutex     = ((args_t *)args)->mutex;
  uint64_t     *count_ptr = ((args_t *)args)->count;
  struct timeval ts1,ts2;

  //printf("Starting\n");

  gettimeofday(&ts1,NULL);
  for(int i=((args_t *)args)->iterations; i!=0; i--) {
    mutex->Lock();
    count_ptr[0]++;
    mutex->Unlock();
  }
  gettimeofday(&ts2,NULL);
  //printf("Inner time is %llu\n", getTimeUS(&ts2, &ts1));
  //Yuck. Want a simple way to pass back our inner time, but
  //pthreads wants a pointer and omp does whatever. Convert
  //our result to be a pointer and recast at the caller.
  return (void *) getTimeUS(&ts2, &ts1);
}
void * th_burnRW(void *argsIn) {

  MutexWrapper *mutex     = ((args_t *)argsIn)->mutex;
  uint64_t     *count_ptr = ((args_t *)argsIn)->count;
  struct timeval ts1,ts2;
  args_t *args = (args_t *)argsIn;


  //printf("Starting %d\n", args->id);
  int num_rd,num_wr,num_bad;
  num_rd=num_wr=num_bad=0;
  float cur_rw_ratio;
  cur_rw_ratio=args->rw_ratio+1.0;
  gettimeofday(&ts1,NULL);
  for(uint64_t i=0; i<args->iterations; i++) {

    if(cur_rw_ratio > args->rw_ratio) {
      //Write data
      int key =  (args->id<<24) | num_wr;
      int val =  (args->id<<24) + num_wr;
      //cout <<"Doing w lock on " << key << " " << args->id << " " << i << endl;
      args->mutex->WriterLock();

      (*args->rw_map)[key] = key;
      mutex->Unlock();
      num_wr++;
    } else {
      int spot = (rand() % num_wr);
      int key  =(args->id<<24) | spot;
      int eval = (args->id<<24)+spot;
      int  val =-1;

      //cout << "Doing r lock on key " << key << " id "<<args->id<<" spot "<<spot<<endl;
      args->mutex->ReaderLock();
      map<int,int>::iterator ii=args->rw_map->find(key);
      if(ii==args->rw_map->end()) {
        cerr << "Didn't find expected val in map? "<<args->id<<" "<<spot<<endl;
        exit(0);
        num_bad++;
      } else {
        val = ii->second;
        if(val != eval) {
          cerr << "Didn't get right val " << val << " vs "<<eval<<endl;
          num_bad++;
        }
      }
      args->mutex->Unlock();
      num_rd++;
    }
    cur_rw_ratio = ((float)(num_rd))/((float)num_wr);
  }
  gettimeofday(&ts2,NULL);
  //printf("Inner time is %llu\n", getTimeUS(&ts2, &ts1));
  //Yuck. Want a simple way to pass back our inner time, but
  //pthreads wants a pointer and omp does whatever. Convert
  //our result to be a pointer and recast at the caller.
  args->bad_count = num_bad;
  return (void *) getTimeUS(&ts2, &ts1);
}


TEST(Mutex, BurnThreaded) {

  struct timeval ts1,ts2;
  uint64_t us_sum=0;
  uint64_t count=0;
  args_t args;
  uint64_t num_threads=8;

  args.iterations = 100000;
  args.count = &count;


  #ifdef _IGNORE_LOCKS
  {
    args.mutex = faodel::GenerateMutex("none");
    string s = args.mutex->GetType();
    EXPECT_EQ("none", s);

    num_threads=1;
    gettimeofday(&ts1, NULL);
    th_burn(&args);
    gettimeofday(&ts2, NULL);
    delete args.mutex;
  }
  #endif

  #ifdef _OPENMP
  {
    agrs.mutex = faodel::GenerateMutex("openmp");
    string s = args.mutex->GetType();
    EXPECT_EQ("openmp", s);

    gettimeofday(&ts1, NULL);
    omp_set_dynamic(0);
    omp_set_num_threads(num_threads);
    #pragma omp parallel for reduction(+: us_sum)
    for(int i=0; i<num_threads; i++)
      us_sum = us_sum + (uint64_t) th_burn(&args);
    gettimeofday(&ts2, NULL);
    delete args.mutex;
  }
  #endif

  #ifdef _PTHREADS
  {
    pthread_t threads[num_threads];
    uint64_t call_times[num_threads];
    void *status=NULL;

    args.mutex = faodel::GenerateMutex("pthreads");
    string s = args.mutex->GetType();
    EXPECT_EQ("pthreads-lock", s);

    gettimeofday(&ts1, NULL);
    for(uint64_t i=0; i<num_threads; i++)
      pthread_create(&threads[i], 0, th_burn, (void *)&args);

    for(uint64_t i=0; i<num_threads; i++) {
      pthread_join(threads[i], &status);
      us_sum += (uint64_t)status;
      //printf("Status says %llu\n", (uint64_t)status);
    }
    gettimeofday(&ts2, NULL);
    delete args.mutex;
  }
  #endif


  EXPECT_EQ(count, num_threads*args.iterations);

  uint64_t us = getTimeUS(&ts2, &ts1);
  cout <<"Done. Threads: "<< num_threads
       <<" Count: "<< count
       <<" Ops: " << args.iterations
       <<" Time: "<< us
       <<" Rate: "<< (double)us/(double)args.iterations
       <<" InternalTime "<< us_sum/num_threads
       <<" InternalAvg: "<<(double)us_sum/(double)(args.iterations*num_threads)<<endl;
}




TEST(Mutex, BurnThreadedRW) {

  struct timeval ts1,ts2;
  uint64_t us_sum=0;
  uint64_t count=0;
  args_t args;
  uint64_t num_threads=8;
  map<int,int> rw_map;

  args.iterations = 100000;
  args.count = &count;
  args.rw_ratio = 5.0;
  args.rw_map = &rw_map;
  args.bad_count = 0;

  args_t args_array[num_threads];
  for(uint64_t i=0; i<num_threads; i++) {
    args_array[i] = args;
    args_array[i].id = i;
  }


  #ifdef _IGNORE_LOCKS
  {
    args.mutex = faodel::GenerateMutex("none");
    string s = args.mutex->GetType();
    EXPECT_EQ("none", s);

    num_threads=1;
    gettimeofday(&ts1, NULL);
    th_burn(&args_array[0]);
    gettimeofday(&ts2, NULL);
    delete args.mutex;
  }
  #endif

  #ifdef _OPENMP
  {
    args.mutex = faodel::GenerateMutex("openmp");
    string s = args.mutex->GetType();
    EXPECT_EQ("openmp", s);

    gettimeofday(&ts1, NULL);
    omp_set_dynamic(0);
    omp_set_num_threads(num_threads);
    #pragma omp parallel for reduction(+: us_sum)
    for(int i=0; i<num_threads; i++) {
      args_array[i].mutex = args.mutex;
      us_sum = us_sum + (uint64_t) th_burnRW(&args_array[i]);
    }
    gettimeofday(&ts2, NULL);
    delete args.mutex;

  }
  #endif

  #ifdef _PTHREADS
  {
    pthread_t threads[num_threads];
    uint64_t call_times[num_threads];
    void *status=NULL;

    //cout << "Pthreads\n";
    args.mutex = faodel::GenerateMutex("pthreads","rwlock");
    string s = args.mutex->GetType();
    EXPECT_EQ("pthreads-rwlock", s);

    gettimeofday(&ts1, NULL);
    for(uint64_t i=0; i<num_threads; i++) {
      args_array[i].mutex = args.mutex;
      pthread_create(&threads[i], 0, th_burnRW, (void *)&args_array[i]);
    }
    for(uint64_t i=0; i<num_threads; i++) {
      pthread_join(threads[i], &status);
      us_sum += (uint64_t)status;
      //printf("Status says %llu\n", (uint64_t)status);
    }
    gettimeofday(&ts2, NULL);
    delete args.mutex;
  }
  #endif



  for(uint64_t i=0;i<num_threads; i++) {
    EXPECT_EQ(0, args_array[i].bad_count);
  }

  uint64_t us = getTimeUS(&ts2, &ts1);

  cout <<"RW Done. Threads: "<< num_threads
       <<" Count: "<< count
       <<" Ops: " << args.iterations
       <<" Time: "<< us
       <<" Rate: "<< (double)us/(double)args.iterations
       <<" InternalTime "<< us_sum/num_threads
       <<" InternalAvg: "<<(double)us_sum/(double)(args.iterations*num_threads)<<endl;

}





TEST(Mutex, NoConflictThreaded) {

  struct timeval ts1,ts2;
  uint64_t us_sum=0;
  uint64_t num_threads=8;

  args_t args[num_threads];
  uint64_t counts[num_threads];
  uint64_t iterations=100000;


  #ifdef _PTHREADS
  {
    pthread_t threads[num_threads];
    uint64_t call_times[num_threads];
    void *status=NULL;

    for(uint64_t i=0; i<num_threads; i++) {
      args[i].iterations = iterations;
      counts[i] = 0;
      args[i].count = &counts[i];
      args[i].mutex = faodel::GenerateMutex("pthreads");
      string s = args[i].mutex->GetType();
      EXPECT_EQ("pthreads-lock", s);
    }

    gettimeofday(&ts1, NULL);
    for(uint64_t i=0; i<num_threads; i++)
      pthread_create(&threads[i], 0, th_burn, (void *)&args[i]);

    for(uint64_t i=0; i<num_threads; i++) {
      pthread_join(threads[i], &status);
      us_sum += (uint64_t)status;
      //printf("Status says %llu\n", (uint64_t)status);
    }
    gettimeofday(&ts2, NULL);
    for(uint64_t i=0; i<num_threads; i++) {
      delete args[i].mutex;
    }
  }
  #endif

  for(uint64_t i=0; i<num_threads; i++) {
    EXPECT_EQ(iterations, counts[i]);
  }

  uint64_t us = getTimeUS(&ts2, &ts1);

  cout <<"Done. Threads: "<< num_threads
       <<" Count: "<< counts[0]
       <<" Ops: " << iterations
       <<" Time: "<< us
       <<" Rate: "<< (double)us/(double)iterations
       <<" InternalTime "<< us_sum/num_threads
       <<" InternalAvg: "<<(double)us_sum/(double)(iterations*num_threads)<<endl;

}

TEST(Mutex, NoConflictThreadedRW) {

  struct timeval ts1,ts2;
  uint64_t us_sum=0;
  uint64_t num_threads=8;

  args_t args[num_threads];
  uint64_t counts[num_threads];
  uint64_t iterations=100000;


  #ifdef _PTHREADS
  {
    pthread_t threads[num_threads];
    uint64_t call_times[num_threads];
    void *status=NULL;

    for(uint64_t i=0; i<num_threads; i++) {
      args[i].iterations = iterations;
      counts[i] = 0;
      args[i].count = &counts[i];
      args[i].mutex = faodel::GenerateMutex("pthreads","rwlock");
      string s= args[i].mutex->GetType();
      EXPECT_EQ("pthreads-rwlock", s);
    }

    gettimeofday(&ts1, NULL);
    for(uint64_t i=0; i<num_threads; i++)
      pthread_create(&threads[i], 0, th_burn, (void *)&args[i]);

    for(uint64_t i=0; i<num_threads; i++) {
      pthread_join(threads[i], &status);
      us_sum += (uint64_t)status;
      //printf("Status says %llu\n", (uint64_t)status);
    }
    gettimeofday(&ts2, NULL);
    for(uint64_t i=0; i<num_threads; i++) {
      delete args[i].mutex;
    }
  }
  #endif

  for(uint64_t i=0; i<num_threads; i++) {
    EXPECT_EQ(iterations, counts[i]);
  }

  uint64_t us = getTimeUS(&ts2, &ts1);
  cout <<"Done. Threads: "<< num_threads
       <<" Count: "<< counts[0]
       <<" Ops: " << iterations
       <<" Time: "<< us
       <<" Rate: "<< (double)us/(double)iterations
       <<" InternalTime "<< us_sum/num_threads
       <<" InternalAvg: "<<(double)us_sum/(double)(iterations*num_threads)<<endl;


}





//=============================================================================
int main(int argc, char **argv) {

  ::testing::InitGoogleTest(&argc, argv);

  cout << MutexWrapperCompileTimeInfo();

  int rc = RUN_ALL_TESTS();

  return rc;
}
