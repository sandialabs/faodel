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

Example Scenario
----------------
Consider the case where an OpBox application needs to
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

DirMan Cores
------------
There are currently three types of dirman cores that can be used:

- **Centralized** (default): The centralized dirman core assigns one node
  in the platform to store all dirman information. While this node may perform
  other FAODEL functions, it must be started before the other nodes. The central
  dirman node has `dirman.host_root true` specified in its configuration file.
  All other nodes must have a pointer to this node specified in either
  their config (eg `dirman.root_node 0x1234`), a dirman root node file, or
  an environment variable (see `Root Node Resolution` for more info).
- **Static**: In the specialized scenario where all information about 
  resources is known at start time, users may use the `static` dirman
  core and plug all the information into the configuration.
- **None**: In some test scenarios, it is useful to launch services without
  a dirman service. This option disables dirman services from running, and
  is not useful in normal workloads.

Root Node Resolution
--------------------
When operating in the `centralized` mode, users must provide information to
their services about how to find the root dirman node. FAODEL checks for
this information using the following process:

1. `dirman.root_node` in Configuration: The easiest way to pass the root
   node id is to just place it in the configuration. This technique works
   especially well if you're using mpisyncstart and you specify the rank
   in the job that is the root via `dirman.root_node_mpi 0`.
2. `dirman.root_node.file` in Configuration: The second option is to read
   a file to learn the id of the root node. This method works well when the
   root node is also configured with `dirman.write_root file`.
3. Environment variable `FAODEL_DIRMAN_ROOT_NODE_FILE`: This option allows
   users to point to the root node file using an environment variable.
4. Environment variable `FAODEL_DIRMAN_ROOT_NODE`: The last option allows
   users to specify the root node as an environment variable.

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

| Property              | Type                    | Default     | Description                                                            |
| --------------------- | ----------------------- | ----------- | ---------------------------------------------------------------------- |
| dirman.type           | none,static,centralized | centralized | Static assumes all info is in config, centralized uses a single server |
| dirman.host_root      | bool                    | false       | When true, this node is the dirman root node                           |
| dirman.write_root     | filename                | ""          | Instruct the root node to write its id to a file                       |
| dirman.root_node      | nodeid                  | ""          | Use the node id supplied here to reference the root node (hex value)   |
| dirman.root_node.file | filename                | ""          | Read the file and use its contents to identify the root node           |
| dirman.resource[]     | url                     | ""          | Define a static resource in the configuration                          |

note: the `dirman.resource` command is typically used when `mpisyncstart` is
enabled, as it provides users with an easy way to define resources that the
mpi job will host. For example, a user might distribute a kelpie pool in their
job via:

```
mpisyncstart.enable  true   # Use mpisyncstart service to let us map ranks to faodel nodes
dirman.root_node_mpi 0      # Set rank 0 as the dirman root

dirman.resources_mpi[]  rft:/my/pool   ALL  # Make a pool across all ranks
dirman.resources_mpi[]  dht:/my/meta   END  # Make a pool w/ just the last rank 
dirman.resources_mpi[]  dht:/my/first  0-3  # Make a pool on the first four ranks
```