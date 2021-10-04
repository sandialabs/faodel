Release Information
===================

This file provides information about different releases of the faodel tools. 
Releases are named alphabetically and have a 4-digit ID associated with them
that designates the year and month of the release.

Fluid (1.2108.1)
----------------
- Summary: Trace record/playback, Stress tools, UDFs, and testing
- Release Improvements
  - Kelpie: Added Drop and RowInfo operations for remote use
  - Kelpie: Added ResultCollector to simplify async requests
  - Kelpie: New trace pool for client-side pool activities
  - Lunasa: Added DataObjectPacker for easier packing
  - Lunasa: Added StringObject for easier packing of text  
  - NNTI: Added support for RoCE
  - faodel-cli: config-info dumps out configuration options
  - faodel-cli: New playback option that can use traces
  - faodel-cli: New kblast option generates parallel kelpie traffic
  - faodel-stress: New tool for benchmarking CPU for non-net activities
  - OpBox: Ability to capture timing traces for ops
  - Backburner: New notification methods to decrease CPU usage  
- Significant User-Visible Changes:
  - Kelpie: kv_row/col_info_t replaced by object_info_t (smaller, simpler)
  - config: "ioms" are now "kelpie.ioms"
    config: backburner.notification_method for pipe, polling, sleep_polling
  - Examples cam now be built inside the build via BUILD_EXAMPLES
  - OpBoxStandard is now OpBoxDeprecatedStandard
- Experimental Features
  - kelpie 'Compute' allows server to perform computations on objects via UDFs
- Known Issues: todo


Excelsior! (1.1906.1)
---------------------
- Summary: Job-to-Job improvements via new cli tool
- Release Improvements
  - New faodel-cli tool for manipulating many things
    - Gets build/configure info (replaces faodel-info)
    - Start/stop services (dirman, kelpie)
    - Define/query/remove dirman resources
    - Put/get/list kelpie objects
    - New example/kelpie-cli script shows how to use
  - Support for ARM platform
  - NNTI adds On-Demand Paging capability
  - NNTI adds Cereal as alternative for serialization
  - NNTI has better detection and selection of IB devices
  - Fixes
    - SBL could segfault due to Boost if exit without calling finish
    - FAODEL couldn't be included in a larger project's cmake
    - LDO had a race condition in destructor
- Significant User-Visible Changes:
  - faodel-info and whookie tools replaced by faodel cli tool
  - Dirman's DirInfo "children" renamed to "members"
  - Faodel now has a package in the Spack develop branch
- Known Issues
  - FAODEL's libfabric transport is still experimental. It does not fully
    implement Atomics or Long Sends. While Kelpie does not require
    these operations, other OpBox-based applications may break
    without this support.
  - On Cray machines with the Aries interconnect, FAODEL can be overwhelmed
    by a sustained stream of sends larger than the MTU. To avoid this problem,
    the sender should limit itself to bursts of 32 long sends at a time.

DIO (1.1811.1)
--------------
- Summary: Build improvements to make FAODEL compatible with EMPIRE
- Release Improvements:
  - Kelpie has rank-folding table (RFT) pool
  - Kelpie has new experimental IOMs (IomHDF5, IomLevelDB)
  - New MPISyncStart service for auto-filling node ids in configurations
  - DirMan is now its own service (previously a child of opbox)
  - Lunasa has templated containers for storing a bundle of items in an LDO
    - GenericSequentialDataBundle : When data is accessed in order
    - GenericRandomDataBundle     : When data is accessed out of order
  - Users can now define whookies for rendering specific DataObject types
  - Bootstrap Start/Stops are much more robust
  - General build fixes for use with EMPIRE
- Significant User-Visible Changes:
  - Common and services directories renamed to faodel-common and faodel-services
  - Kelpie pools now have a "behavior" that controls how data is copied
  - Some components/tools changed directories. Check your includes
- Known Issues
  - Kelpie behavior setting may not be fully implemented on remote operations
  - Some tests hang when using OpenMPI with the openib BTL. Given that
    this BTL is deprecated, we recommend using OpenMPI with a different
    network component.  FAODEL has been tested with the TCP BTL and the
    psm, psm2, and libfabric MTLs.
  - FAODEL's libfabric transport is still experimental. It does not fully
    implement Atomics or Long Sends. While Kelpie does not require
    these operations, other OpBox-based applications may break
    without this support.
  - On Cray machines with the Aries interconnect, FAODEL can be overwhelmed
    by a sustained stream of sends larger than the MTU. To avoid this problem,
    the sender should limit itself to bursts of 32 long sends at a time.
  - This version does not have support for ARM8 or POWER cpus.

Cachet (1.1803.1)
-----------------
- Summary: Flattened repos, simplify builds, and do external release
- Release Improvements:
  - Quicker build system
  - Better stability with OpBox core
- Significant User Visible Changes:
  - Renamed gutties to common. Gutties namespace is now faodel.
  - Switched default OpBox core to threaded
- Known Issues
  - Similar to 0.1801.1
  - Kelpie is still a preview and may have stability issues
  - NNTI MPI transport may have stability issues

Belfry (0.1801.1)
-----------------
- Summary: Rename to FAODEL, expanded Opbox and Kelpie features
- Release Improvements:
  - Renamed repo and internals from data-warehouse to FAODEL
  - Simplified release process
  - Updated LDO API
  - Added Opbox threaded core
  - Unified network config tags at the Opbox level
  - New Kelpie core ("standard") has networking capabilities
  - Kelpie has a preliminary IOM interface for posix io
- Significant User Visible Changes:
  - The name external config file environment variable changed from 
    CONFIG to FAODEL_CONFIG
  - The Opbox network configurations tags are now the same for either 
    NNTI or libfabric
- Known Issues
  - Mutrino builds are still challenging
  - OpBox/Dirman does not allow new connections after one app shuts down
  - IOM API expected to need changes for other backends
  - Kelpie row operations not implemented
  - Libfabric wrapper does not implement Atomics and lacks maturity 
  - Sparse documentation
  
Amigo (0.1707.2)
----------------
- Summary: Bug fix release, for friendly users
- Release Improvements:
  - Improved stability of NNTI at large scale
  - Fixed include dir bug in DataWarehouseConfig.cmake
  - Fixed "always on" debug message in Kelpie
- Known Issues
  - Same as 1707.1

Amigo (0.1707.1)
----------------
- Summary: First packaged release, for friendly users
- Release Improvements:
  - Stable versions of SBL, Gutties, Whookie, NNTI, Lunasa, and Opbox
  - Experimental version of Kelpie (nonet)
  - Switched to Graith CMake modules
  - Initial doxygen and readme documentation
- Known Issues
  - Mutrino builds are still challenging
  - Kelpie only supports nonet core (for local testing)
  - OpBox/Dirman does not allow new connections after one app shuts down
  - Sparse documentation (especially examples)
