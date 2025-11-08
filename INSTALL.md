Faodel Installation
===================
The Faodel software is written in C++11 and uses CMake for building. Users must
install a small number of third-party libraries (TPLs) to build Faodel.

Quickstart
----------
Faodel can now be built with the [Spack package manager](https://spack.io). The easiest way to build Faodel and all its dependencies is through:
```
spack install faodel
```
Create a config file:
```
export FAODEL_CONFIG=~/my-faodel.conf
vi $FAODEL_CONFIG
```
Put basic settings in your $FAODEL_CONFIG file:
```
whookie.interfaces ib0,eth0   # list of ip interface where web info will live
net.transport.name mpi        # The HPC nic type, eg infiniband, ugni
whookie.info true             # Print the web address to stdout at boot    
bootstrap.debug true          # Print more detailed debug messages about boot
                              # lunasa,opbox,kelpie,dirman all have debug/info 
```
Then use the `faodel` tool to run services, put/get data, etc:
```
faodel help   # Show all the options
faodel binfo  # Verify your build looks right and devices found
faodel cinfo  # Verify your config looks right

# Create a pool in one window, then put/get/list data in another
mpirun -np 4 faodel all-in-one "dht:/my/dht ALL"
faodel kput -p /my/dht -k bob -f myfile.txt
faodel kget -p /my/dht -k bob
faodel klist -p /my/dht -k "b*"
```
When you're ready to connect to Faodel from your own code, look 
in `faodel/examples/kelpie`.


Prerequisites
-------------
Faodel has a small number of prerequisites for building. A target platform
should have:

- C++11 Compiler: The current implementation is typically built with GCC and
  requires version 4.8.5 or higher in order to use C++11 features.
- CMake 3.8
- Doxygen

Third-Party Libraries (TPLs)
----------------------------
Several TPLs are required in this project:

- Boost (1.60 or later)
- GoogleTest
- libfabric (optional)

Notes for building TPLs are included int the "Building Third-Party Libraries"
section later in this document.

Configuring and Building Faodel
-------------------------------
Faodel uses a standard CMake workflow for configuring, building, and installing
the Faodel software. On a system where all compilers and TPLs have already been
setup (i.e., CC, BOOST_ROOT, and GTEST_ROOT environment variables are correct),
you can simply change into a build directory, run cmake to configure the build,
and then run make install to build/install the software. For example:

    cd faodel
    mkdir build
    cd build
    cmake                                         \
       -DCMAKE_INSTALL_PREFIX=$(pwd)/../install   \
       ..
    make -j 4 doc install

Often TPLs are not installed where they should be and it is necessary to specify
where things are located during configuration. You can also specify a number of
Faodel-specific options to CMake during configuration. A complete list of CMake
options is provided in **Appendix A** of this Install guide. The following
provides a more thorough configure/build/install example:

    cd faodel
    mkdir build
    cd build
    cmake                                           \
       -DCMAKE_INSTALL_PREFIX=$(pwd)/../install     \
       -DCMAKE_BUILD_TYPE=Release                   \
       \
       -DGTEST_ROOT=${GTEST_ROOT}                   \
       -DBOOST_ROOT=${BOOST_ROOT}                   \
       \
       -DFaodel_NETWORK_LIBRARY=nnti                \
       -DFaodel_ENABLE_MPI_SUPPORT=ON               \
       \
       -DBUILD_TESTS=ON                             \
       -DBUILD_EXAMPLES=ON                          \
       -DBUILD_DOCS=ON                              \
       ..
    make -j 4 doc install

Doing a plain 'make' will build the libraries (and tests) in the build
directory. The 'make install' operation will install the libraries and headers
into the CMAKE_INSTALL_PREFIX location specified during configuration. The
install step also generates an <install-prefix/lib/cmake/faodel directory that
contains cmake modules that you'll need to build your own applications.

tip: CMake's `ccmake` tool provides a visual way to look at and change the
configuration options that are set in a build directory. After
running `ccmake .` in the build directory, press 't' to toggle advanced options.

tip: You can speed up your build times by telling CMake to use ninja instead of
make (if available on your platform). The configure and build steps are then:

    cmake -GNinja -DCMAKE_INSTALL_PREFIX=$(pwd)/../install ..
    ninja

If Builds Fail
--------------
There are many ways builds go wrong. Here are a few problems and their fixes:

- Could NOT find ZLIB
    - Some systems have a zlib library installed, but not its headers
    - This does not affect libraries/examples, but may skip some tests
    - You can fix by setting the environment variable ZLIB_ROOT
- Many gtest warnings:
    - Newer versions of GTest have deprecated some APIs we use
    - Ignore warnings or build with an older version of gtest

Running Tests
-------------
Each subcomponent of Faodel comes with its own set of tests. Some of the simpler
tests (eg usually unit tests) are designed to run in a standalone mode and do
not need MPI to run. These tests begin with the prefix "tb_", where tb stands
for testbench. Other tests that need to be launched with mpi use a prefix of "
mpi_". Running the entire suite of tests for a tool can be done via:

     cd build/testX
     make test

On some platforms you may need to pass in additional information about the
hardware. Runtime information can be provided through the definition of an
environment variable that points to a configuration file. See 'Passing in
Configuration Information' below for more info.

Several of the tests use mpi to launch multiple programs that then communicate
with each other via Faodel's primitives. On platforms that use Slurm, the build
system will launch tests in a parallel form if you allocated nodes properly and
use an mpiexec that inherits Slurm job settings correctly:

     cd build salloc -N 16 --ntasks-per-node=1 make test exit

tip: ntasks-per-node isn't necessary, but it forces the tasks to run on
different nodes.

If Tests Fail
-------------
If tests fail, you should first look in the build/Testing/Temporary directory
and look at the LastTest.log file for information about what happened in the
test. Common problems include:

- **Not Enough MPI Slots:** Often tests fail because mpi doesn't think there is
  enough room in an allocation to run all tests. For Slurm, increase the job
  allocation size or remove ntasks-per-node. If you're running on a laptop with
  openmpi, pass "--oversubscribe" into mpirun, or use the following environment
  variable before running tests:

      export OMPI_MCA_rmaps_base_oversubscribe=1

- **Check your whookie.interfaces list:** Compute nodes often have several 
  network interfaces and whookie often guesses wrong. Log into a compute node,
  do `ifconfig` or `ip addr` to find a live network interface, and set it in
  your config.

- **Timeouts:** Each test is configured to abort if it runs longer than 
  30 seconds. While a timeout may be a sign of an overloaded node, it's more
  likely that something has gone wrong (look in the logs for info).

- **Atomics:** Mellanox installations sometimes have issues that cause them to
  implement atomics properly. Faodel often detects problems at build time, but
  the atomics tests are the true test of whether things work properly. If they
  fail, its likely your Mellanox/OFED install is broken. Given that Atomics are
  not currently used in Faodel's current libs, it may not be essential for these
  tests to work in order to use Faodel.

- **InfiniBand "Cannot Create CQ":** If you're seeing InfiniBand errors about
  not being able to create a CQ due to memory issues, you may need to talk to
  your system admin and have the ulimits adjusted. Faodel (and HPC 
  applications) need to allocate large chunks of memory the NIC can access. By
  default, systems only allow users to allocate a small amount of locked 
  memory (eg, `ulimit -l` might show 64KB). An admin can fix this by editing
  the `/etc/security/limits.conf` file and setting a high value for 
  particular users. eg,
  

     @ib_users        soft    memlock        1074741824
     @ib_users        hard    memlock        1610612736

Faodel provides the faodel tool as a sanity check for your build. This tools
prints out build information and performs basic checks to determine if the
libraries will work. You should run this test on your platform's login node, as
well as a compute node (some platforms have different hardware).

     build/tools/faodel-cli/faodel build-info
     salloc -N 1
     srun build/tools/faodel-cli/faodel build-info
     exit 

Optimizing Faodel Builds
------------------------
Faodel has a significant amount of debugging infrastructure built into it to 
help users troubleshoot services. These debug options can cause significant 
slow downs. When building for performance, we recommend setting the following
cmake options:

- CMAKE_BUILD_TYPE Release: This enables compiler optimizations
- Faodel_LOGGING_METHOD disabled: This macros-out most dbg/warn message
- Faodel_ASSERT_METHOD none: This removes a lot of safety checks
- Faodel_ENABLE_DEBUG_TIMERS OFF: This is usually off, but removes debug timing

Building the Examples
=====================
Most of the subprojects in Faodel have their own set of examples that users can
build on their own. While you can enable the building of these examples by
turning on the BUILD_EXAMPLES option when building Faodel, the expectation is
that users will build/install Faodel, and then build the examples directory as
its own project. The motivation here is to provide an example CMake project
users can use as a skeleton for their own work.

Note: The examples don't do anything exciting themselves. The in-line comments
in the example source code explain what is happening.

Building All the Subcomponent Examples
--------------------------------------
Once Faodel is built and installed, a user can set the `Faodel_DIR`
environment variable to the installation and then build the examples. For
example:

    cd faodel
    mkdir build_examples
    cd build_examples
    export Faodel_DIR=/path/to/faodel/install
    cmake \
          ../examples
    make -j 4
    ./kelpie/nonet/local-pool-basics/local-pool-basics

The examples can be run directly out of the build directory.


Passing in Configuration Information
====================================
Faodel uses a Configuration class to control how various aspects of the system
are configured at runtime. The Faodel examples and tests have all been
configured to examine the "Faodel_CONFIG" environment variable to locate the
name of a configuration file that should be loaded during initialization.

A complete list of configuration parameters can be found by using the
`faodel config-options` tool. Many of these options are for turning on
debug/info messages for different components in the stack. The main ones
you might need to adjust are (in order of importance):

```
whookie.interfaces  ib0,eth0    # Change the nic used for whookie
net.transport.name  ibverbs     # Select net driver when using nnti or libfabric
whookie.log.info true           # Print whookie url at boot
bootstrap.debug true            # Turn on debug message for a component
bootstrap.show_config true      # Show final config settings used at boot
bootstrap.halt_on_shutdown true # Hang on exit so you can query stats in whookie
dirman.write_root filename      # Write dirman info to a file for others
```

When you run an application, you can either set FAODEL_CONFIG in you shell, or
overwrite it on the command line:

```
# Use a default config
export FAODEL_CONFIG=~/cofigs/my_generic_config.conf
srun myapp

# Use a specific config on a single run
FAODEL_CONFIG=custom.conf srun myapp
```

OpBox Cores
-----------
OpBox provides two Cores for driving Op state machines. The Standard Core is a
single threaded synchronous implementation. The Standard Core executes all Op
state machine updates after launch. Launch is executed in the application's
calling thread and all other updates are executed in the network module's
callback thread.

The Threaded Core is a multi-threaded asynchronous implementation. The Threaded
Core uses a threadpool to isolate itself from the network module. When an Op is
launched, it is assigned to a thread in the pool and all state machine updates (
including launch) are executed by that thread.

To select a Core and the size of the threadpool add the following to your
Configuration:

```
opbox.type         threaded   # select either "standard" or "threaded"
opbox.core.threads 4          # select the size of the threadpool
``` 

Network Transports
==================
Faodel uses RDMA communication to perform all of its network interactions.
Faodel includes the NNTI library, which is a low-level communication library
that Sandia has developed and used in different applications. NNTI provides
transports for InfiniBand, Cray, and MPI and has been updated to support
different Faodel needs. Recent versions of Faodel also include experimental
support for the Open Fabrics library (libfabric). Libfabric provides additional
network transports (eg sockets) that may be useful to developers.

During configuration, NNTI determines the network interconnects available on the
target machine. This is done by probing for headers and libraries particular to
each interconnect. There may be cases when NNTI finds an interconnect that is
not actually functional. To disable a specific transport, add one of the
following to your CMake command-line.

```
-DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE
-DNNTI_DISABLE_MPI_TRANSPORT:BOOL=TRUE
-DNNTI_DISABLE_UGNI_TRANSPORT:BOOL=TRUE
``` 

Mellanox Network Devices
------------------------
There are two common network drivers/devices for Mellanox NICs -- mlx4 and mlx5.
There is an atomics bug in the Mellanox verbs library that breaks atomics on
mlx5 when using the standard verbs API. In order for RDMA library developers to
do atomics on mlx5, they must use the expanded verbs API and then byte-swap the
result to little endian (if needed). The NNTI library in Faodel performs this
operation for users automatically. However, it is imporant that users understand
what is happening in case their build environment is not correct and problems
arise.

Faodel uses a combination of configuration and runtime detection to determine if
the expanded verbs API must be used. At configuration, Faodel looks for the
expanded verbs API. If found, a "universal"
library is built that can run on either mlx4 or mlx5. If the expanded verbs API
is not found, an mlx4-only library is built. At startup, the NIC is queried to
determine its atomics capabilities and requirements. The
"universal" library can then decided between standard and expanded verbs API at
runtime.

Caveat: In order for this to work, Faodel must be configured and built with a
libverbs that has the expanded API. Otherwise, you get only the standard API.
It does not matter which NIC is in the machine where you configure and build.
It only matters which library is installed. To determine if a libverbs 
installation has the expanded API, look for a file 
named `/usr/include/infiniband/verbs_exp.h`.

kahuna has the expanded API on all login and compute nodes, so you can
configure and build on any node and run on any node.



Selecting An Infiniband Network Device
--------------------------------------
When Faodel bootstraps the network, it searches for an Infiniband device with an
active port. By default, Faodel queries the verbs library for a list of devices
and chooses the first one with an active port. If there are multiple devices or
multiple active ports, Faodel may not choose the correct device.

In this case, add the following to the configuration file:

    net.transport.interfaces ib1,ib0  # Prefer ib1 over ib0

When `net.transport.interfaces` is defined, Faodel will search for these
devices (and only these devices) in the order given.


Building Third-Party Libraries (TPLs)
=====================================
This section provides information about building TPLs needed by Faodel.

Installing Boost
----------------
Faodel requires Boost version 1.60 or higher due to bugs in previous versions.
There is a version dependency between Boost and CMake in that newer versions of
Boost require newer versions of CMake. Begin by downloading and extracting a
supported version of Boost from http://www.boost.org/users/download/

On most machines the build is straight forward:

```
wget "https://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download" -O boost.tar.bz2
tar xf boost.tar.bz2
cd boost_1_60_0
./bootstrap.sh --prefix=$TPL_INSTALL_DIR
./b2 -a install
```

The Cray XC40 family of machines has some caveats. See the platform notes at the
end of this document for more information.


Installing GoogleTest
---------------------
Faodel uses GoogleTest for tests. Begin by downloading and extracting the
GoogleTest source from https://github.com/google/googletest/releases/

Googletest is a straightforward build.

```
cd faodel/tpl
git clone https://github.com/google/googletest.git -b release-1.10.0
mkdir googletest/build
cd googletest/build
cmake -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DBUILD_GMOCK=OFF ..
make install
cd ../../..
export GTEST_ROOT=$(pwd)/tpl/googletest/install
```

Note: A few of the kelpie tests use features in googletest that are now
deprecated. We recommend either using a prior version (eg above) or avoiding
the tests.

Installing libfabric (optional)
-------------------------------
Faodel has **experimental** support for using libfabric as it's communication
library. While some Faodel functions work correctly with the 1.4.2 release of
libfabric, there are known problems with atomics and long sends. To build
libfabric from source:

     git clone https://github.com/ofiwg/libfabric
     cd libfabric
     git checkout tags/v1.4.2
     ./autogen.sh
     ./configure --prefix=$TPL_INSTALL_DIR (extra options)
     make install

You may need to pass in extra options (eg --disable-verbs --disable-psm) to
control which network transports get built.

In order to build Faodel with libfabric, you need to (1) set the
`Faodel_NETWORK_LIBRARY` to `libfabric` when configuring Faodel and (2) add
libfabric's package configuration info to `PKG_CONFIG_PATH`. eg,

    export PKG_CONFIG_PATH=${LIBFABRIC_ROOT}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    cmake \
       -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR        \
       -DFaodel_NETWORK_LIBRARY=libfabric                \
       ..

You will also need to set your `LD_LIBRARY_PATH` to include a path to the
libfabric library in order to run applications.




Data Structure Serialization
============================

NNTI data structures can be sent to peers both implicitly (command messages)
and explicitly (buffer references). To support heterogeneous platforms, these
data structures are serialized to a portable format, sent to the peer and
deserialized at the recipient.

NNTI has historically used XDR for serialization because it is fast, tight and
ubiquitous. In recent releases (eg. Mojave) of MacOS, XDR is not fully
implemented. NNTI detects this condition during configuration and uses the
bundled Cereal library as an alternative.

If you prefer Cereal over XDR, you can force the use of Cereal using
the `Faodel_NNTI_SERIALIZATION_METHOD` option. The possible values are XDR and
CEREAL.

In recent releases of Faodel, Cereal is used to serialize data structures beyond
NNTI. To support these features the bundled Cereal library gets installed as
tpl-cereal along with the other Faodel headers. External projects can add these
headers as a dependency using the `Faodel::tpl-cereal` CMake target that gets
created when `find_package(Faodel)` runs.

Note: There is no way to reference an external installation of Cereal.


Platform-Specific Notes: Installing on Mutrino (Cray XC40)
==========================================================

Installing Faodel on a Cray XC40 is more complicated than other platforms due to
the fact that there are two processor architectures (HSW and KNL) and Cray has
its own set of modules for tools. Before building Faodel, you'll need to setup
the build environment for the CPU architecture you're targeting and build any
missing TPLs for that architecture.


Loading Modules for GNU/Intel Compilers on HSW/KNL
--------------------------------------------------
Mutrino loads a default set of modules that may not be compatible with Faodel
when building with the GNU compilers. To reset your environment to a known-good
state, unload the PrgEnv modules and then load in the ones you need.

Note: don't do a `module purge` here, as you'll lose other important variables.

```
# Unload any existing cray programming environment modules
module unload PrgEnv-pgi
module unload PrgEnv-gnu
module unload PrgEnv-cray
module unload PrgEnv-intel

# Load in a GNU or Intel compiler environment
module load PrgEnv-gnu             # ..or for Intel: module load PrgEnv-intel
module load craype-hugepages2M
module load cmake/3.17.2

# Set the build environment for haswell
module load craype-haswell

# ..or if you're targeting the knl processors do this instead:
# module unload craype-haswell
# module load craype-mic-knl

# Mutrino only has one interconnect, but multiple modules. Make sure that 
# the Aries module is loaded.
module load craype-network-aries
```

Setting the environment
-----------------------

On CLE6, the PrgEnv modules are missing the environment variable that identifies
the CLE version number. This confuses CMake. Setting the CRAYOS_VERSION
environment variable helps CMake complete the configuration. This is only
necessary on CLE6.

```
export CRAYOS_VERSION=6
```

Some versions of pthread can't be linked statically. Set an environment variable
that defaults the link type to dynamic.

```
export CRAYPE_LINK_TYPE=dynamic
```

Building Boost
--------------
Some installations provide pre-built versions of CMake, Boost, and GTest. We
recommend using these installations if they meet Faodel's requirements. On
Mutrino, currently use:

```
module load friendly-testing
```

Building Boost by hand on the Cray XC40 currently requires some extra
configuration. After running Boost's `bootstrap`
command, create a file named `user-config.jam` that describes the compiler in
use, and then tell Boost's `b2` to use it during the build:

```
./bootstrap.sh --prefix=$TPL_INSTALL_DIR
echo 'using gcc : 2.5.6craycompute : /opt/cray/pe/craype/2.5.6/bin/CC : ;' > user-config.jam 
./b2 -a toolset=gcc-2.5.6craycompute install
```

Building Faodel
---------------

After setting up the environment, the rest of the configure and build is the
same as any other platform with two exceptions:

1. Use the Cray Linux Environment (CLE): You need to tell CMake that we are
   building for the CLE using the `CMAKE_SYSTEM_NAME` variable.
2. Disable IBVerbs Transport: Mutrino has IBVerbs header files installed that
   will cause NNTI to mistakenly think IB nics are available on the system. You
   need to set the `NNTI_DISABLE_IBVERBS_TRANSPORT` variable option.

A generic Faodel build on Mutrino starts with the following:

```
cd faodel
export Faodel_DIR=$(pwd)/install
mkdir build
cd build
cmake \
    -DCMAKE_SYSTEM_NAME=CrayLinuxEnvironment   \
    -DCMAKE_INSTALL_PREFIX=${Faodel_DIR}       \
    -DFaodel_NETWORK_LIBRARY={libfabric|nnti}  \
    -DREQUIRE_STATIC_LINKING:BOOL=TRUE         \
    -DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE \
    -DCMAKE_C_COMPILER=cc                      \
    -DCMAKE_CXX_COMPILER=CC                    \
    -DGTEST_ROOT=${GTEST_ROOT}                 \
    -DBOOST_ROOT=${BOOST_ROOT}                 \
    ..
make -j 20 install
```

Faodel Configuration Settings for Mutrino
-----------------------------------------
There are three Cray-specific system settings that you'll want to add to
your `FAODEL_CONFIG` file when running on mutrino: (1) the name of the
IP-over-Gemini device that whookie can use to setup connections
(eg `ipogif0`), (2) the network transport that should be used (eg, `ugni`),
and (3) the DRC credential id you've been assigned by a system admin if you plan
on doing job-to-job communication.

```
whookie.interfaces  ipogif0
net.transport.name  ugni
nnti.transport.credential_id 42
```

Inter-job Communication on Mutrino
----------------------------------
Mutrino has an Aries/Gemini interconnect that is programmed using the UGNI
library. The UGNI library protects applications from outside data corruption (
benign or malicious) by preventing RDMA operations between applications. In
order for two processes to communicate, they must have the same credentials. At
launch, the typical application is assigned a system-wide unique credential that
allows communication between processes making up the application and prevents
communication with other processes outside the application even if they are
launched by the same user within the same allocation.

In order to communicate between applications, an administrator must create a
Dynamic RDMA Credential (DRC) and assign it to a user. Then a group of
applications using the same credential can transfer data to/from each other.


Mutrino Caveats
---------------
Building and running has a few caveats:

1. Missing mpiexec breaks tests: CMake's mpi testing expects to launch with
   mpiexec, while mutrino expects users to use srun. If you look at the CTest
   file to get the path of mpiexec, you can use the following sed script to
   replace it:

```
cd faodel/build
find . -name CTestTestfile.cmake | xargs sed -i 's@/opt/cray/elogin/eproxy/2.0.22-6.0.5.0_2.1__g1ebe45c.ari/bin/mpiexec@srun@' 
```

2. Missed CC's: CMake sometimes has trouble figuring out the MPI configuration
   with the Intel compiler targeting KNL. You may need to add the following to
   your cmake configure of Faodel:

``` 
-DCMAKE_CXX_COMPILER=CC \
-DCMAKE_C_COMPILER=cc
```

3. Gethost Warnings: The build will give many warning messages about
   gethostbyname in a statically linked application requiring a runtime with the
   shared libraries from glibc. This is normal and does not affect Faodel.

Installing on Astra (ARM-based Mellanox InfiniBand Cluster)
===========================================================

Astra is an ARM-based cluster with a Mellanox InfiniBand interconnect. In
general, Faodel operates the same on Astra as on any other InfiniBand platform.
One extra feature of Astra is that it has full support for Mellanox's On-Demand
Paging feature that allows a process' entire virtual address space to be
registered without pinning (locking) pages in memory. The availability of ODP is
detected during configuration, but it is disable by default because it is still
experimental. To enable it, add the following to your configuration file.

```
net.transport.use_odp true
```

This feature is still experimental with no guarantees of performance or
correctness.


Installing on Kahuna (Generic InfiniBand Cluster)
=================================================

Kahuna is a generic InfiniBand cluster that
uses [Spack](https://spack.readthedocs.io) to generate its software modules. The
following instrutions provide an example of how to build the environment.

Loading Modules
---------------

Kahuna has all the required TPLs installed in its modules. The current
recommended environment setting is as follows:

```
module load doxygen
module load ninja
module load cmake/3.15.4
module load gcc9-support
module load gcc/9.2.0
module load boost/1.70.0
module load googletest
module load openmpi/3.1.3

export GTEST_ROOT=${GOOGLETEST_ROOT}
```

Faodel Configuration Settings for Kahuna
----------------------------------------
When Faodel starts, its default behavior is to read a configuration file
specified by the environment variable `FAODEL_CONFIG`. Kahuna is a generic
InfiniBand Cluster with both 10GigE (on eth0) and IPoIB (on ib0) network ports
for sockets. Thus, you should add the following info to the configuration file
specified by `FAODEL_CONFIG`:

```
whookie.interfaces  eth0,ib0
net.transport.name  ibverbs
```

Appendix A: Faodel CMake Options
================================
The following CMake options can be toggled when building Faodel:

Basic Options
-------------

| CMake Variable                   | Type                  | Purpose                                                                                           |
| -------------------------------- | --------------------- | ------------------------------------------------------------------------------------------------- |
| BUILD_DOCS                       | Boolean               | Use Doxygen to build and install docs in Faodel/docs                                              |
| BUILD_SHARED_LIBS                | Boolean               | Build Faodel as shared libraries (instead of static)                                              |
| BUILD_TESTS                      | Boolean               | Build Faodel unit and component tests                                                             |
| CMAKE_INSTALL_PREFIX             | Path                  | Standard CMake variable for where to install Faodel. Will create include,lib,bin subdirectories   |
| Faodel_ENABLE_IOM_HDF5           | Boolean               | Build a Kelpie IO module for writing objects to an HDF5 file. Requires HDF5_DIR                   |
| Faodel_ENABLE_IOM_LEVELDB        | Boolean               | Build a Kelpie IO module for writing objects to a LevelDB store. Required leveldb_DIR             |
| Faodel_ENABLE_MPI_SUPPORT        | Boolean               | Include MPI capabilities (MPISyncStart service, mpi-based testing, and NNTI MPI Transport)        |
| Faodel_ENABLE_TCMALLOC           | Boolean               | Compile the included tpl/gperftools tcmalloc and use in Lunasa memory allocator                   |
| Faodel_LOGGING_METHOD            | stdout, sbl, disabled | Determines where the logging interface writes. Select disabled to optimize away                   |
| Faodel_NETWORK_LIBRARY           | nnti,libfabric        | Select which RDMA network library to use                                                          |
| Faodel_NNTI_SERIALIZATION_METHOD | CEREAL, XDR           | Controls whether NNTI serializes data with XDR or Cereal                                          |

Advanced Options
----------------

| CMake Variable             | Type                | Purpose                                                                                                                   |
| -------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| HDF5_DIR                   | Path                | Location of HDF5 libs. Only used when Faodel_ENABLE_IOM_HDF5 used                                                         |
| leveldb_DIR                | Path                | Location of leveldb libs. Only used when Faodel_ENABLE_IOM_LEVELDB used                                                   |
| Faodel_ASSERT_METHOD       | cassert,debug*,none | Select assert behavior. Cassert is std c. debug(Exit/Halt/Warn) prints more info. none does no assertion checks           |
| Faodel_ENABLE_DEBUG_TIMERS | Boolean             | When enabled, some components such as opbox can capture timing traces that are dumped on exit. use opbox.enable_timers    |
| Faodel_NO_ISYSTEM_FLAG     | Boolean             | Some compilers use "-isystem" to identify system libs instead of "-I". CMake should autodetect, but this can override     |
| Faodel_OPBOX_NET_NNTI      | Boolean             | Set to true if Faodel_NETWORK_LIBRARY is nnti                                                                             |
| Faodel_PERFTOOLS_*         | -                   | These variables are used by the tpl/gperftools library. See their documentation for more info                             |
| Faodel_THREADING_LIBRARY   | pthreads, openmp    | Select fundamental threading lib we're using. Almost always is pthreads. This may be modified in the future (eg QTHREADS) |

Environment Variables
---------------------
The following environment variables are used during configuration

- `BOOST_ROOT`: Path to the boost installation (inside should have an include and
  lib directory for Boost). On some OS's where boost is a system package,
  you can use `/usr/lib`.
- `GTEST_ROOT`: Path to the googletest installation is BUILD_TESTS is enabled
