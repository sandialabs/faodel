Lunasa: A Library for Managing Network Memory
=============================================

Lunasa is a library that provides programmers with a simple and efficient
interface for reusing network-registered memory. Reuse allows the cost 
of registering and deregistering memory to be amortized over the lifetime
of the application. As a result, Lunasa can, in many cases, significantly
reduce the cost of memory registration.

Lunasa Data Objects
-------------------

Users manage and access memory allocated by Lunasa through instances of
the Data Object class. These objects are commonly identified as 
_Lunasa Data Objects_(LDOs).

LDOs are divided into four sections:

* **Local Header**: bookkeeping information used internally by Lunasa

* **Header**: description of LDO contents, potentially useful for RDMA
    operations (i.e., allows the receiver to determine the number of bytes 
    that were transferred and where section boundaries lie).

* **User Metadata & User Data**: these two sections represent the memory 
    expressly and directly managed by the user. The user can create 
    semantically-distinct regions by partitioning data between the data
    and metadata sections.

The primary objective of Lunasa is to exclusively manage
network-registered memory. However, it also provides the ability to
create LDOs from user-allocated memory ("User LDOs"). In this case,
Lunasa will allocate network-registered memory for the Local Header
and Header sections. Lunasa will register the user-allocated memory
and use it to store the User Metadata and User Data sections. To
fully exploit the advantages provided by Lunasa, programmers are
encouraged to use User LDOs only where necessary.

Allocator Types
---------------

Lunasa supports two allocators: _Lazy_ and _Eager_. The programmer
can select which allocator to use for each allocation. The lazy
allocator allocates unregistered memory. Memory allocated with this
allocator will be registered when and if the programmer requests a
RDMA handle. The eager allocator allocates memory that has already
been registered. Programmers are encouraged to use the _Eager_
allocator wherever possible.


Memory Managers
---------------

Lunasa supports two memory managers: _malloc_ and _tcmalloc_. 

The **malloc** allocator is essentially a wrapper for the standard
malloc library function. Memory for LDOs is acquired with _malloc()_
and registered as appropriate based on the allocator type,_see_
**Allocator Types**. When the LDO is deleted, the memory is
unregistered and released using _free()_.

The **tcmalloc** allocator is based Google's tcmalloc library. During
initilization, Lunasa'a tcmalloc allocator acquires a large block of
memory. If tcmalloc is used as the eager allocator, it registers the
entire block of memory. LDOs are allocated from this block of memory.
Lunasa requests additional blocks of memory as free space is
exhausted. Only one instance of the tcmalloc allocator is currently
allowed. To obtain the greatest performance benefit, the programmer
is encouraged to use tcmalloc as the eager allocator.

Build and Configuration Settings
================================

Build Depndencies
-----------------

Lunasa has a few library dependencies:
| Dependency      | Information                         |
| --------------- | ----------------------------------- |
| FAODEL:SBL      | Uses logging capabilities for boost |
| FAODEL:Common   | Uses bootstrap and nodeid_t         |
| FAODEL:WebHook  | For status info                     |

Compile-Time Options
--------------------

Lunasa does not currently have add any unique compile-time options.

Run-Time Options
----------------

When started, Lunasa examines the Configuration passed to it for the
following variables:

| Property                    | Type        | Default  | Description                                       |
| --------------------------- | ----------- | -------- | ------------------------------------------------- |
| lunasa.eager_memory_manager | string      | tcmalloc | Select memory allocator used on eager allocations |
| lunasa.lazy_memory_manager  | string      | malloc   | Select memory allocator used on lazy allocations  |


Lunasa's eager and lazy memory allocations manage separate pools of memory. 
While the tcmalloc allocator is faster at reallocating memory, only one
of the two pools can use it. 


Why the name "Lunasa"?
======================
Nathan Fabian picked the name Lunasa for this memory management
library, because in Scottish Gaelic Lunasa is a Gaelic festival
marking the beginning of the harvest season. The thinking is that the
memory manager maintained large patches of memory and harvested them
when users needed allocations.
