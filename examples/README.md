FAODEL Examples
===============

The FAODEL project provides a set of different libraries that
developers can use to create complex data management services. Each
library provides its own set of examples and tutorials to help
developers get started with the software. The examples are located in
the examples directory of each project.

Examples Philosophy
-------------------
We assume that most developers just want to start using the
FAODEL libraries without having to dig into the guts of the
source code or its (admittedly) complex build system. Thus, our
philosophy for providing examples is that they should be stand-alone
code that's outside of the core build system, that users can pick up
and use as templates for their own projects. The overall build process
for users is to:

- Download the FAODEL source code
- Build and install the libraries
- Build one or more examples, with each one finding/using the
  installed libs 
  

Components with Examples
------------------------
The current list of FAODEL projects with examples is: common,
webhook, lunasa, nnti, opbox, and kelpie.

Linking Example
---------------

This directory contains one stand-alone example which is only for
testing whether libraries link properly. After building and installing
the FAODEL libraries, you can test the linking by doing the
following:

```
    cd faodel
    ROOT_DIR=$(pwd)
    INSTALL_DIR=$ROOT_DIR/install
    mkdir build_examples
    cd build_examples
    cmake \
      -DCMAKE_PREFIX_PATH=${INSTALL_DIR}/lib/cmake \
      ${ROOT_DIR}/examples
    make 
```
