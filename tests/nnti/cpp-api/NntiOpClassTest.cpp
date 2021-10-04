// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
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
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/null/null_transport.hpp"

const int num_op=1024;

bool success=true;


class test_op
: public nnti::core::nnti_op {
private:
    test_op(void);

public:
    test_op(
        nnti::transports::transport  *transport,
        nnti::datatype::nnti_work_id *wid)
    : nnti_op(
        wid)
    {
        return;
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
    nnti::core::nnti_op_queue &op_q,
    nnti::core::nnti_op_map   &op_map)
{
    nnti::transports::null_transport  *transport = nullptr;
    nnti::datatype::nnti_work_id       wid(transport);
    std::vector<test_op*>              op_source;

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    for (auto i = 0; i < num_op; ++i) {
        op_source.push_back(new test_op(transport, &wid));
        op_q.push(op_source[i]);
        op_map.insert(op_source[i]);
    }
    for (auto i = 0; i < num_op; ++i) {
        nnti::core::nnti_op *front_op=op_q.front();
        nnti::core::nnti_op *pop_op=op_q.pop();

        nnti::core::nnti_op *get_op=op_map.get(front_op->id());
        nnti::core::nnti_op *rm_op=op_map.remove(front_op);

        if (front_op != get_op) {
             std::cout << "front_op != get_op (" << front_op << " != " << get_op << ")" << std::endl;
             success = false;
        }
        if (front_op != rm_op) {
             std::cout << "front_op != rm_op (" << front_op << " != " << rm_op << ")" << std::endl;
             success = false;
        }
        if (front_op != pop_op) {
             std::cout << "front_op != pop_op (" << front_op << " != " << pop_op << ")" << std::endl;
             success = false;
        }
    }
    if (!op_q.empty()) {
        std::cout << "expected op_q to be empty (op_q.empty() == " << op_q.empty() << ")" << std::endl;
        success = false;
    }

    gettimeofday(&tv1, NULL);
    std::cout << (tv_to_ms(tv1) - tv_to_ms(tv0)) << "ms" << std::endl;
}

int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiOpClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    nnti::core::nnti_op_queue op_q;
    nnti::core::nnti_op_map   op_map;

    run_test(op_q, op_map);

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
