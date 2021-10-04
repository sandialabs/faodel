# Configuration File Cookbook

FAODEL applications load a configuration file at start time to determine
how different components should behave. FAODEL reads the environment
variable `FAODEL_CONFIG` to determine which file to load. This file provides
a summary of different options that can be supplied in the config file.

## General Configuration Options
### Using Node Role to select from multiple configurations
The `node_role` setting allows you to specify multiple behaviors for different
classes of systems in the same config file. Just prefix all your settings with
the role name, and then append specify the role for each machine:

```
server.backburner.num_threads  32  # crank up the threads on a dedicated server
client.backburner.num_threads  1   # minimize threads on a client

node_role server  # usually code sets this via config.Append("node_role","server");
```
### Setting the Security Bucket
It isn't used in many of our examples, but all kelpie calls use a security bucket
to help isolate different users or workloads in the store. Set the bucket:

``` 
secutity_bucket my_special_stuff
```


### Read Additional Configuration Files
While not recommended, your main configuration file can load additional config
files. 
``` 
config.additional_files config_file1;config_file2;config_file3
```

## Logging
### Changing the log levels
Components that have logging capabilities can dump out different levels of detail:
```
kelpie.debug      true  # shortcut for seeing all debug messages

kelpie.log.debug  true  # enable debug, info, and warning messages
kelpie.log.info   true  # enable info and warning messages
kelpie.log.warn   true  # enable warning messages
```

### Top-level components with logging
These top-level components support logging:

```
backburner.debug   true
bootstrap.debug    true
dirman.debug       true
kelpie.debug       true 
lunasa.debug       true
mpisyncstart.debug true
opbox.debug        true
whookie.debug      true
```

## Lower-level components with logging
Several lower-level components also have logging capabilities.

```
backburner.worker         # individual threads doing backburner work
dirman.cache.mine         # stores list of known resources on this nodes
dirman.cache.others       # stores list of known resources on other nodes
dirman.cache.owners       # stores nodes of where to find items
kelpie.compute            # stores list of known compute functions
kelpie.iom                # stores list of known io modules
kelpie.lkv                # manages local key/blob storage
kelpie.op.compute         # state machine for compute Ops
kelpie.op.drop            # state machine for drop Ops
kelpie.op.getbounded      # state machine for want/need known size Ops
kelpie.op.getunbounded    # state machine for want/need unknown size Ops
kelpie.op.list            # state machien for list Ops
kelpie.op.meta            # state machine for meta query Ops
kelpie.op.publish         # state machine for publish Ops
kelpie.pool               # stores list of known pools
lunasa.allocator          # controls memory allocations
```

## Bootstrap Tricks
### Dumping the parsed configuration at Start
You can tell a node to dump out its configuration when bootstrap initializes:

```
bootstrap.show_config_at_init
```

### Delaying Finish
It can be beneficial to use whookie as a means of querying the state of
a running (or halted) simulation. You can either halt or delay shutdown
via bootstrap options. Also, you can display a large "ok" message on a 
successful exit to make it more obvious how the sim ended.

```
bootstrap.status_on_shutdown            true  # Dumps a visible "ok" message
bootstrap.halt_on_shutdown              true  # Halt without exiting
bootstrap.sleep_seconds_before_shutdown 60    # Halt for 60s and then exit
```

## Managing DirMan
### Designating the Root Node and using it
Services that use DirMan generally need to know which node is the root node. If
you want to designate your node as the root and use a file to convey the id,
do the following on the server:

```
dirman.host_root  true
dirman.write_root myroot_file.txt
```
and the following on the other nodes:
```
dirman.root_node.file myroot_file.txt
```

### Use MPISyncStart to setup the first rank as DirMan Root Node
If you're running inside an mpi job, MPISyncStart can automate picking
rank 0 as the dirman root:

```

mpisyncstart.enable  true
dirman.root_node_mpi 0

```

Make sure you remember to add mpisyncstart to bootstraps:

```

faodel::mpisyncstart::bootstrap();
faodel::bootstrap::Start(faodel::Configuration(default_config_string), kelpie::bootstrap);

```

## Defining Resources
### Defining DHT Resources Using MPISyncStart

In test programs you often need a simple way to define pool resources. One
way to do this is to write some startup code that creates pools and then
has nodes join the pool. An easier way to define basic resources is to
use the *mpisyncstart* service to populate dirman. MPISyncStart does some 
communication operations at startup to convert MPI ranks to FAODEL node ids.

This example enables mpisyncstart and then creates two pool resources.

Note: Resources are ONLY written to the dirman node at boot. It is assumed
      that the dirman service will be used to retrieve the info
      
Note: This currently does not work with the static dirman implementation

```

mpisyncstart.enable      true
dirman.type              centralized
dirman.root_node_mpi     0

dirman.resources_mpi[]   local:/EMPIRE/particles ALL
dirman.resources_mpi[]   dht:/EMPIRE/fluid       ALL

```

### Pointing I/O Modules (IOMs) at storage
An I/O module is essentially a driver for exchanging data with a storage
device. Each IOM needs to have a type and a path specified.

```
default.iom.type    PosixIndividualObjects
default.iom.path    ./faodel_data

default.ioms        empire_particles;empire_fields
dirman.resources[]  local:/EMPIRE/particles&iom=empire_particles
dirman.resources[]  local:/EMPIRE/fields&iom=empire_fields

dirman.type         static
```

# Summary of Options (Excluding Logging)

| Property                                | Type                         | Default     | Description                                                                      |
| --------------------------------------- | ---------------------------- | ----------- | -------------------------------------------------------------------------------- |
| backburner.threads                      | integer                      | 1           | Number of worker threads to use                                                  |
| backburner.notification_method          | pipe, polling, sleep_polling | pipe        | Controls how workers are notified of new work                                    |
| backburner.sleep_polling_time           | time (us)                    | 100us       | Set polling delay in sleep_polling mode                                          |
| bootstrap.show_config_at_init           | boolean                      | false       | Display config used at init time                                                 |
| bootstrap.halt_on_shutdown              | boolean                      | false       | Do infinite-wait instead of exit at shutdown                                     |
| bootstrap.status_on_shutdown            | boolean                      | false       | Dump an ok message on successful exit                                            |
| bootstrap.exit_on_errors                | boolean                      | false       | Exit on errors instead of throwing exception                                     |
| bootstrap.sleep_seconds_before_shutdown | int                          | 0           | Delay Finish shutdown for specified seconds                                      |
| mpisyncstart.enable                     | boolean                      | false       | Enable the service                                                               |
| mpisyncstop.enable                      | boolean                      | false       | Perform an mpi barrier before Finish                                             |
| config.additional_files                 | string list                  | ""          | Load additional info for listed files                                            |
| config.purge                            | boolean                      | false       | Removes all tags and reloads current config                                      |
| dirman.type                             | none,static,centralized      | centralized | Static assumes all info is in config, centralized uses a single server           |
| dirman.host_root                        | bool                         | false       | When true, this node is the dirman root node                                     |
| dirman.write_root                       | filename                     | ""          | Instruct root node to write id to file                                           |
| dirman.root_node                        | nodeid                       | ""          | The nodeid of the root node (hex value)                                          |
| dirman.root_node.file                   | filename                     | ""          | Read the supplied file to find the root node                                     |
| dirman.root_node_mpi                    | Rank                         | -1          | Specify rank for dirman root node                                                |
| dirman.resource[]                       | url                          | ""          | Define a static resource in the configuration                                    |
| dirman.resources_mpi[]                  | url node(s)                  | ""          | Specify resource using mpi ranks                                                 |
| kelpie.type                             | standard, nonet              | standard    | Select between the standard networked core, or a debug option without networking |
| lunasa.eager_memory_manager             | tcmalloc, malloc             | tcmalloc    | Select memory allocator used on eager allocations                                |
| lunasa.lazy_memory_manager              | tcmalloc, malloc             | malloc      | Select memory allocator used on lazy allocations                                 |
| lunasa.tcmalloc.min_system_alloc        | size                         | -           | Override tcmalloc's minimum allocation size                                      |
| opbox.type                              | standard                     | standard    | Select the opbox implementation type                                             |
| opbox.enable_timers                     | bool                         | false       | Enable performance timers on Ops and dump timelines on shutdown                  |
| net.transport.name                      | verbs,gni,mpi,sockets        | none        | Network library to use, default is system dependent                              |
| net.log.debug                           | bool                         | false       | When true, output debug messages                                                 |
| net.log.info                            | bool                         | false       | When true, output info messages                                                  |
| net.log.warn                            | bool                         | false       | When true, output warning messages                                               |
| net.log.error                           | bool                         | false       | When true, output error messages                                                 |
| net.log.fatal                           | bool                         | false       | When true, output fatal messages                                                 |
| net.log.filename                        | string                       | none        | When set, direct log messages to file else stdout                                |
| whookie.interfaces                      | string list                  | eth0,lo     | Order of interfaces to get IP from                                               |
| whookie.app_name                        | string                       | Whookie     | Name of the application to display up front                                      |
| whookie.address                         | string                       | 0.0.0.0     | The IP address the app would like to bind to                                     |
| whookie.port                            | integer                      | 1990        | The desired port to use                                                          |
