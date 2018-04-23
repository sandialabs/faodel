# Using precompiled headers to improve NNTI compile times

## What are precompiled headers?

Precompiled headers are C++ header files that have been converted to 
an intermediate format that can be more easily and quickly included 
by the compiler.  This can significantly reduce compile times for 
large complicated headers.  The GNU C++ compiler supports 
precompiled headers with some caveats.

## How to use precompiled headers with NNTI

To use precompiled headers when building NNTI follow these steps:
- edit scripts/build_pch.sh to set the compiler, SBL and Boost paths
- run cmake to configure NNTI
- run "sh scripts/build_pch.sh" to generate nnti_pch.hpp.gch
- make

## When to generate precompiled headers

nnti_pch.hpp.gch only needs to be generated in these cases:
- the first time building a new NNTI
- if a new Boost is installed
- if a new SBL is installed
- if the g++ version changes
- if the compiler options change

## Diagnosing problems with precompiled headers

If compile times are not reduced after following the above steps, 
run "make VERBOSE=1" to find the compile command for your source 
file.  Then add the -H option and manually compile.  The first 
line of output should look like this:
 
! <PATH_TO_NNTI>/nnti/nnti_pch.hpp.gch

If the first character is '.' instead of '!' then the precompiled 
header is missing.  Go back and run build_pch.sh. 

If the first character is 'x' instead of '!' then the precompiled 
header was rejected.  This most likely because the compiler options 
changed.  Ensure that the options in build_pch.sh match those in 
the "make VERBOSE=1" output.
