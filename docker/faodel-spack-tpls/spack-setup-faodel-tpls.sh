#!/bin/bash

BOOST_VERSION=1.66.0
GTEST_VERSION=1.8.0
LIBFABRIC_VERSION=1.6.1
OPENMPI_VERSION=3.1.2


TOOLS="boost cereal leveldb googletest"


# Plug in specific versions of our dependencies
sed -i '/^packages:/a\
  boost:\
    version: ['${BOOST_VERSION}']\
  googletest:\
    version: ['${GTEST_VERSION}']\
  libfabric:\
    version: ['${LIBFABRIC_VERSION}']\
  openmpi:\
    version: ['${OPENMPI_VERSION}']' ${SPACK_ROOT}/etc/spack/defaults/packages.yaml




. /usr/share/Modules/init/sh 
. ${SPACK_ROOT}/share/spack/setup-env.sh 




# openmpi has options
spack -k install openmpi thread_multiple=True

# libfabric needs transports defined
spack install libfabric fabrics=sockets,verbs

spack install ${TOOLS}

# Python causes conflicts?
spack view --verbose -e python symlink -i ${SPACK_INSTALL} ${TOOLS} libfabric openmpi

# Clean all
spack clean -a
