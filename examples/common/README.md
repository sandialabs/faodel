Faodel Common Examples
======================

The Faodel Common library provides basic functions that are used in
different parts of Faodel. This directory contains some basic examples of
how to use the common components. The current set of examples includes:

data_types
----------
The data_types example shows how to use some common data types in the
Faodel, such as nodeid_t.

info_interface
--------------
Common provides an InfoInterface class that let's Faodel users get 
debug information about the components that are in a hierarchy. This example
shows how you can use the interface in your own code.

logging_interface
-----------------
Some Faodel components have a logging interface for controlling
debug and info messages. These examples show how to add that interface
to your own classes.

bootstrap
---------
Many of the services in Faodel need to be started in a specific order. 
Bootstrap provides a way to define dependencies between services. The
bootstrap example shows how a user can made their own class (or code)
part of the bootstrap process.

singleton
---------
Most of the services in the Faodel are singletons that are
started up by bootstrap. This example shows how to declare
dependencies between singleton classes and have them automatically
register themselves so they will be usable with bootstrap.

Note: We do NOT recommend using this trickery for registering singletons,
      as it is prone to linking problems.

