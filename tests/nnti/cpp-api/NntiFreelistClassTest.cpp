// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"
#include "nnti/nntiConfig.h"

#include <unistd.h>

#include <iostream>
#include <thread>

#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_freelist.hpp"


bool success=true;

static const auto freelist_size = 1024;
static const auto test_iters    = 10000;
static const auto num_workers   = 5;

std::atomic<int> events_popped(0);
std::atomic<int> events_pushed(0);

class Worker {
private:
    nnti::core::nnti_freelist<NNTI_event_t*> *fl_;

public:
    Worker(nnti::core::nnti_freelist<NNTI_event_t*> *fl)
    {
        fl_ = fl;
        return;
    }

    void
    operator()()
    {
        for (auto i=0;i < test_iters;i++) {
            NNTI_event_t *e = nullptr;
            while (!fl_->pop(e)) {
                std::this_thread::yield();
            }
            assert(e);
            events_popped.fetch_add(1);
            fl_->push(e);
            events_pushed.fetch_add(1);
        }
    }
};

static inline unsigned long
tv_to_ms(
    const struct timeval &tv)
{
    return ((unsigned long)tv.tv_sec * 1000000 + tv.tv_usec) / 1000;
}

void
test1()
{
    std::thread workers[num_workers];
    nnti::core::nnti_freelist<NNTI_event_t*> fl(freelist_size);

    for (auto i=0;i<freelist_size;i++) {
        NNTI_event_t *e = new NNTI_event_t;
        fl.push(e);
    }

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    for (auto i = 0; i < num_workers; ++i) {
        workers[i] = std::thread(Worker(&fl));
    }

    for (auto i = 0; i < num_workers; ++i) {
        workers[i].join();
    }

    gettimeofday(&tv1, NULL);
    std::cout << (tv_to_ms(tv1) - tv_to_ms(tv0)) << "ms" << std::endl;

    std::cout << "total popped = " << events_popped << std::endl;
    std::cout << "total pushed = " << events_pushed << std::endl;
    if (events_pushed != events_popped) {
        success = false;
    }
}

int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiFreelistClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    test1();

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
