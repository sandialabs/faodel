FAODEL-CLI: A Multipurpose Tool for Interacting with FAODEL Services
=====================================================================

The FAODEL cli tool is an all-in-one tool for doing a wide variety of 
FAODEL tasks. You can:

- Get build and configuration info for the current installation
- Launch DirMan and Kelpie servers
- Define/remove/list the Kelpie pool metadata maintained in a DirMan server
- Put/Get/List/Save/Load objects to/from a Kelpie pool
- Launch an mpi job to blast data into a kelpie pool
- Re-play a script with different commands in it
- Query whookie services on platforms that lack wget/curl


The documentation for the cli tool is provided in the `help` command. 

Help
----
```
$ faodel help
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  build-info       | binfo          : Display FAODEL build information
  config-info      | cinfo          : Display the Configuration tools will use
  config-options   | copt           : List configuration options FAODEL inspects
  whookie-get      | wget    <url>  : Retrieve a faodel service webpage
  dirman-start     | dstart         : Start a dirman server
  dirman-stop      | dstop          : Stop a dirman server
  resource-list    | rlist   <urls> : Retrieve list of known resource names
  resource-define  | rdef    <urls> : Define new resource
  resource-drop    | rdrop   <urls> : Remove references to resources in dirman
  kelpie-start     | kstart  <urls> : Start a kelpie server
  kelpie-stop      | kstop   <urls> : Stop a kelpie server
  kelpie-put       | kput    <args> : Publish data to kelpie
  kelpie-get       | kget    <args> : Retrieve an item
  kelpie-get-meta  | kgetm   <args> : Retrieve metadata for item
  kelpie-info      | kinfo   <keys> : Retrieve info for different keys
  kelpie-list      | klist   <key>  : Retrieve key names/sizes
  kelpie-save      | ksave   <keys> : Save objects from a pool to a local dir
  kelpie-load      | kload          : Load objects from disk and store to a pool
  kelpie-blast     | kblast         : Run MPI job to generate kelpie traffic
  all-in-one       | aone    <urls> : Start nodes w/ dirman and pools
  play-script      | play    script : Execute commands specified by a script
  help             | help    <cmd>  : Provide more info about specific commands

```

dstart: dirman-start
--------------------

```
$ faodel help dstart
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  dirman-start     | dstart         : Start a dirman server


DirMan is a service for keeping track of what resources are available in a
system. A user typically launches one dirman server and then establishes
one or more resource pools for hosting data. This command launches a dirman
server and then waits for the user to issue a dstop command to stop it.

In order to make it easier to find the dirman server in later commands,
dirman-start creates a file with its nodeid when it launches. By default this
file is located at ./.faodel-dirman. You can override this location by
setting the dirman.write_root.file value in your $FAODEL_CONFIG file, or
by passing the location in through the environment variable
FAODEL_DIRMAN_ROOT_NODE_FILE.

Examples:

  # Start and generate ./.faodel-dirman
  $ faodel dstart
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=$(pwd)/.faodel-dirman
  $ faodel fstop

  # Start and specify file
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=~/.my-dirman
  $ faodel dstart
  $ faodel dstop
```

kstart: kelpie-start
--------------------
``` 
faodel help kstart
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  kelpie-start     | kstart  <urls> : Start a kelpie server


After defining resource pools with the rdef command, users will need to start
nodes to run kelpie servers that can join as nodes in the pool. When launching
a kelpie server, a user specifies a list of all the pool urls that the server
will join. Internally, a server locates dirman and issues a Join command to
volunteer to be a part of the pool. The server will continue to run until
the user issues a kstop command for all of the pools that a sever initially
was configured to join.

Example:

  # Start and generate ./.faodel-dirman
  $ faodel dstart
  $ export FAODEL_DIRMAN_ROOT_NODE_FILE=$(pwd)/.faodel-dirman

  # Define a pool with two members
  $ faodel rdef "dht:/my/dht&min_members=2"
  $ faodel kstart /my/dht &
  $ faodel kstart /my/dht &

  # Stop the pool
  $ faodel kstop /my/dht
```
kput: kelpie-put
--------------------
```
$ faodel help kput
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  kelpie-put       | kput    <args> : Publish data to kelpie


kelpie-put arguments:
  -p/pool pool_url           : The pool to publish to (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

  -k1/--key1 rowname         : Row part of key
  -k2/--key2 colname         : Optional column part of key
     or
  -k/--key "rowname|colname" : Specify both parts of key, separated by '|'

  -f/--file filename         : Read data from file instead of stdin
  -m/--meta "string"         : Add optional meta data to the object

The kelpie-put command provides a simple way to publish a data object into
a pool. A user must specify a pool and a key name for the object. If no
file argument is provided, kelpie-put will read from stdin until it
gets an EOF. This version of the command is intended for publishing a
single, contiguous object and will truncate the data if it exceeds
the kelpie.chunk_size specified in Configuration (default = 512MB).

Examples:

  # Populate from the command line
  faodel kput --pool ref:/my/dht --key bob -m "My Stuff"
     type text on cmd line
     here, then hit con-d con-d to end

  # Use another tool to unpack a file and pipe into an object
  xzcat myfile.xz | faodel kput --pool ref:/my/dht --key1 myfile

  # Load from a file and store in row stuff, column file.txt
  faodel kput --pool ref:/my/dht --file file.txt --key "stuff|file.txt"

```

kget: kelpie-get
--------------------
```
faodel help kget
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  kelpie-get       | kget    <args> : Retrieve an item


kelpie-get arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

  -k1/--key1 rowname         : Row part of key
  -k2/--key2 colname         : Optional column part of key
     or
  -k/--key "rowname|colname" : Specify both parts of key, separated by '|'

  -f/--file filename         : Read data from file instead of stdin
  -i/--meta-only             : Only display the meta data for the object

The kelpie-get command provides a simple way to retrieve an object from a
pool. A user must specify the pool and key name for an object. If no file
argument is provided, the data will be dumped to stdout. A user may also
select the meta-only option to display only the meta data section of the
object.

Examples:

  # Dump an object to stdout and use standard unix tools
  faodel kget --pool ref:/my/dht --key mything | wc -l

  # Dump an object to file
  faodel kget --pool ref:/my/dht --key "stuff|file.txt" --file file2.txt
```

kblast: kelpie-blast
``` 
$ faodel help kblast
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  kelpie-blast     | kblast         : Run MPI job to generate kelpie traffic


kelpie-blast Flags:
 -a/--async              : send many objects per timestep before blocking
 -m/--reuse-memory       : allocate LDOs in advance and reuse them
 -r/--rank-grouping      : ensure all of rank's data lands on same server
 -s/--skip-barrier       : skip the barrier that happens at start of cycle

kelpie-blast Options:
 -t/--timesteps x        : Run for x timesteps and then stop (default = 1)
 -T/--delay x            : Delay for x seconds between timesteps

 -o/--object-sizes x,y   : List of object sizes to publish each timestep

 -p/--external-pool pool : Name of an external pool to write (eg '/my/dht')
 -P/--internal-pool pool : Type & path of internal pool to write to
                           (eg 'dht:/tmp', 'local:/tmp' for disk, or 'dht:')


The kelpie-blast command provides a parallel data generator that can produce
a variety of traffic conditions. When you run as an mpi job, each rank will
follow a bulk-sync parallel flow where each rank sleeps, dumps a collection
of objects, and then does an optional barrier. Output is in a tab-separated
format that is easy to parse.

Examples:
 mpirun -n 4 faodel kblast -P local:/tmp -t 10  # Write 10 timesteps to /tmp
 mpirun -n 4 faodel kblast -P dht:/tmp -o 1k,2M,32  # Pub 3 objects/timestep
 mpirun -n 4 faodel kblast -p /my/pool            # Connect to external pool

```

aone: all-in-one
----------------
```
$ faodel help aone
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  all-in-one       | aone    <urls> : Start nodes w/ dirman and pools


The all-in-one option launches an mpi job that includes a dirman server, a
collection of kelpie servers (one per rank), and any pool settings you've
defined in either your configuration or the command line.
Example:

  mpirun -N 4 faodel aone  "dht:/x ALL" "rft:/y 0-middle" "dht:/z 2"
  # Use 4 nodes with
  #    "dht:/x ALL"       dht named /x on all four ranks
  #    "dft:/y 0-middle"  rft named /y on second half of ranks
  #    "dht:/z 2"         dht named /z just on rank 2

```

play: play-script
-----------------
``` 
faodel help aone
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  all-in-one       | aone    <urls> : Start nodes w/ dirman and pools


The all-in-one option launches an mpi job that includes a dirman server, a
collection of kelpie servers (one per rank), and any pool settings you've
defined in either your configuration or the command line.
Example:

  mpirun -N 4 faodel aone  "dht:/x ALL" "rft:/y 0-middle" "dht:/z 2"
  # Use 4 nodes with
  #    "dht:/x ALL"       dht named /x on all four ranks
  #    "dft:/y 0-middle"  rft named /y on second half of ranks
  #    "dht:/z 2"         dht named /z just on rank 2


cdulmer@vortex:~/projects/faodel/build$ ./tools/faodel-cli/faodel help play
faodel <options> COMMAND <args>

 options:
  -v/-V or --verbose/--very-verbose : Display runtime/debug info
  --dirman-node id                  : Override config and use id for dirman

 commands:
  play-script      | play    script : Execute commands specified by a script


Play a series of commands that setup the FAODEL environment. A script may
contain both configuration info and (most) actions that are part of the
faodel tool. The following is a brief example that shows the basic format.
More examples can be found in faodel/examples/faodel-cli/playback-scripts.
```
The following is an example of a play script. There are a few other examples
in the `faodel/examples/faodel-cli/playback-scripts` directory.

```
# Hello world play script
config bootstrap.debug true                    # Turn on some debug messages
config dirman.root_node_mpi 0                  # Set first node as dirman root
config dirman.resources_mpi[] dht:/my/dht ALL  # Create a pool on all ranks

set pool /my/dht                               # Set default pool
set rank 0                                     # Set default rank

barrier                                        # Do an mpi barrier
rlist -r 0 /my/dht                             # Rank 0 lists info for dht

barrier                                        # Do an mpi barrier
kput -D 1k -k object1                          # Rank 0 writes 1 KB object
kput -p local:[abc] -D 1k -k object2           # Rank 0 write to local pool
klist -p /my/dht -k *                          # Show everything in dht
kinfo -p /my/dht -k object1                    # Get more info on object1

barrier
ksave -d ./tmp -k object1                      # Save an object to ldo file
kload -d ./tmp -k object1 -p local:[abc]       # Load and write to new pool
klist -p local:[abc] *                         # Show all local items

print ...And now to grab a file and display..  # Print some text
kput --file /etc/profile -k profile_file       # Send a plain file to pool
kget -k profile_file                           # Grab file and display
barrier

```