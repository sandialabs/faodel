FAODEL-Common: Support Code for FAODEL
======================================

The common directory in FAODEL contains componentst that are used in
different libraries in the FAODEL stack. The main components of
interest are:

- **Data Types**: Common provides a few common data types that are
    used throughout FAODEL, including:
    - nodeid_t: Concise id for connecting to any node in the system
    - bucket_t: Hashed value of a string, used for data namespaces
    - ResourceURL: String for referencing different system resources
    - DirectoryInfo: Provides a way to describe info about a FAODEL
      resource (eg, url and info, plus any members of this resource).

- **Configuration**: Configuration is a class for defining how to
    configure different components in a runtime. It uses a key/value
    notation, and has a basic understanding of paths in the keys to
    help assign values to components.

- **Bootstrap**: Bootstrap is a tool starting/stopping different
    services that are part of the FAODEL. Bootstrap lets users
    define dependencies between components in order to ensure that they
    are started/stopped in the correct order.

- **Debug**: Debug contains asserts and debugging function to help
    internal FAODEL developers track down problems.

- **InfoInterface**: An interface for presenting text information about a
    specific component (and it's children) back to the user.

- **LoggingInterface**: An interface for passing debug/information
    info back to the user. This interface is configured at compile time
    to route information to either stdout or the Simple Boost
    Library (SBL).

- **MutexWrapper**: MutexWrapper provides a simple way to implement
    different kinds of mutexs (plain or reader/writer) on different threading
    libraries.

- **SerializationHelpers**: A few helpers to make it easier to pack
    data structures using Boost's serialization library.

- **StringHelpers**: A few helpers for common string operations.

More information for each of these components is available in the
Doxygen-generated docs.




Build and Configuration Options
===============================

Build Dependencies
------------------

Common has the following build dependencies:

| Dependency | Information                           |
| ---------- | ------------------------------------- |
| FAODEL:SBL | Uses logging capabilities for boost   |
| Boost      | Uses boost, system, and serialization |


Compile-Time Options
--------------------

Common uses the following flags at compile time.

| CMake Flag             | Values                 |Description                                                                      |
| ---------------------- | -----------------------|-------------------------------------------------------------------------------- |
| Faodel_LOGGING_METHOD  | stdout, sbl, disabled  | Determines where the logging interface writes. Select disabled to optimize away |
| Faodel_THREADING_MODEL | pthreads, openmp       | Selects how mutexes are implemented. For future use. Only select PTHREADS       |


Run-Time Options
----------------

The following options are parsed out of a Configuration by different
components in Common:

| Property                                | Type        | Default | Description                                  |
| --------------------------------------  | ----------- | ------- | -------------------------------------------- |
| bootstrap.debug                         | boolean     | false   | Display debug messages during bootstrap      |
| bootstrap.show_config_at_init           | boolean     | false   | Display config used at init time             |
| bootstrap.halt_on_shutdown              | boolean     | false   | Do infinite-wait instead of exit at shutdown |
| bootstrap.status_on_shutdown            | boolean     | false   | Dump an ok message on successful exit        |
| bootstrap.exit_on_errors                | boolean     | false   | Exit on errors instead of throwing exception |
| bootstrap.sleep_seconds_before_shutdown | int         | 0       | Delay Finish shutdown for specified seconds  |
| mpisyncstop.enable                      | boolean     | false   | Perform an mpi barrier before Finish         |
| config.additional_files                 | string list | ""      | Load additional info for listed files        |
| config.purge                            | boolean     | false   | Removes all tags and reloads current config  |

Note: There are multiple suffixes for config.additional_files that can
be used to control how additional config files are loaded:

| Property                                    | Behavior                                                                 |
| --------                                    | --------                                                                 |
| config.additional_files.if_exist            | Do not exit if the specified file does not exist                         |
| config.additional_files.env_name            | Retrieve list of files from specified environment variable               |
| config.additional_files.env_name.if_defined | Do not exit if the specified files from environment variable don't exist |

