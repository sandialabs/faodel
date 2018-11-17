FAODEL Installation
===================
The FAODEL software is written in C++11 and use CMake for
building. Users must install a small number of third-party libraries
(TPLs) in order to build FAODEL.

Prerequisites
-------------
FAODEL has a small number of prerequisites for building. A target platform
should have:

- C++11 Compiler: The current implementation is typically built with
  GCC and requires version 4.8.5 or higher in order to use C++11
  features. 
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

Configuring and Building FAODEL
-------------------------------
FAODEL uses a standard CMake workflow for configuring, building, and installing
the FAODEL software. On a system where all compilers and TPLs have already
been setup (i.e., CC, BOOST_ROOT, and GTEST_ROOT environment variables are
correct), you can simply change into a build directory, run cmake to configure
the build, and then run make install to build/install the software. 
For example:

    cd faodel
    ROOT_DIR=$(pwd)
    INSTALL_DIR=${ROOT_DIR}/install
    mkdir build
    cd build
    cmake                                         \
       -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR        \
       ..
    make -j 4 doc install

Often TPLs are not installed where they should be and it is necessary
to specify where things are located during configuration. You can also
specify a number of FAODEL-specific options to CMake during configuration.
A complete list of CMake options is provided in **Appendix A** of this
Install guide. The following provides a more thorough
configure/build/install example:


    cd faodel
    ROOT_DIR=$(pwd)
    INSTALL_DIR=${ROOT_DIR}/install
    mkdir build
    cd build
    cmake                                           \
       -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}        \
       -DCMAKE_BUILD_TYPE=Release                   \
       \
       -DGTEST_ROOT=${GTEST_ROOT}                   \
       -DBOOST_ROOT=${BOOST_ROOT}                   \
       \
       -DFaodel_NETWORK_LIBRARY=nnti                \
       -DFaodel_ENABLE_MPI_SUPPORT=ON               \
       \
       -DBUILD_TESTS=ON                             \
       -DBUILD_DOCS=ON                              \
       ..
    make -j 4 doc install

Doing a plain 'make' will build the libraries (and tests) in the 
build directory. The 'make install' operation will install the 
libraries and headers into the CMAKE_INSTALL_PREFIX location
specified during configuration. The install step also generates
an <install-prefix/lib/cmake/faodel directory that contains cmake
modules that you'll need to build your own applications.

tip: CMake's `ccmake` tool provides a visual way to look at and
     change the configuration options that are set in a build
     directory. After running `ccmake .` in the build directory,
     press 't' to toggle advanced options.

tip: You can speed up your build times by telling CMake to use ninja
     instead of make (if available on your platform). The configure
     and build steps are then:


    cmake -GNinja -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ..
    ninja


Running Tests
-------------
Each subcomponent of FAODEL comes with its own set of tests. Some of
the simpler tests (eg usually unit tests) are designed to run in a 
standalone mode and do not need MPI to run. These tests begin with the
prefix "tb_", where tb stands for testbench. Other tests that need
to be launched with mpi use a prefix of "mpi_". Running the entire
suite of tests for a tool can be done via:

     cd build/testX
     make test

On some platforms you may need to pass in additional information
about the hardware. Runtime information can be provided through the
definition of an environment variable that points to a configuration
file. See 'Passing in Configuration Information' below for more info.

Several of the tests use mpi to launch multiple programs that then
communicate with each other via FAODEL's primitives. On platforms that
use Slurm, the build system will launch tests in a parallel form if
you allocated nodes properly and use an mpiexec that inherits Slurm
job settings correctly:

   cd build
   salloc -N 16 --ntasks-per-node=1 
   make test
   exit

tip: ntasks-per-node isn't necessary, but it forces the tasks to run
     on different nodes.

If Tests Fail
-------------
If tests fail, you should first look in the build/Testing/Temporary
directory and look at the LastTest.log file for information about
what happened in the test. Common problems include:

- Not Enough MPI Slots: Often tests fail because mpi doesn't think
  there is enough room in an allocation to run all tests. For Slurm,
  increase the job allocation size or remove ntasks-per-node. If you're
  running on a laptop with openmpi, pass "--oversubscribe" into 
  mpirun, or use the following environment variable before running tests:

     export OMPI_MCA_rmaps_base_oversubscribe=1

- Check your webhook.interfaces list: Compute nodes often have several
  network interfaces and webhook often guesses wrong. Log into a compute
  node, do "ifconfig" or "ip addr" to find a live network interface,
  and set it in your config.

- Timeouts: Each test is configured to abort if it runs longer than 30
  seconds. While a timeout may be a sign of an overloaded node, it's 
  more likely that something has gone wrong (look in the logs for info).

- Atomics: Mellanox installations sometimes have issues that cause them
  to implement atomics properly. FAODEL often detects problems at build
  time, but the atomics tests are the true test of whether things work
  properly. If they fail, its likely your Mellanox/OFED install is broken.
  Given that Atomics are not currently used in FAODEL's current libs, it
  may not be essential for these tests to work in order to use FAODEL.

FAODEL provides the faodel_info tool as a sanity check for your build. This
tools prints out build information and performs basic checks to determine
if the libraries will work. You should run this test on your platform's
login node, as well as a compute node (some platforms have different
hardware).

     build/tools/faodel-info/faodel_info
     salloc -N 1
     srun build/tools/faodel-info/faodel_info
     exit 


Building the Examples
=====================
Most of the subprojects in FAODEL have their own set of examples that
users can build on their own. These examples assume the user 
**has already built and installed FAODEL**. The motivation for separating
the examples from the FAODEL build is to give a minimal, self-contained
CMake project that users can pick up and modify for their own projects.

Note: The examples don't do anything exciting themselves. The in-line
comments in the example source code explains what is happening.

Building All the Subcomponent Examples
--------------------------------------
Once FAODEL is built and installed, a user can set the `Faodel_DIR`
environment variable to the installation and then build the examples.
For example:

    cd faodel
    mkdir build_examples
    cd build_examples
    export Faodel_DIR=${INSTALL_DIR}
    cmake \
          ../examples
    make -j 4
    ./kelpie/nonet/local-pool-basics/local-pool-basics


The examples can be run directly out of the build directory.


Passing in Configuration Information
====================================
FAODEL uses a Configuration class to control how various aspects of the
system are configured at runtime. The FAODEL examples and tests have all
been configured to examine the "FAODEL_CONFIG" environment variable to
locate the name of a configuration file that should be loaded during
initialization.

A complete list of 
[project configuration parameters](https://gitlab.sandia.gov/nessie-dev/faodel/wikis/project-configuration-tags)
can be found on the FAODEL wiki. Common settings that a
user may wish to change include:

```
webhook.interfaces  ib0,eth0  # Change the nic used for webhook

net.transport.name  ibverbs   # Select net driver when using nnti or libfabric

bootstrap.debug     true      # Turn on debug message for a component
```

When you run an application, you can either set FAODEL_CONFIG in you shell,
or overwrite it on the command line:

```
# Use a default config
export FAODEL_CONFIG=~/cofigs/my_generic_config.conf
srun myapp

# Use a specific config on a single run
FAODEL_CONFIG=custom.conf srun myapp
```



Opbox Cores
-----------
Opbox provides two Cores for driving Op state machines.  The Standard Core 
is a single threaded synchronous implementation.  The Standard Core executes 
all Op state machine updates after launch.  Launch is executed in the 
application's calling thread and all other updates are executed in the 
network module's callback thread.  

The Threaded Core is a multi-threaded asynchronous implementation.  The 
Threaded Core uses a threadpool to isolate itself from the network module.  
When an Op is launched, it is assigned to a thread in the pool and all state 
machine updates (including launch) are executed by that thread.

To select a Core and the size of the threadpool add the following to your 
Configuration:

```
opbox.type         threaded   # select either "standard" or "threaded"
opbox.core.threads 4          # select the size of the threadpool
``` 



Network Transports
==================
FAODEL uses RDMA communication to perform all of its network interactions. 
FAODEL includes the NNTI library, which is a low-level communication library
that Sandia has developed and used in different applications. NNTI provides
transports for InfiniBand, Cray, and MPI and has been updated to support
different FAODEL needs. Recent versions of FAODEL also include 
experimental support for the Open Fabrics library (libfabric).  Libfabric
provides additional network transports (eg sockets) that may be useful
to developers.

During configuration, NNTI determines the network interconnects available 
on the target machine.  This is done by probing for headers and 
libraries particular to each interconnect.  There may be cases when NNTI 
finds an interconnect that is not actually functional.  To disable a specific 
transport, add one of the following to your CMake command-line. 

```
-DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE
-DNNTI_DISABLE_MPI_TRANSPORT:BOOL=TRUE
-DNNTI_DISABLE_UGNI_TRANSPORT:BOOL=TRUE
``` 


Mellanox Network Devices
------------------------
There are two common network drivers/devices for Mellanox NICs -- mlx4 
and mlx5.  There is an atomics bug in the Mellanox verbs library that 
breaks atomics on mlx5 when using the standard verbs API.  In order for
RDMA library developers to do atomics on mlx5, they must use the expanded
verbs API and then byte-swap the result to little endian (if needed).
The NNTI library in FAODEL performs this operation for users automatically.
However, it is imporant that users understand what is happening in case
their build environment is not correct and problems arise.

FAODEL uses a combination of configuration and runtime detection to 
determine if the expanded verbs API must be used.  At configuration, 
FAODEL looks for the expanded verbs API.  If found, a "universal" 
library is built that can run on either mlx4 or mlx5.  If the expanded
verbs API is not found, an mlx4-only library is built.  At startup, the
NIC is queried to determine its atomics capabilities and requirements.  The
"universal" library can then decided between standard and expanded verbs 
API at runtime.

Caveat:  In order for this to work, FAODEL must be configured and built 
with a libverbs that has the expanded API.  Otherwise, you get only the 
standard API.  It does not matter which NIC is in the machine where you 
configure and build.  It only matters which library is installed.  To 
determine if a libverbs installation has the expanded API, look for a 
file named /usr/include/infiniband/verbs_exp.h.

kahuna has the expanded API on all login and compute nodes, so you can 
configure and build on any node and run on any node.






Building Third-Party Libraries (TPLs)
=====================================
This section provides information about building TPLs needed by FAODEL.

Installing Boost
----------------
FAODEL requires Boost version 1.60 or higher due to bugs 
in previous versions.  There is a version dependency between Boost 
and CMake in that newer versions of Boost require newer versions 
of CMake. Begin by downloading and extracting a supported version 
of Boost from http://www.boost.org/users/download/

On most machines the build is straight forward:
```
wget "https://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download" -O boost.tar.bz2
tar xf boost.tar.bz2
cd boost_1_60_0
./bootstrap.sh --prefix=$TPL_INSTALL_DIR
./b2 -a install
```
The Cray XC40 family of machines has some caveats. See the platform
notes at the end of this document for more information.


Installing GoogleTest
---------------------
FAODEL uses GoogleTest for tests. Begin by downloading and extracting
the GoogleTest source from https://github.com/google/googletest/releases/

Googletest is a straight forward build.

```
cmake -DCMAKE_INSTALL_PREFIX=$TPL_INSTALL_DIR .
make install
```

Installing libfabric (optional)
-------------------------------
FAODEL has **experimental** support for using libfabric as it's communication
library. While some FAODEL functions work correctly with the 1.4.2 release
of libfabric, there are known problems with atomics and long sends. To build
libfabric from source:

     git clone https://github.com/ofiwg/libfabric
     cd libfabric
     git checkout tags/v1.4.2
     ./autogen.sh
     ./configure --prefix=$TPL_INSTALL_DIR (extra options)
     make install

You may need to pass in extra options (eg --disable-verbs --disable-psm) to 
control which network transports get built.

In order to build FAODEL with libfabric, you need to (1) set the 
`Faodel_NETWORK_LIBRARY` to `libfabric` when configuring FAODEL and (2) add 
libfabric's package configuration info to `PKG_CONFIG_PATH`. eg,

    export PKG_CONFIG_PATH=${LIBFABRIC_ROOT}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    cmake \
       -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR        \
       -DFaodel_NETWORK_LIBRARY=libfabric                \
       ..

You will also need to set your `LD_LIBRARY_PATH` to include a path to
the libfabric library in order to run applications.




Platform-Specific Notes: Installing on Mutrino (Cray XC40)
==========================================================

Installing Faodel on a Cray XC40 is more complicated than other platforms due
to the fact that there are two processor architectures (HSW and KNL) and 
Cray has its own set of modules for tools.  Before building FAODEL, you'll
need to setup the build environment for the CPU architecture you're targeting
and build any missing TPLs for that architecture.

Loading Modules for Haswell (HSW)
---------------------------------

mutrino loads a default set of modules that may not be compatibilty with 
FAODEL.  Selecting the correct set of modules requires a few steps.

Begin by unloading any programming environment modules that may interfere 
with loading the GNU module that FAODEL needs.
```
module unload PrgEnv-pgi
module unload PrgEnv-gnu
module unload PrgEnv-cray
module unload PrgEnv-intel
```
Then load the modules required to build FAODEL.
```
module load PrgEnv-gnu
module swap gcc/6.1.0
module load craype-hugepages2M
module load cmake
```
CMake has a hardcoded table of Boost versions that it knows how to 
configure.  It is important that the CMake version matches the Boost 
version.  The recommended version of Boost for FAODEL is 1.60.0 
which is compatible with the default cmake/3.6.2 module on mutrino.  If you 
need a later version of Boost, you will also need a later version of CMake.  
Confirm that we have CMake 3.6.2 or later.
```
module whatis cmake
```
You can load the Cray enviroment for Haswell with the following module:
```
module load craype-haswell
```
Mutrino only has one interconnect, but multiple modules.  Make sure that 
the Aries module is loaded.
```
module load craype-network-aries
```

Loading Modules for Knights Landing (KNL)
-----------------------------------------



Setting the environment
-----------------------

Some versions of the PrgEnv modules are missing the version number which 
confuses CMake.  Set an environment variable to help CMake.
```
export CRAYOS_VERSION=6
```
Some versions of pthread can't be linked statically.  Set an environment 
variable that defaults the link type to dynamic.
```
export CRAYPE_LINK_TYPE=dynamic
```

Building Boost
--------------

Some installations provide pre-built versions of CMake, Boost, and GTest. We
recommend using these installations if they meet FAODEL's requirements. On 
Mutrino, currently use:

```
module load friendly-testing
module load cmake/3.11.4
```

Building Boost by hand on the Cray XC40 currently requires some extra 
configuration. After running Boost's `bootstrap` command, create a 
file named `user-config.jam` that describes the compiler in use, and
then tell Boost's `b2` to use it during the build:


```
./bootstrap.sh --prefix=$TPL_INSTALL_DIR
echo 'using gcc : 2.5.6craycompute : /opt/cray/pe/craype/2.5.6/bin/CC : ;' > user-config.jam 
./b2 -a toolset=gcc-2.5.6craycompute install
```

Building FAODEL
---------------

After setting up the environment, the rest of the configure and build is 
the same as any other platform with two exceptions:

1. Use the Cray Linux Environment (CLE): You need to tell CMake that we
   are building for the CLE using the `CMAKE_SYSTEM_NAME` variable.
2. Disable IBVerbs Transport: Mutrino has IBVerbs header files installed
   that will cause NNTI to mistakenly think IB nics are available on the
   system. You need to set the `NNTI_DISABLE_IBVERBS_TRANSPORT` variable
   option.

A generic FAODEL build on Mutrino starts with the following:

```
cd faodel
export Faodel_DIR=$(pwd)/install
mkdir build
cd build
cmake \
    -DCMAKE_SYSTEM_NAME=CrayLinuxEnvironment \
    -DCMAKE_INSTALL_PREFIX=${Faodel_DIR} \
    
    -DFaodel_NETWORK_LIBRARY={libfabric|nnti} \
    -DREQUIRE_STATIC_LINKING:BOOL=TRUE \
    -DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE \
    -DCMAKE_C_COMPILER=cc \
    -DCMAKE_CXX_COMPILER=CC \
    -DGTEST_ROOT=${GTEST_ROOT} \
    -DBOOST_ROOT=${BOOST_ROOT} \
    ..
make -j 20 install
```

FAODEL Configuration Settings for Mutrino
-----------------------------------------

When FAODEL starts, its default behavior is to read a configuration file
specified by the environment variable `FAODEL_CONFIG`. The Cray platforms
use an IP over Gemini Fabric nic for socket communication (usually
defined as ipogif0). Thus, you should add the following info to the
configuration file specified by `FAODEL_CONFIG`:

```
webhook.interfaces  ipogif0
net.transport.name  ugni
```


Mutrino Caveats
---------------
Building and running has a few caveats:

1. Missing mpiexec breaks tests: CMake's mpi testing expects to launch
   with mpiexec, while mutrino expects users to use srun. If you look
   at the CTest file to get the path of mpiexec, you can use the following
   sed script to replace it:

```
cd faodel/build
find . -name CTestTestfile.cmake | xargs sed -i 's@/opt/cray/elogin/eproxy/2.0.22-6.0.5.0_2.1__g1ebe45c.ari/bin/mpiexec@srun@' 
```

2. Missed CC's: CMake sometimes has trouble figuring out the MPI configuration
   with the Intel compiler targeting KNL. You may need to add the following to
   your cmake configure of FAODEL:

``` 
-DCMAKE_CXX_COMPILER=CC \
-DCMAKE_C_COMPILER=cc
```

3. Gethost Warnings: The build will give many warning messages about 
   gethostbyname in a statically linked application requiring a runtime with
   the shared libraries from glibc. This is normal and does not affect
   FAODEL.


Installing on Kahuna (Generic InfiniBand Cluster)
=================================================

Kahuna is a generic InfiniBand cluster that
uses [Spack](https://spack.readthedocs.io) to generate its software
modules. The following instrutions provide an example of how to build
the environment.

Loading Modules
---------------

Kahuna has all the required TPLs installed in its modules. The 
current recommended environment setting is as follows:

```
module load mpi-gcc6
module load cmake/3.11.0
module load pkg-config
module load gcc/6.3.0
module load boost/1.63.0
module load googletest/1.8.0
module load ninja
module use /opt/spack/c7.3/new-mpi4-gcc6.3.0/modules
module load openmpi/3.1.0

export GTEST_ROOT=${GOOGLETEST_ROOT}
```

FAODEL Configuration Settings for Kahuna
----------------------------------------
When FAODEL starts, its default behavior is to read a configuration file
specified by the environment variable `FAODEL_CONFIG`. Kahuna is a generic
InfiniBand Cluster with both 10GigE (on eth0) and IPoIB (on ib0) network
ports for sockets. Thus, you should add the following info to the
configuration file specified by `FAODEL_CONFIG`:

```
webhook.interfaces  eth0,ib0
net.transport.name  ibverbs
```



Appendix A: FAODEL CMake Options
================================
The following CMake options can be toggled when building FAODEL:

Basic Options
-------------

| CMake Variable            | Type                  | Purpose                                                                                                                   |
| ------------------------- | --------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| BUILD_DOCS                | Boolean               | Use Doxygen to build and install docs in FAODEL/docs                                                                      |
| BUILD_SHARED_LIBS         | Boolean               | Build FAODEL as shared libraries (instead of static)                                                                      |
| BUILD_TESTS               | Boolean               | Build FAODEL unit and component tests                                                                                     |
| CMAKE_INSTALL_PREFIX      | Path                  | Standard CMake variable for where to install FAODEL. Will create include,lib,bin subdirectories                           |
| Faodel_ENABLE_IOM_HDF5    | Boolean               | Build a Kelpie IO module for writing objects to an HDF5 file. Requires HDF5_DIR                                           |
| Faodel_ENABLE_IOM_LEVELDB | Boolean               | Build a Kelpie IO module for writing objects to a LevelDB store. Required leveldb_DIR                                     |
| Faodel_ENABLE_MPI_SUPPORT | Boolean               | Include MPI capabilities (MPISyncStart service, mpi-based testing, and NNTI MPI Transport)                                |
| Faodel_ENABLE_TCMALLOC    | Boolean               | Compile the included tpl/gperftools tcmalloc and use in Lunasa memory allocator                                           |
| Faodel_LOGGING_METHOD     | stdout, sbl, disabled | Determines where the logging interface writes. Select disabled to optimize away |

| Faodel_NETWORK_LIBRARY    | nnti,libfabric        | Select which RDMA network library to use                                                                                  |


Advanced Options
----------------
| CMake Variable            | Type             | Purpose                                                                                                                   |
| ------------------------- | ---------------- | ------------------------------------------------------------------------------------------------------------------------- |
| HDF5_DIR                  | Path             | Location of HDF5 libs. Only used when FAODEL_ENABLE_IOM_HDF5 used                                                         |
| leveldb_DIR               | Path             | Location of leveldb libs. Only used when FAODEL_ENABLE_IOM_LEVELDB used                                                   |
| Faodel_NO_ISYSTEM_FLAG    | Boolean          | Some compilers use "-isystem" to identify system libs instead of "-I". CMake should autodetect, but this can override     |
| Faodel_OPBOX_NET_NNTI     | Boolean          | Set to true if Faodel_NETWORK_LIBRARY is nnti                                                                             |
| Faodel_PERFTOOLS_*        | -                | These variables are used by the tpl/gperftools library. See their documentation for more info                             |
| Faodel_THREADING_LIBRARY  | pthreads, openmp | Select fundamental threading lib we're using. Almost always is pthreads. This may be modified in the future (eg QTHREADS) |

Environment Variables
---------------------
The following environment variables are used during configuration
- BOOST_ROOT: Path to the boost installation (inside should have an include and lib directory for Boost)
- GTEST_ROOT: Path to the googletest installation is BUILD_TESTS is enabled
