Faodel-Stress: A Tool for Examining the Performance of Host Systems
=====================================================================

When porting FAODEL to a new platform, it can be useful to run some basic 
benchmarks to understand the strengths and weaknesses of the new platform.
The [stress-ng][https://wiki.ubuntu.com/Kernel/Reference/stress-ng] tool
is an excellent collection of microbenchmarks that can help you compare
a new system to an old system. 

The `faodel-stress` tool follows `stress-ng`'s example and provides a
collection of stress tests that are relevant to data management tasks.
Each test performs the same operation in a tight loop for a specified 
amount of time. Users can vary the number of worker threads used in
each test to explore how well the system scales. Performance is reported
back to the user in terms of millions of operations per second. (Mops/s),
where an operation is a basic but non-trival task (eg, hashing a string).

While it is meaningless to compare the Mops/s of one stressor to another,
users can use the reported numbers to compare different machines or
configurations. 

Stressors
---------
The basic categories of stressors are as follows:

- **keys:**: These stressors generate a collection of Kelpie keys and then
  sort them.
- **memalloc**: Lunasa's memory allocator is used to obtain either plain
  memory (ie, not registered with the nic) or registered memory (ie,
  memory the nic can access).
- **localpool**: Kelpie is used to write a large number of objects into
  the node's local key/blob store. These tests vary whether threads write
  to the same row (ie, maximize contention) or independent rows.
- **serdes**: Multiple data structures are serialized/deserialized using
  a variety of packing libraries. The Particles example mimics a particle
  dataset where there are many particles with a small number of data values.
  The Strings example packs a variety of variable-length strings into 
  an object.
- **webclient**: These stressors start a whookie server and then issue
  a number of web get commands to fetch data. These operations take
  place over traditional sockets.
  
Recommended Settings
--------------------
It is useful to start by running the stressors with a single thread and
a short duration (5s) to verify the system can run to completion. As 
more threads and longer runtimes are used, it is likely that a few tests
will exhaust all the resources in the system and cause the test to hang.
Using a duration of 30s is generally good enough to load a system and
get usable numbers.




