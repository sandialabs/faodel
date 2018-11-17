// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

 /*
 * SblSourceClassTest.c
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

#include "sbl/sbl_source.hh"
#include "sbl/sbl_stream.hh"


bool success=true;

void test_debug_console_stream() {
    sbl::stream  debug_stream(sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should see 5 messages from this test", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 5 messages from this test", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 5 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 5 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 5 messages from this test", 5);

    return;
}

void test_info_console_stream() {
    sbl::stream  info_stream(sbl::severity_level::info);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 4 messages from this test", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 4 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 4 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 4 messages from this test", 5);

    return;
}

void test_warning_console_stream() {
    sbl::stream  warning_stream(sbl::severity_level::warning);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 3 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 3 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 3 messages from this test", 5);

    return;
}

void test_error_console_stream() {
    sbl::stream  error_stream(sbl::severity_level::error);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should NOT see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 2 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 2 messages from this test", 5);

    return;
}

void test_fatal_console_stream() {
    sbl::stream  fatal_stream(sbl::severity_level::fatal);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should NOT see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should NOT see this", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 1 messages from this test", 5);

    return;
}

void test_single_file_stream() {
    sbl::stream  debug_stream("single-file-stream.log", sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should see 5 messages from this logger", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 5 messages from this logger", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 5 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 5 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 5 messages from this logger", 5);

    debug_stream.set_severity(sbl::severity_level::info);
    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 4 messages from this logger", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 4 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 4 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 4 messages from this logger", 5);

    debug_stream.set_severity(sbl::severity_level::warning);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 3 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 3 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 3 messages from this logger", 5);

    debug_stream.set_severity(sbl::severity_level::error);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you NOT should see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 2 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 2 messages from this logger", 5);

    debug_stream.set_severity(sbl::severity_level::fatal);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you NOT should see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you NOT should see this", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 1 messages from this logger", 5);

    return;
}

void test_stringstream_stream() {
    std::stringstream ss;

    sbl::stream  stream(ss, sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should see 5 messages from this logger", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 5 messages from this logger", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 5 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 5 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 5 messages from this logger", 5);

    //change the severity threshold.
    stream.set_severity(sbl::severity_level::info);
    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 4 messages from this logger", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 4 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 4 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 4 messages from this logger", 5);

    //change the severity threshold.
    stream.set_severity(sbl::severity_level::warning);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 3 messages from this logger", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 3 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 3 messages from this logger", 5);

    //change the severity threshold.
    stream.set_severity(sbl::severity_level::error);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you NOT should see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 2 messages from this logger", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 2 messages from this logger", 5);

    //change the severity threshold.
    stream.set_severity(sbl::severity_level::fatal);
    SBL_LOG(debug_source, "debug_source message #%d - you NOT should see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you NOT should see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you NOT should see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you NOT should see this", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 1 messages from this logger", 5);


    std::cout << ss.str();

    return;
}

void test_debug_file_stream() {
    sbl::stream  debug_stream("debug.log", sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should see 5 messages from this test", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 5 messages from this test", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 5 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 5 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 5 messages from this test", 5);

    return;
}

void test_info_file_stream() {
    sbl::stream  info_stream("info.log", sbl::severity_level::info);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should see 4 messages from this test", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 4 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 4 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 4 messages from this test", 5);

    return;
}

void test_warning_file_stream() {
    sbl::stream  warning_stream("warning.log", sbl::severity_level::warning);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should see 3 messages from this test", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 3 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 3 messages from this test", 5);

    return;
}

void test_error_file_stream() {
    sbl::stream  error_stream("error.log", sbl::severity_level::error);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should NOT see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should see 2 messages from this test", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 2 messages from this test", 5);

    return;
}

void test_fatal_file_stream() {
    sbl::stream  fatal_stream("fatal.log", sbl::severity_level::fatal);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG(debug_source, "debug_source message #%d - you should NOT see this", 1);
    SBL_LOG(info_source, "info_source message #%d - you should NOT see this", 2);
    SBL_LOG(warning_source, "warning_source message #%d - you should NOT see this", 3);
    SBL_LOG(error_source, "error_source message #%d - you should NOT see this", 4);
    SBL_LOG(fatal_source, "fatal_source message #%d - you should see 1 messages from this test", 5);

    return;
}

void test_stream_interface() {
    sbl::stream  test_stream("streaminterface.log", sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    sbl::source info_source(sbl::severity_level::info);
    sbl::source warning_source(sbl::severity_level::warning);
    sbl::source error_source(sbl::severity_level::error);
    sbl::source fatal_source(sbl::severity_level::fatal);

    SBL_LOG_STREAM(debug_source, "")     << "debug_source message #" << 1 << " - you should see 5 messages from this logger";
    SBL_LOG_STREAM(info_source, "")       << "info_source message #" << 2 << " - you should see 5 messages from this logger";
    SBL_LOG_STREAM(warning_source, "") << "warning_source message #" << 3 << " - you should see 5 messages from this logger";
    SBL_LOG_STREAM(error_source, "")     << "error_source message #" << 4 << " - you should see 5 messages from this logger";
    SBL_LOG_STREAM(fatal_source, "")     << "fatal_source message #" << 5 << " - you should see 5 messages from this logger";

    test_stream.set_severity(sbl::severity_level::info);
    SBL_LOG_STREAM(debug_source, "")     << "debug_source message #" << 1 << " - you should NOT see this";
    SBL_LOG_STREAM(info_source, "")       << "info_source message #" << 2 << " - you should see 4 messages from this logger";
    SBL_LOG_STREAM(warning_source, "") << "warning_source message #" << 3 << " - you should see 4 messages from this logger";
    SBL_LOG_STREAM(error_source, "")     << "error_source message #" << 4 << " - you should see 4 messages from this logger";
    SBL_LOG_STREAM(fatal_source, "")     << "fatal_source message #" << 5 << " - you should see 4 messages from this logger";

    test_stream.set_severity(sbl::severity_level::warning);
    SBL_LOG_STREAM(debug_source, "")     << "debug_source message #" << 1 << " - you NOT should see this";
    SBL_LOG_STREAM(info_source, "")       << "info_source message #" << 2 << " - you NOT should see this";
    SBL_LOG_STREAM(warning_source, "") << "warning_source message #" << 3 << " - you should see 3 messages from this logger";
    SBL_LOG_STREAM(error_source, "")     << "error_source message #" << 4 << " - you should see 3 messages from this logger";
    SBL_LOG_STREAM(fatal_source, "")     << "fatal_source message #" << 5 << " - you should see 3 messages from this logger";

    test_stream.set_severity(sbl::severity_level::error);
    SBL_LOG_STREAM(debug_source, "")     << "debug_source message #" << 1 << " - you NOT should see this";
    SBL_LOG_STREAM(info_source, "")       << "info_source message #" << 2 << " - you NOT should see this";
    SBL_LOG_STREAM(warning_source, "") << "warning_source message #" << 3 << " - you NOT should see this";
    SBL_LOG_STREAM(error_source, "")     << "error_source message #" << 4 << " - you should see 2 messages from this logger";
    SBL_LOG_STREAM(fatal_source, "")     << "fatal_source message #" << 5 << " - you should see 2 messages from this logger";

    test_stream.set_severity(sbl::severity_level::fatal);
    SBL_LOG_STREAM(debug_source, "")     << "debug_source message #" << 1 << " - you NOT should see this";
    SBL_LOG_STREAM(info_source, "")       << "info_source message #" << 2 << " - you NOT should see this";
    SBL_LOG_STREAM(warning_source, "") << "warning_source message #" << 3 << " - you NOT should see this";
    SBL_LOG_STREAM(error_source, "")     << "error_source message #" << 4 << " - you NOT should see this";
    SBL_LOG_STREAM(fatal_source, "")     << "fatal_source message #" << 5 << " - you should see 1 messages from this logger";

    return;
}

int main(int argc, char *argv[])
{
    test_debug_console_stream();
    test_info_console_stream();
    test_warning_console_stream();
    test_error_console_stream();
    test_fatal_console_stream();

    test_debug_file_stream();
    test_info_file_stream();
    test_warning_file_stream();
    test_error_file_stream();
    test_fatal_file_stream();

    test_stringstream_stream();

    test_stream_interface();

    test_single_file_stream();

    if (success)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return (success ? 0 : 1 );
}
