// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

 /*
 * SblLoggerClassTest.c
 *
 *  Created on: July 13, 2015
 *      Author: thkorde
 */

#include "faodelConfig.h"

#include "sbl/sbl_boost_headers.hh"

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <iostream>
#include <sstream>
#include <thread>

#include "sbl/sbl_logger.hh"


bool success=true;

void test_single_logger() {
    sbl::logger  l("single-logger.log", sbl::severity_level::debug);

    l.debug("", "logger.debug message #%d - you should see 5 messages from this logger", 1);
    l.info("", "logger.info message #%d - you should see 5 messages from this logger", 2);
    l.warning("", "logger.warning message #%d - you should see 5 messages from this logger", 3);
    l.error("", "logger.error message #%d - you should see 5 messages from this logger", 4);
    l.fatal("", "logger.fatal message #%d - you should see 5 messages from this logger", 5);

    l.set_severity(sbl::severity_level::info);
    l.debug("", "logger.debug message #%d - you should NOT see this", 1);
    l.info("", "logger.info message #%d - you should see 4 messages from this logger", 2);
    l.warning("", "logger.warning message #%d - you should see 4 messages from this logger", 3);
    l.error("", "logger.error message #%d - you should see 4 messages from this logger", 4);
    l.fatal("", "logger.fatal message #%d - you should see 4 messages from this logger", 5);

    l.set_severity(sbl::severity_level::warning);
    l.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l.info("", "logger.info message #%d - you NOT should see this", 2);
    l.warning("", "logger.warning message #%d - you should see 3 messages from this logger", 3);
    l.error("", "logger.error message #%d - you should see 3 messages from this logger", 4);
    l.fatal("", "logger.fatal message #%d - you should see 3 messages from this logger", 5);

    l.set_severity(sbl::severity_level::error);
    l.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l.info("", "logger.info message #%d - you NOT should see this", 2);
    l.warning("", "logger.warning message #%d - you NOT should see this", 3);
    l.error("", "logger.error message #%d - you should see 2 messages from this logger", 4);
    l.fatal("", "logger.fatal message #%d - you should see 2 messages from this logger", 5);

    l.set_severity(sbl::severity_level::fatal);
    l.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l.info("", "logger.info message #%d - you NOT should see this", 2);
    l.warning("", "logger.warning message #%d - you NOT should see this", 3);
    l.error("", "logger.error message #%d - you NOT should see this", 4);
    l.fatal("", "logger.fatal message #%d - you should see 1 messages from this logger", 5);

    /*
     * The fallback severity is still fatal, but these tests will use the channel severity.
     */

    l.set_channel_severity("my_channel", sbl::severity_level::debug);
    l.debug("my_channel", "logger.debug message #%d - you should see 5 messages from this logger", 1);
    l.info("my_channel", "logger.info message #%d - you should see 5 messages from this logger", 2);
    l.warning("my_channel", "logger.warning message #%d - you should see 5 messages from this logger", 3);
    l.error("my_channel", "logger.error message #%d - you should see 5 messages from this logger", 4);
    l.fatal("my_channel", "logger.fatal message #%d - you should see 5 messages from this logger", 5);

    l.set_channel_severity("my_channel", sbl::severity_level::info);
    l.debug("my_channel", "logger.debug message #%d - you should NOT see this", 1);
    l.info("my_channel", "logger.info message #%d - you should see 4 messages from this logger", 2);
    l.warning("my_channel", "logger.warning message #%d - you should see 4 messages from this logger", 3);
    l.error("my_channel", "logger.error message #%d - you should see 4 messages from this logger", 4);
    l.fatal("my_channel", "logger.fatal message #%d - you should see 4 messages from this logger", 5);

    l.set_channel_severity("my_channel", sbl::severity_level::warning);
    l.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l.warning("my_channel", "logger.warning message #%d - you should see 3 messages from this logger", 3);
    l.error("my_channel", "logger.error message #%d - you should see 3 messages from this logger", 4);
    l.fatal("my_channel", "logger.fatal message #%d - you should see 3 messages from this logger", 5);

    l.set_channel_severity("my_channel", sbl::severity_level::error);
    l.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l.warning("my_channel", "logger.warning message #%d - you NOT should see this", 3);
    l.error("my_channel", "logger.error message #%d - you should see 2 messages from this logger", 4);
    l.fatal("my_channel", "logger.fatal message #%d - you should see 2 messages from this logger", 5);

    l.set_channel_severity("my_channel", sbl::severity_level::fatal);
    l.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l.warning("my_channel", "logger.warning message #%d - you NOT should see this", 3);
    l.error("my_channel", "logger.error message #%d - you NOT should see this", 4);
    l.fatal("my_channel", "logger.fatal message #%d - you should see 1 messages from this logger", 5);

    return;
}

void test_multi_logger() {
    sbl::logger  l1("multi-logger-debug.log", sbl::severity_level::debug);
    sbl::logger  l2("multi-logger-info.log", sbl::severity_level::info);
    sbl::logger  l3("multi-logger-warn.log", sbl::severity_level::warning);
    sbl::logger  l4("multi-logger-error.log", sbl::severity_level::error);
    sbl::logger  l5("multi-logger-fatal.log", sbl::severity_level::fatal);

    l1.debug("", "logger.debug message #%d - you should see 5 messages from this logger", 1);
    l1.info("", "logger.info message #%d - you should see 5 messages from this logger", 2);
    l1.warning("", "logger.warning message #%d - you should see 5 messages from this logger", 3);
    l1.error("", "logger.error message #%d - you should see 5 messages from this logger", 4);
    l1.fatal("", "logger.fatal message #%d - you should see 5 messages from this logger", 5);

    l2.debug("", "logger.debug message #%d - you should NOT see this", 1);
    l2.info("", "logger.info message #%d - you should see 4 messages from this logger", 2);
    l2.warning("", "logger.warning message #%d - you should see 4 messages from this logger", 3);
    l2.error("", "logger.error message #%d - you should see 4 messages from this logger", 4);
    l2.fatal("", "logger.fatal message #%d - you should see 4 messages from this logger", 5);

    l3.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l3.info("", "logger.info message #%d - you NOT should see this", 2);
    l3.warning("", "logger.warning message #%d - you should see 3 messages from this logger", 3);
    l3.error("", "logger.error message #%d - you should see 3 messages from this logger", 4);
    l3.fatal("", "logger.fatal message #%d - you should see 3 messages from this logger", 5);

    l4.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l4.info("", "logger.info message #%d - you NOT should see this", 2);
    l4.warning("", "logger.warning message #%d - you NOT should see this", 3);
    l4.error("", "logger.error message #%d - you should see 2 messages from this logger", 4);
    l4.fatal("", "logger.fatal message #%d - you should see 2 messages from this logger", 5);

    l5.debug("", "logger.debug message #%d - you NOT should see this", 1);
    l5.info("", "logger.info message #%d - you NOT should see this", 2);
    l5.warning("", "logger.warning message #%d - you NOT should see this", 3);
    l5.error("", "logger.error message #%d - you NOT should see this", 4);
    l5.fatal("", "logger.fatal message #%d - you should see 1 messages from this logger", 5);

    /*
     * The fallback severity is set to fatal, but these tests will use the channel severity.
     */

    l1.set_severity(sbl::severity_level::fatal);
    l2.set_severity(sbl::severity_level::fatal);
    l3.set_severity(sbl::severity_level::fatal);
    l4.set_severity(sbl::severity_level::fatal);
    l5.set_severity(sbl::severity_level::fatal);

    l1.set_channel_severity("my_channel", sbl::severity_level::debug);
    l1.debug("my_channel", "logger.debug message #%d - you should see 5 messages from this logger", 1);
    l1.info("my_channel", "logger.info message #%d - you should see 5 messages from this logger", 2);
    l1.warning("my_channel", "logger.warning message #%d - you should see 5 messages from this logger", 3);
    l1.error("my_channel", "logger.error message #%d - you should see 5 messages from this logger", 4);
    l1.fatal("my_channel", "logger.fatal message #%d - you should see 5 messages from this logger", 5);

    l2.set_channel_severity("my_channel", sbl::severity_level::info);
    l2.debug("my_channel", "logger.debug message #%d - you should NOT see this", 1);
    l2.info("my_channel", "logger.info message #%d - you should see 4 messages from this logger", 2);
    l2.warning("my_channel", "logger.warning message #%d - you should see 4 messages from this logger", 3);
    l2.error("my_channel", "logger.error message #%d - you should see 4 messages from this logger", 4);
    l2.fatal("my_channel", "logger.fatal message #%d - you should see 4 messages from this logger", 5);

    l3.set_channel_severity("my_channel", sbl::severity_level::warning);
    l3.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l3.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l3.warning("my_channel", "logger.warning message #%d - you should see 3 messages from this logger", 3);
    l3.error("my_channel", "logger.error message #%d - you should see 3 messages from this logger", 4);
    l3.fatal("my_channel", "logger.fatal message #%d - you should see 3 messages from this logger", 5);

    l4.set_channel_severity("my_channel", sbl::severity_level::error);
    l4.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l4.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l4.warning("my_channel", "logger.warning message #%d - you NOT should see this", 3);
    l4.error("my_channel", "logger.error message #%d - you should see 2 messages from this logger", 4);
    l4.fatal("my_channel", "logger.fatal message #%d - you should see 2 messages from this logger", 5);

    l5.set_channel_severity("my_channel", sbl::severity_level::fatal);
    l5.debug("my_channel", "logger.debug message #%d - you NOT should see this", 1);
    l5.info("my_channel", "logger.info message #%d - you NOT should see this", 2);
    l5.warning("my_channel", "logger.warning message #%d - you NOT should see this", 3);
    l5.error("my_channel", "logger.error message #%d - you NOT should see this", 4);
    l5.fatal("my_channel", "logger.fatal message #%d - you should see 1 messages from this logger", 5);

    return;
}

int main(int argc, char *argv[])
{
    test_single_logger();

    test_multi_logger();

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
