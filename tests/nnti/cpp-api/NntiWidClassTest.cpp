// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <iostream>
#include <thread>
#include <vector>

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_wr.hpp"

#include "nnti/transports/null/null_transport.hpp"

const int num_wid=1024;

bool success=true;


class test_wid
: public nnti::datatype::nnti_work_id {
private:
    bool complete_;

private:
    test_wid(void);

public:
    test_wid(
        nnti::transports::transport *transport)
    : nnti_work_id(
        transport)
    {
        return;
    }
    test_wid(
        nnti::datatype::nnti_work_request &wr)
    : nnti_work_id(
        wr)
    {
        return;
    }

    virtual bool
    is_complete() const
    {
        return complete_;
    }
    void
    complete(
        bool c)
    {
        complete_ = c;
    }
};

static inline unsigned long
tv_to_ms(
    const struct timeval &tv)
{
    return ((unsigned long)tv.tv_sec * 1000000 + tv.tv_usec) / 1000;
}

void
run_test(
    nnti::datatype::nnti_work_id_queue wid_q)
{
    nnti::transports::null_transport  *transport = nullptr;
    nnti::datatype::nnti_work_request  wr(transport);
    std::vector<test_wid>              wid_source(num_wid, test_wid(wr));

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    for (auto i = 0; i < num_wid; ++i) {
        wid_q.push(&wid_source[i]);
    }
    for (auto i = 0; i < num_wid; ++i) {
        wid_source[i].complete(true);
    }
    for (auto i = 0; i < num_wid; ++i) {
        nnti::datatype::nnti_work_id *front_wid=wid_q.front();
        if (!front_wid->is_complete()) {
            std::cout << "expected front_wid to be complete (front_wid->is_complete() == " << front_wid->is_complete() << ")" << std::endl;
            success = false;
        }
        nnti::datatype::nnti_work_id *pop_wid=wid_q.pop();
        if (!pop_wid->is_complete()) {
            std::cout << "expected pop_wid to be complete (pop_wid->is_complete() == " << pop_wid->is_complete() << ")" << std::endl;
            success = false;
        }
        if (front_wid != pop_wid) {
             std::cout << "front_wid != pop_wid (" << front_wid << " != " << pop_wid << ")" << std::endl;
             success = false;
        }
    }
    if (!wid_q.empty()) {
        std::cout << "expected wid_q to be empty (wid_q.empty() == " << wid_q.empty() << ")" << std::endl;
        success = false;
    }

    gettimeofday(&tv1, NULL);
    std::cout << (tv_to_ms(tv1) - tv_to_ms(tv0)) << "ms" << std::endl;
}

int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiWidClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    nnti::datatype::nnti_work_id_queue wid_q;

    run_test(std::move(wid_q));

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
