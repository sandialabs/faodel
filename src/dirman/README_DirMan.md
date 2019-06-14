DirMan: A Service for Storing Runtime Information
=================================================

FAODEL applications often need a simple way to manage runtime
information about resources that are available in the system. For
example, a Kelpie job may use a collection of nodes to implement a
distributed pool for storing large data objects. Client applications
must obtain information about how the pool is configured as well as
the addresses of all the nodes in the pool in order to interact with
it. While users can manually manage this data themselves (e.g.,
through file I/O or collectives), FAODEL provides a built-in service
called DirMan that can be used to handle a number of runtime
configuration tasks.

DirMan (or "Directory Manager") is a thin OpBox service that uses a
'directory' to associate a list of nodes with a resource name. The
directory name includes a path to the resource (eg,
/mydataset/mesh_block4/dht) in order to allow users to group related
directories together and to enable future implementations to
distribute the tree across multiple nodes. Each node in a directory
has a unique label associated with it. A directory may also contain a
string of information that can be used to record human-readable data
about the directory or to store additional application-specific data
for the directory.

One node in the system is designated as the **root node** for the
dirman service. The root node serves as the place nodes can query if
an application needs information about a particular directory. Nodes
cache directory information to reduce pressure on the root node. In
the current **centralized** implementation, all directory information
is stored at the root node. An application can create and register a
new directory via the HostNewDir() function, and lookup existing
directories via GetDirectoryInfo(). If a directory is not known during
a lookup, the user must resubmit the query.

The current implementation assumes that node collections are generated
in a **static** manner (ie, a configuration program will setup all
directories at start time and node memberships will last the lifetime
of the application). While nodes can opt to join a particular
directory via JoinDirWithName(), it may be more straightforward for
one node simply to volunteer a list of nodes via HostNewDir().

All of the dirman calls use a common **ResourceURL** to reference a
directory. While future implementations may use the nodeid field to
designate which node is responsible for hosting a directory, the
centralized implementation currently overwrites the nodeid with the
root node. Users should only populate that path and directory portions
of the ResourceURL when passing a ResourceURL into dirman.

Example Use: Consider the case where an OpBox application needs to
connect to a separate array of nodes that store data objects in
memory. In order to simplify how nodeid information is passed between
applications, we would expect three jobs to run. First, a single node
would start and serve as the dirman root for maintaining the nodeids
of other nodes in the system. The nodeid of this node will be captured
via job scripts and distributed to the other two jobs. Second, an mpi
job that started the array of nodes would start. These nodes would use
the dirman root id provided to them to connect with the root node in
order to allow them to register their nodeids in a new
directory. Finally, the application would start. It would use the
provided dirman root nodeid to connect with the root, obtain the list
of nodes for the array, and then start using the list. As this example
illustrates, dirman provides a scalable way to share runtime
information without having to write node information to files.
Build and Configuration Settings
================================

Build Dependencies
------------------

DirMan has the following build dependencies:

| Dependency     | Information                         |
| -------------- | ----------------------------------- |
| FAODEL:SBL     | Uses logging capabilities for boost |
| FAODEL:Common  | Uses bootstrap and `nodeid_t`       |
| FAODEL:Whookie | For status info and new connections |
| FAODEL:Lunasa  | For network memory management       |
| FAODEL:OpBox   | For communication via ops           |
| Boost          | Uses boost and systems components   |

Run-Time Options
----------------

When the DirMan service starts, it examines the Configuration object
for the following run-time options:

| Property              | Type        | Default  | Description                                         |
| --------------------- | ----------- | -------- | --------------------------------------------------- |
| dirman.host_root      | bool        | false    | When true, this node is the dirman root node        |
| dirman.root_node      | nodeid      | ""       | The nodeid of the root node (hex value)             |
