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

#include "nnti/nnti_logger.hpp"


bool success=true;

int main(int argc, char *argv[])
{
    nnti::core::logger::init(sbl::severity_level::debug);
    nnti::core::logger::get_instance()->debug("NntiLoggerClassTest", "this is a debug message using get_instance()");
    nnti::core::logger::get_instance()->info("NntiLoggerClassTest", "this is a info message using get_instance()");
    nnti::core::logger::get_instance()->warning("NntiLoggerClassTest", "this is a warning message using get_instance()");
    nnti::core::logger::get_instance()->error("NntiLoggerClassTest", "this is a error message using get_instance()");
    nnti::core::logger::get_instance()->fatal("NntiLoggerClassTest", "this is a fatal message using get_instance()");

    log_debug("NntiLoggerClassTest", "this is a debug message using C macro");
    log_info("NntiLoggerClassTest", "this is a info message using C macro");
    log_warn("NntiLoggerClassTest", "this is a warning message using C macro");
    log_error("NntiLoggerClassTest", "this is a error message using C macro");
    log_fatal("NntiLoggerClassTest", "this is a fatal message using C macro");

    log_debug_stream("NntiLoggerClassTest") << "this is a debug message using C++ ostream";
    log_info_stream("NntiLoggerClassTest")  << "this is a info message using C++ ostream";
    log_warn_stream("NntiLoggerClassTest")  << "this is a warning message using C++ ostream";
    log_error_stream("NntiLoggerClassTest") << "this is a error message using C++ ostream";
    log_fatal_stream("NntiLoggerClassTest") << "this is a fatal message using C++ ostream";

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
