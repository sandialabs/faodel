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
- CMake 3.2
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

Often TPLs are not installed where they should be and it is helpful to
be more pedantic during configuration. In addition to specifying
common cmake options, you can also supply a few FAODEL-specific options
to CMake during configuration. The following provides a more thorough
configure/build/install example for the software:

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
       -DNETWORK_LIBRARY=nnti                       \
       -DUSE_MPI=ON                                 \
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
     instead of make. The configure and build steps are then:


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
          -DUSE_MPI=ON \
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


Interchangeable Implementations 
===============================
FAODEL uses the concept of Cores to provide interchangeable implementations 
of key functionality.  Cores could be developed for specific work loads, to 
exploit known algorithmic patterns or to enforce/relax semantic rules.

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
`NETWORK_LIBRARY` to `libfabric` when configuring FAODEL and (2) add 
libfabric's package configuration info to `PKG_CONFIG_PATH`. eg,

    export PKG_CONFIG_PATH=${LIBFABRIC_ROOT}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    cmake \
       -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR        \
       -DNETWORK_LIBRARY=libfabric                \
       ..

You will also need to set your `LD_LIBRARY_PATH` to include a path to
the libfabric library in order to run applications.


Platform-Specific Installation Notes
====================================
While the FAODEL software can run on a number of platforms, it
is often the case that there are differences in the way system tools
and libraries are installed that make it difficult to compile our
software. This section provides platform-specific installation nodes
relating to the systems we routinely use.

Installing on Mutrino (Cray XC40)
---------------------------------
Installing FAODEL on mutrino is not as straight forward as it is on 
other platforms.  Here we present the necessary steps.

### Loading Modules

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
Mutrino has modules for both haswell and KNL processors. FAODEL is 
currently only tested on haswell.
```
module load craype-haswell
```
Mutrino only has one interconnect, but multiple modules.  Make sure that 
the Aries module is loaded.
```
module load craype-network-aries
```

### Setting the environment

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

### Building Boost
On the Cray XC40 family of machines, Boost needs a little 
help with the compilers.  After running `bootstrap`, create a 
file named `user-config.jam` that describes the compiler in use and 
then tell `b2` to use it.

```
./bootstrap.sh --prefix=$TPL_INSTALL_DIR
echo 'using gcc : 2.5.6craycompute : /opt/cray/pe/craype/2.5.6/bin/CC : ;' > user-config.jam 
./b2 -a toolset=gcc-2.5.6craycompute install
```

### Invoking CMake

After setting up the environment, the rest of the configure and build is 
the same as any other platform with a couple of exceptions.  First, you need 
to tell CMake that we are building for the Cray Linux Environment (CLE) using 
`CMAKE_SYSTEM_NAME` variable.  Second, NNTI finds the IBVerbs headers and 
libraries during configuration, but there is no IB software or hardware on 
the compute nodes which can lead to launch failures.  To disable the IBVerbs 
transport, use the `NNTI_DISABLE_IBVERBS_TRANSPORT` variable.  An example 
of both are shown below.
```
cd faodel
ROOT_DIR=$(pwd)
INSTALL_DIR=$ROOT_DIR/install
mkdir build
cd build
cmake \
    -DCMAKE_SYSTEM_NAME=CrayLinuxEnvironment \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
    -DNNTI_CACHE_ARGS="-DNNTI_DISABLE_IBVERBS_TRANSPORT:BOOL=TRUE" \
    -DNETWORK_LIBRARY={libfabric|nnti} \
    -DGTEST_ROOT=${GTEST_ROOT} \
    -DBOOST_ROOT=${BOOST_ROOT} \
    ..
```
After the configuration step, build the whole stack by running make as
you normally would:
```
make -j 4
```

Installing on Kahuna (Generic InfiniBand Cluster)
-------------------------------------------------
Kahuna is a generic InfiniBand cluster that
uses [Spack](https://spack.readthedocs.io) to generate its software
modules. The following instrutions provide an example of how to build
the environment.

### Loading Modules

Kahuna does not load any modules by default which means that some required 
software will not be detected during the configuration stage.  Begin by 
loading the modules that provide the required software.
```
module load mpi-gcc6
module load cmake
module load pkg-config
module load gcc/6.3.0
module load openmpi/1.10.3
```
CMake has a hardcoded table of Boost versions that it knows how to
configure.  It is important that the CMake version matches the Boost
version.  The recommended version of Boost for FAODEL is 1.60.0
which is compatible with the default cmake/3.8.1 module on kahuna.  If you
need a later version of Boost, you will also need a later version of CMake.
Confirm that we have CMake 3.8.1 or later.
```
module whatis cmake
```

### Using Kahuna's gtest/boost modules

Kahuna has basic installations of Google Test and Boost. If you want
to use these installations instead of compiling them yourself, do the
following:

```
module load boost
module load googletest
export GTEST_ROOT=$GOOGLETEST_ROOT
```


Platform-Specific Configuration Notes
=====================================

Users may which to made the following configuration settings
modifications in order to run applications on Mutrino and
Kahuna. These modifications can be made without recompiling the
programs by creating a file with the settings in it, and then setting
the environment variable `FAODEL_CONFIG` to point to the file.


Configuring for Mutrino (Cray XC40)
-----------------------------------

The FAODEL creates network endpoints for each process at 
startup.  The endpoints allow processes to find each other and to 
allow the users to query the state of the process. By default, FAODEL
will look for an `eth` device to bind to.  The mutrino 
compute nodes do not have an `eth` device, so we need to provide an 
alternate device like this:
```
webhook.interfaces       ipogif0
```

The default NNTI transport is MPI.  On mutrino, the prefered tranport is 
UGNI which is selected like this:
```
net.transport.name       ugni
```

The default libfabric provider is sockets.  On mutrino, the prefered 
tranport is UGNI which is selected like this:
```
net.transport.name       ugni
```

Configuring for Kahuna (Generic InfiniBand Cluster)
---------------------------------------------------

The FAODEL creates network endpoints for each process at
startup.  The endpoints allow processes to find each other and to
allow the users to query the state of the process. By default, FAODEL
will look for an `eth` device to bind to.  The correct 
device on kahuna compute nodes is `eth2`, which should be correctly 
identified.  If FAODEL does not select `eth2` and connections 
fail, we can provide an alternate device like this:
```
webhook.interfaces       eth2
```

The default NNTI transport is MPI.  On kahuna, the prefered tranport is
IBVerbs which is selected like this:
```
net.transport.name       ibverbs
```

The default libfabric provider is sockets.  On kahuna, the prefered 
tranport is IBVerbs which is selected like this:
```
net.transport.name       ibverbs
```



