FAODEL Overview
===============
FAODEL (Flexible, Asynchronous, Object Data-Exchange Libraries) is a 
collection of software libraries that are used to implement 
different data management services on high-performance computing (HPC)
platforms. This project is part of the Advanced Technology Development
and Mitigation (ATDM) effort for NNSA's ASC program at Sandia 
National Laboratories. 

Components
----------
FAODEL is composed of multiple libraries:

- [Kelpie](src/kelpie/README_Kelpie.md): Kelpie is a distributed memory
  service that enables applications to migrate different data objects
  between compute nodes in a platform. It utilizes out-of-band RDMA
  communication to enable different MPI jobs to interact with each
  other.
- [DirMan](src/dirman/README_DirMan.md): DirMan is a service for 
  managing runtime information (e.g., a list of nodes that make up
  a pool for storing data).
- [OpBox](src/opbox/README_OpBox.md): OpBox is a communication engine
  responsible for orchestrating complex communication patterns in a
  distributed system. Rather than use traditional remote-procedure
  call (RPC) techniques, communication is facilitated through state
  machines called Ops. Ops allow distributed protocols to run
  asynchronously, without explicit maintenance by user services.
- [Lunasa](src/lunasa/README_Lunasa.md): Lunasa is a memory management unit
  for data that may be transmitted on the network using RDMAs. In
  order to reduce network registration overheads, Lunasa allocates
  sizable amounts of registered memory and then suballocates it to
  applications through tcmalloc. User allocations are described by
  Lunasa Data Objects (LDOs), which provide reference counting and
  object description in the stack.
- [NNTI](src/nnti/README_NNTI.md): NNTI is a low-level, RDMA portability
  layer for high-performance networks. It provides application with
  the ability to send messages and coordinate RDMA transfers via
  registered memory.
- [Whookie](src/whookie/README_Whookie.md): Whookie is a network service for
  FAODEL nodes that enables users and applications to query
  and change the state of a node via an HTTP connection.
- [Services](src/faodel-services/README_Services.md): Basic services that make it
  easier to write communication applications.  
- [Common](src/faodel-common/README_Common.md): Common is a collection of data types
  and software functions that are used throughout FAODEL.
- [SBL](src/sbl/README_SBL.md): The Simplified Boost Logging (SBL) library
  provides a way to map log information in FAODEL components to
  Boost's logging library.
  
Additional Information
======================
This release includes files to help guide users. The files are:

- [INSTALL](INSTALL.md): Details about how to configure, build,
  install and run the software provided in this release. This document
  is a good starting point, as the build process can be challenging on
  different platforms.
- [LICENSE](LICENSE.md): The FAODEL code uses the MIT license.
- [NEWS](NEWS.md): The news file provides a history of major changes provided
  with each release of this software. Developers should review this document
  when switching to a new release.

Contributors
============
The following developers contributed code to the FAODEL:

- Nathan Fabian
- Todd Kordenbrock
- Scott Levy
- Shyamali Mukherjee
- Gary Templet
- Craig Ulmer
- Patrick Widener

The following helped contribute ideas and provided feedback for the project:

- Margaret Lawson
- Jay Lofstead
- Ron Oldfield
- Jeremy Wilke

This release includes third-party software that contains its own licensing
and copyright info:
- cereal (in tpl/cereal)
- gperftools (in tpl/gperftools)
- Boost ASIO examples (in src/whookie/server)

Copyright
=========
Copyright 2018 National Technology & Engineering Solutions of Sandia, LLC
(NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S. 
Government retains certain rights in this software. 
