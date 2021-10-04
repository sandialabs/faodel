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

#include "nnti/nnti_pid.hpp"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_url.hpp"

bool success=true;


int main(int argc, char *argv[])
{
    std::string          str;
    nnti::core::nnti_url url;
    NNTI_process_id_t    pid;

    std::stringstream logfile;
    logfile << "NntiPidClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    str = std::string("ib://localhost:2015");
    std::cout << "original URL string is " << str << std::endl;
    url = nnti::core::nnti_url(str);
    std::cout << "URL object is " << url.url() << std::endl;

    pid = nnti::datatype::nnti_pid::to_pid(str);
    printf("pid from URL string is %016lX\n", pid);
    str = nnti::datatype::nnti_pid::to_url(pid);
    std::cout << "URL reconstituted from pid is " << str << std::endl;

    pid = nnti::datatype::nnti_pid::to_pid(url);
    printf("pid from URL object is %016lX\n", pid);
    str = nnti::datatype::nnti_pid::to_url(pid);
    std::cout << "URL reconstituted from pid is " << str << std::endl;


    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
