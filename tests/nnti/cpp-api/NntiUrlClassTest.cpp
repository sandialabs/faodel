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

#include "nnti/nnti_url.hpp"

bool success=true;


int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiUrlClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    nnti::core::nnti_url *url=NULL;

    url = new nnti::core::nnti_url("http://localhost");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost/");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost/nnti");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost/nnti?qp_num=3142&cmd_size=1218");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost/nnti/connect");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost/nnti/connect?qp_num=3142&cmd_size=1218");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:NaN");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015/");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015/nnti");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015/nnti?qp_num=3142&cmd_size=1218");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015/nnti/connect");
    std::cout << *url;
    delete url;

    url = new nnti::core::nnti_url("http://localhost:2015/nnti/connect?qp_num=3142&cmd_size=1218");
    std::cout << *url;
    delete url;

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
