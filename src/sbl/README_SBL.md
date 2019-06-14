# SBL: A Simplified Interface to Boost.Log

Simplified Boost.Log (SBL) provides an easy to use interface to 
Boost.Log.  Many key features of Boost.Log are available including 
multiple sinks, application created channels, per-sink severity 
filter and per-channel severity filters.

## sbl::severity_level enum

Nothing exciting here.  Sets the threshold for an sbl::stream and 
the severity of a log message.  Values are:

    debug, info, warning, error, fatal


## sbl::stream class

The sbl::stream class represents a destination for log messages.  
Each stream has a logger_id that ties it to a source 
and limits the message that pass through the filter.
Each stream has a severity_level that acts as a threshold to 
limit the types of messages delivered to this stream.  A severity 
is assigned when the stream is created, but can be updated later.  
An application can also add channels that have their own severity 
level.  When the sink applies the severity filter, the channel 
filter takes precedence over the sink severity.  At this time, 
channels cannot be removed.  When the sbl::stream object is 
destroyed, the Boost.Log sink is removed so no more message will 
be delivered to the destination. 

    // write log messages to the console
    stream(
        const severity_level severity);

    // open filename (overwrite not append) and write log messages to it
    stream(
        const std::string    filename,
        const severity_level severity);

    // write to an existng std::ostream (including std::stringstream)
    stream(
        std::ostream&        stream,
        const severity_level severity);


## sbl::source class

The sbl::source class publishes log messages to all sbl::stream 
objects.  When the sbl::source is created, it is assigned a severity 
which is fixed for the life of the source.  Each source has a 
logger_id attribute that is applied to each message and ties it to 
a stream.  Multiple sources should be created to generate message 
with different severities.  sbl::source has three methods to output 
log messages.  

    // output with default logger_id
    source (
        severity_level  severity);
    // output with a logger_id
    source (
        uint64_t       logger_id,
        severity_level severity);


    // output a simple log message to a channel
    void log(
        const char *channel,
        const char *msg);

    // add a prefix to your log message 
    void log(
        const char *channel,
        const char *prefix,
        const char *msg);

    // pairs with the SBL_LOG() macro.
    void log(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);


## sbl::logger class

The sbl::logger class creates a single sbl::stream plus an 
sbl::source for each severity level.  The sbl::stream and 
sbl::sources are assigned the same unique logger_id so that log 
message from this sbl::logger are seen by other sbl::streams.

    // write log messages to the console
    logger (
        const severity_level severity);

    // open filename (overwrite not append) and write log messages to it
    logger (
        const std::string    filename,
        const severity_level severity);

    // write to an existng std::ostream (including std::stringstream)
    logger (
        std::ostream&        stream,
        const severity_level severity);

    // generate a simple severity_level::debug message
    void debug(
        const char *channel,
        const char *msg,
        ...);
    // generate a severity_level::debug message with more info
    void debug(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);

    // generate a simple severity_level::info message
    void info(
        const char *channel,
        const char *msg,
        ...);
    // generate a severity_level::info message with more info
    void info(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);

    // generate a simple severity_level::warning message
    void warning(
        const char *channel,
        const char *msg,
        ...);
    // generate a severity_level::warning message with more info
    void warning(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);

    // generate a simple severity_level::error message
    void error(
        const char *channel,
        const char *msg,
        ...);
    // generate a severity_level::error message with more info
    void error(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);

    // generate a simple severity_level::fatal message
    void fatal(
        const char *channel,
        const char *msg,
        ...);
    // generate a severity_level::fatal message with more info
    void fatal(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);


## Logging Macros

The SBL_LOG() is meant to easy the transition away from the NNTI 
logger.  It's parameters are an sbl::source object, a printf() style 
message and a variable number of parameters that match the message.  
The resulting log message will be formatted mostly like the NNTI 
logger.

The SBL_LOG_STREAM() creates an object that is compatible with 
std::ostream so you can generate log message with stream operators.

Example:

    sbl::stream test_stream("myapp.log", sbl::severity_level::debug);
    sbl::source debug_source(sbl::severity_level::debug);
    SBL_LOG_STREAM(debug_source) << "debug message #" << 1;

## An Example

    #include "sbl/sblConfig.h"

    #include <unistd.h>
    #include <string.h>
    #include <pthread.h>

    #include <iostream>
    #include <sstream>
    #include <thread>
    #include <map>

    #include "sbl/sbl_logger.hh"
    
    int main(int argc, char *argv[])
    {
        sbl::stream console_stream(sbl::severity_level::fatal);           // fatal messages go to the console
        sbl::stream file_stream("debug.log", sbl::severity_level::debug); // all messages go to this file
        std::stringstream ss;
        sbl::stream buffer_stream(ss, sbl::severity_level::warning); // warning, error and fatal messages go to a stringstream 
                                                                     // that whookie can put in replies
        
        sbl::source debug_source(sbl::severity_level::debug);   // these will only go to the file_stream
        sbl::source fatal_source(sbl::severity_level::fatal);  // these will go to all streams
        
        SBL_LOG(debug_source, "start testing");
        
        std::map<std::string,int> *mymap = new std::map<std::string,int>();
        //
        // force a failure
        //
        mymap = nullptr;
        if (mymap == nullptr) {
            SBL_LOG(fatal_source, "couldn't create a map.  Aborting.");
            goto fail;
        }
        
        // do stuff
        
        SBL_LOG(debug_source, "done testing");
        
        return(0);
    fail:
        std::cout << " ==== begin stringstream ====" << std::endl << ss.str()  << " ==== end stringstream ====" << std::endl;
        return(1);
    }


    bash$ BOOST_PATH=${HOME}/local
    bash$ SBL_PATH=${HOME}/projects/atdm/install
    bash$ g++ -std=c++11 -I${BOOST_PATH}/include -I${SBL_PATH}/include -DBOOST_LOG_DYN_LINK -o sbltest.exe ./sbltest.cpp ${BOOST_PATH}/lib/libboost_system.so ${BOOST_PATH}/lib/libboost_log.so ${BOOST_PATH}/lib/libboost_log_setup.so ${BOOST_PATH}/lib/libboost_thread.so -pthread -Wl,-rpath,${BOOST_PATH}/lib
    bash$ 
    bash$ ./sbltest.exe
    2: <FATAL> [priority] [main:sbltest.cpp:33:t23349]: couldn't create a map.  Aborting.
    ==== begin stringstream ====
    2: <FATAL> [priority] [main:sbltest.cpp:33:t23349]: couldn't create a map.  Aborting.
    ==== end stringstream ====
    bash$ 
    bash$ cat debug.log 
    1: <DEBUG> [general] [main:sbltest.cpp:25:t23349]: start testing
    2: <FATAL> [priority] [main:sbltest.cpp:33:t23349]: couldn't create a map.  Aborting.
    bash$





