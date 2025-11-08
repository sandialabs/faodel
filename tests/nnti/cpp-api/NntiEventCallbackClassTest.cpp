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

#include "nnti/nnti_threads.h"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_callback.hpp"

#include "nnti/transports/null/null_transport.hpp"

bool success=true;

NNTI_result_t callback_func(NNTI_event_t *event, void *context)
{
    std::cout << "This is a callback function.  My parameters are event(" << (void*)event << ") and context(" << (void*)context << ")." << std::endl;
    return NNTI_OK;
}

class callback {
public:
    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        std::cout << "This is a callback object.  My parameters are event(" << (void*)event << ") and context(" << (void*)context << ")." << std::endl;
        return NNTI_OK;
    }
};

int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiEventCallbackClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::warning);

    nnti::transports::null_transport  *transport = nullptr;

    callback callback_obj;

    nnti::datatype::nnti_event_callback func_cb(transport, callback_func);
    nnti::datatype::nnti_event_callback obj_cb(transport, callback_obj);

    func_cb.invoke(nullptr, nullptr);
    obj_cb.invoke(nullptr, nullptr);

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
