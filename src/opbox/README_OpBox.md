OpBox: An Asynchronous Communication Engine
===========================================

OpBox is a library for implementing asynchronous communication between
multiple entities in a distributed application. Our experiences with
remote procedure call (RPC) libraries found that while RPCs provide a
simple way to coordinate data transfers and invoke action at remote
nodes, it is often difficult to coordinate more sophisticated data
management services (eg, ones involving more than two nodes, time-out
conditions, or race conditions). Rather than leave the task of
coordinating transfers entirely to the next layer up, we advocate
providing the user with primitives for expressing a protocol as a
state machine that the communication layer can process in an
asynchronous manner. A communication pattern between a collection of
nodes in OpBox is an **Op**. Users create an Op class, retrieve
application-specific handles back to the Op (eg, a future/promise) and
then hand the Op over to OpBox via Launch to start the state machine.


Terminology
-----------

The following terminology is used throughout OpBox:

- **Op**: An Op is a C++ class that implements state machines for the
    nodes that are involved in the communication. A user-defined Op is
    derived from the Op base class and includes both the origin and
    target state machines. All Ops must be registered with the library
    in order to be used.
- **Origin**: The origin is the node that initiates an Op. A user
    typically creates a new Op and hands all the information necessary
    for communication to the Op's constructor.
- **Target**: The target is any node in the communication pattern that
    isn't the origin. When a new Op request arrives from the network,
    OpBox creates a new target instance of the Op and executes the first
    step in its target state machine.
- **op-id**: Each Op class must have its own, unique Op Identifier
    (op-id) to ensure that all nodes follow the correct state machines
    when executing a particular op. The op-id value for the "Ping" and
    "Hello" Ops are different. However, multiple Ping operations can be
    in flight at the same time by specifying different mailbox
    ids. While users can pick whatever values they want with op-ids
    (excluding collisions), the standard convention is to define a
    human-readable string for the name of the op and then use a constant
    hash to map it to a discrete value.
- **Mailbox**: A mailbox identifies a specific instances of a
    particular, in-flight Op. Each Op that is launched is assigned a
    mailbox to give the target an ID for sending back a response.
- **`nodeid_t`**: A universal ID for referencing a node that can easily
    be passed between nodes and used.
- **`peer_ptr_t`**: A local handle for referencing a remote node. This
    handle is a pointer to the actual data structures used to
    communicate with a node and is therefore quicker to use because it
    avoids a lookup.


Initializing OpBox
------------------

It is expected that developers will use the common Bootstrap and
Configuration classes to start OpBox in each rank that needs to
communicate. After starting all services on a rank (including OpBox),
a user can obtain the nodeid for the rank and share it with other
nodes (eg mpi broadcast) that wish to communicate.

```
common::Configuration config(my_config_string);
common::bootstrap::Start(config, opbox::bootstrap);

common::nodeid_t myid = opbox::GetMyID();

//Share node ids with all other mpi ranks
nodes = new common::nodeid_t[mpi_size];
MPI_Allgather(&myid, sizeof(common::nodeid_t),MPI_CHAR,
              nodes, sizeof(common::nodeid_t),MPI_CHAR,
              MPI_COMM_WORLD);
```

Creating a New Op
-----------------

Developers can create their own Ops and use them with OpBox. The
examples directory provides several scenarios of how to create and
register Ops. The main points are:

- **Inherit From `opbox::Op`**: The Op class defines the interface for
    the Op and provides basic functions.
- **Static Variable Naming**: For consistency, we typically use two
    static variables in an Op to identify it: op_id and op_name. Using
    the const_hash of the name to generate the ID removes the need for a
    developer to keep a list of known IDs. Hash collisions cause errors
    during registration. Simply pick a different Op name until the
    collision goes away.
- **Op Constructors**: An Op must have a constructor for both the
    Origin and the Target. The origin constructor provides an
    opportunity for the developer to pass in information the Op will
    need for it to progress (eg the target nodes or application
    data). The target constructor takes a single argument
    (`op_create_as_target_t`) and is only called by OpBox when a new
    incoming message arrives.
- **Update Origin/Target**: An Op's state machines are defined in the
    UpdateOrigin and UpdateTarget functions. These functions are only
    called by OpBox. Both are passed an OpArgs argument to specify what
    change has happened in OpBox to trigger an update to the state
    machine. In most cases, the change is due to the arrival of a new
    message. Results may be returned through the `results_t` argument.
- **Updating Op State by returning WaitingType**: After analyzing a
    new event, the UpdateOrigin/Target functions **must** return a
    WaitingType return code. The WaitingType specifies what the Op is
    doing now (eg, waiting on a new message), and is used by OpBox to
    determine what to do with the Op (eg, put it on a wait list or
    destroy it).
- **Message Processing**: Ops may transmit or receive messages, as
    well as initiate RDMA transfers. Messages have a small header with a
    specific number of fields, and may be extended to include
    application-specific information. Users can pack data for these
    messages through any means they see fit (eg, POD or Boost
    Serialization).
- **Sending Messages**: A developer may use `opbox::net::SendMsg` to
    transmit a message, provided that it resides in a Lunasa Data Object
    (LDO). It is typically beneficial to create a function to construct
    the message, if it is used in more than one place.
- **Returning Information to the User**: Developers need a way to pass
    information from an Op to the application that is running on a
    node. In the target case, handoffs are typically performed the same
    way that RPCs are executed: the target op calls a function that
    presents data to (or invokes action at) the target rank. This style
    of communication is also possible at the Origin. However, it may be
    more useful for the Origin's state machine to pass results back
    through a C++ future. Note: futures must be obtained before
    launching an Op, as OpBox will destroy the Op after it has been
    launched.


Registering a New Op
--------------------

The following points relate to how custom Ops are registered with
OpBox:

- **Registering**: Each Op must be registered with OpBox before it can
    be used. Manual registration can be done by passing the Op ID, name,
    and a callback for creating a new target Op to RegisterOp().
- **Registration Phases**: A user may register a new Op at any
    point. However, registering an Op before bootstrap Starts allows the
    Op to be placed in a list that does not incur locking overhead (and
    is thus faster).
- **Registration Template**: If the Op uses the standard id and name
    fields, a RegisterOp template can be used to simplify the
    registration.

Launching an Op
---------------

Once the OpBox service is started a user can create a new Op and then
hand it over to OpBox for execution. The following example illustrates
how a new Op is registered, constructed, and launched:

```
int main(){

  // Register before Start in order to avoid locks
  opbox::RegisterOp<OpMyExample>();

  // Start all services, using default settings
  common::bootstrap::Start(common::Configuration(),
                            opbox::bootstrap);

  ... // Share node ids

  // Create a new op
  auto op = new OpMyExample(node, "hello");

  // Get a future from it for later use
  future<int> f = op->GetFuture();

  // Launch it. Op is owned by opbox after launch
  opbox::LaunchOp(op);

  //Get the return value
  int num = f.get();
}
```


Build and Configuration Settings
================================

Build Dependencies
------------------

OpBox has the following build dependencies:

| Dependency     | Information                         |
| -------------- | ----------------------------------- |
| FAODEL:SBL     | Uses logging capabilities for boost |
| FAODEL:Common  | Uses bootstrap and `nodeid_t`       |
| FAODEL:Whookie | For status info and new connections |
| FAODEL:Lunasa  | For network memory management       |
| Boost          | Uses boost and systems components   |

OpBox also needs a networking library with RDMA primitives. It can use
one of the following:

| Dependency  | Information                                 |
| ----------- | ------------------------------------------- |
| FAODEL:NNTI | Nessie Network Transport Interface          |
| libfabric   | TPL that supports many different transports |


Compile-Time Options
--------------------

OpBox uses the following flags at compile time when they're passed
into CMake:

| CMake Flag                        | Description                   |
| --------------------------------- | ----------------------------- |
| Faodel_NETWORK_LIBRARY                   | Either "nnti" or "libfabric"  |



Run-Time Options
----------------

When OpBox starts it examines the Configuration object provided to it
for the following run-time options:


| Property              | Type        | Default  | Description                                         |
| --------------------- | ----------- | -------- | --------------------------------------------------- |
| opbox.type            | string      | standard | Select the opbox implementation type                |
| net.transport.name    | string      | none     | Network library to use, default is system dependent |
| net.log.debug         | bool        | false    | When true, output debug messages                    |
| net.log.info          | bool        | false    | When true, output info messages                     |
| net.log.warn          | bool        | false    | When true, output warning messages                  |
| net.log.error         | bool        | false    | When true, output error messages                    |
| net.log.fatal         | bool        | false    | When true, output fatal messages                    |
| net.log.filename      | string      | none     | When set, direct log messages to file else stdout   |

