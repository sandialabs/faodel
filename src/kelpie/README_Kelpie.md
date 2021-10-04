Kelpie: A Distributed, In-Memory Store for High-Performance Computing
=====================================================================

Kelpie is a distributed, in-memory key/blob store for high-perfomance
computing. It is intended to serve as a flexible way to exchange large
data objects between different nodes in the system. Each node is
equipped with a Local Key/Value (or LocalKV) store for caching data
objects that are relevant to the node. The LocalKV stores items in a
map where keys can be one dimensional (row notation) or two
(row/column notation). Network operations are provided in order to
publish/retrieve an object to/from one or more remote nodes in the
system. A collection of remote nodes can function as a pool for
storing data values in a way that distributes the load
appropriately. In order to improve asynchronous performance in a
distributed system, Kelpie also provides mechanisms for leaving
callbacks associated with a requested value at a remote node. These
callbacks provide a means of forwarding an object to multiple
locations when it becomes available.

**Note**: This release of Kelpie is considered a **preview** for
developers. Networking operations are disabled: a user may only
instantiate the **nonet** implementation.


Terminology
-----------

The following terminology is used throughout Kelpie:

- **bucket**: A bucket is a data namespace in Kelpie for keeping two
    collections of objects (possibly with the same key names) separate
    from each other in the same Kelpie resources. Applications with a
    single user generally do not need to specify a bucket (a default
    will be assigned). The bucket is not secure, as a bucket label is
    converted into a 32b value that is simply prepended to a key in
    Kelpie operations.
- **key**: A key is a simple identifier for a particular data object
    in the system. A key is composed of one or two C++ strings (thus
    ascii and binary keys are allowed). The first string serves as a row
    identifier, while the second is a column identifier. In some pools
    (eg a distributed hash table or DHT) the row identifier is used to
    determine which node will a house an item. Other pools may use the
    column portion of the key for versioning or aging out older entries.
- **LocalKV**: Each node has a local key/blob storage unit for housing
    data objects. A LocalKV stores objects in two dimensions, in order
    to allow users to group related items together.
- **LDOs**: Kelpie uses a Lunasa Data Object (LDOs) to house each
    application object that is maintained in the system. An LDO is
    locally referenced counted (in order to prevent users from
    deallocating memory that is being exchanged with the network) and
    has knowledge about whether the allocation can be RDMA'd by the
    network. Users may create LDOs, use them, and hand them over to
    Kelpie in order to minimize copying.
- **pool**: A pool is a collection of resources in the system that
    implement a single storage entity for applications. While a pool can
    simply refer to the node's LocalKV, it may also provide an interface
    into a distributed hash table (DHT) built from a set of nodes. Users
    may create and register their own pool code for controlling how data
    is distributed among a list of resources.
- **ResourceURL**: A ResourceURL provides information describing how a
    node can interact with a set of resources that implement a pool. A
    user may leverage OpBox's Dirman service to register and local
    pools, or implement their own means of sharing URLs.


Kelpie uses a modified version of the put/get API that is familiar to
most users. Users may invoke the following operations on a pool:

- **Publish**: This operation implies a result has been generated and
    that others in the system may want to use it. A publish operation
    pushes the data object to the node(s) responsible for housing it in
    the pool, and triggers any pending operations that are waiting on
    the item.
- **Want**: A want operation signifies that a particular node will
    require a particular data value at a later point in time. The user
    may provide an optional callback to invoke when the value is
    available. Want provides a way to establish good, asynchronous
    behavior in the overall system.
- **Need**: A need operation is a blocking operation that requires a
    particular data object be available before execution can continue. A
    need returns the actual, local LDO to the user (while generally
    retaining the LDO in the LocalKV).
- **Drop**: A drop operation signifies that an object is no longer
    needed and should be removed from the pool.

Users can also obtain information about objects that are in a pool. These
operations return with current information and do not wait for objects
to be published.

- **Info**: An info operation is a blocking request that retrieves
    information about a single object in the pool. The info command
    provides more detailed information about an object than a list
    command.
- **List**: Dispatch a query that retrieves all keys that exist in a
    pool that match a specific value or wildcard pattern. Wildcards
    can only be suffix based (eg `foo*` but not `foo*bar`). Results
    are the list of keys and the current object sizes.
  
Finally, Kelpie now provides an experimental mechanism for executing
compute operations on objects via user-defined functions (UDFs). UDFs
must be registered at start time and given a string label for 
referencing the function.

- **Compute**: The compute function retrieves one or more objects from
    a remote row, executes a UDF on the object(s), and then returns
    a new object back to the user. See the examples/kelpie/compute
    directory for more information on how to use Compute.
  

Connecting to a Pool
--------------------

In order to use a pool, a user must first make a connection to it. A
resource URL provides information that Kelpie can use to look up
information about a particular resource (eg, number of nodes, nodeid's
of its members, replication factors, etc) and connect with
it. Invoking a Connect operation to a pool will provide a pool handle
that the application can use. Pool handles are reference counted and
reused in the implementation, given that user operations cannot change
the pool's settings after creation.

Issuing Multiple Requests
-------------------------
Often it is desirable for users to issue a batch of asynchronous requests
and then at a later time block until all tasks are complete. While users
car create their own system for managing notifications via the pool
command's callback functions, Kelpie provides a `ResultCollector` class
that gathers results and blocks until all operations complete.

Pool Types
----------

Kelpie supports multiple types of pools and can be extended with new
functionality as needed. The following are the built-in pool types:

- **LocalPool**: Only reference the localkv store that is hosted in
    this node. 
- **DHTPool**: A distributed hash table (DHT) pool that spreads data
    across a collection of (static) nodes. The owner of the an
    object is determined by hashing the row portion of a key. This
    approach ensures all objects for a row reside on the same node.
- **RFTPool**: A rank-folding table (RFT) pool is similar to a 
    DHT, except the modulo of the producer's rank is used to select
    which node receives the data. Consumer nodes should provide
    the rank they wish to access in the pool construction request.
- **TracePool**: A trace pool records information about all the 
    requests a client made to a particular pool. The TracePool can
    be configured to relay requests to another pool.
- **NullPool**: The null pool simply drops all requests made to it.

Users may create their own pools by extending the PoolBase. We envision
that users may wish to handle replication or perform more customized
RDMA opertions in a pool.


Build and Configuration Settings
================================

Build Dependencies
------------------

Kelpie has a few library dependencies:

| Dependency      | Information                         |
| --------------- | ----------------------------------- |
| FAODEL:SBL      | Uses logging capabilities for boost |
| FAODEL:Common   | Uses bootstrap and nodeid_t         |
| FAODEL:Whookie  | For status info and new connections |
| FAODEL:Lunasa   | For network memory management       |
| FAODEL:OpBox    | For communication state machines    |
| Network Lib     | Either FAODEL:NNTI or libfabric     |
| Boost           | Uses boost and systems components   |

Compile-Time Options
--------------------

Kelpie does not currently have add any unique compile-time options.

Run-Time Options
----------------

When started, Kelpie examines the Configuration passed to it for the
following variables:

| Property                | Type            | Default  | Description                                                                     |
| ----------------------- | --------------- | -------- | ------------------------------------------------------------------------------- |
| kelpie.type             | standard, nonet | standard | Select between the standard networked core, or a debug option without networkin |


This release provides two kelpie implementation types:
- nonet: nonet is a standalone implementation that does not include any
         pools that have network operations. 
- standard: The standard implementation provides local and networked 
         pools.




Why the name "Kelpie"
=====================

A Kelpie is a horse-like beast in Scottish mythology. The name was
chosen to follow the theme of Scottish mythical creatures, started
with Nessie.
