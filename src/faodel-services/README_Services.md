FAODEL-Services: Simple Services for FAODEL
===========================================

FAODEL provides a few optional services that can be launched during the
bootstrap process:


BackBurner
----------
Many communication services need a simple way to queue up tasks that need
to happen at a later point in time (e.g., notifying clients a new object
is available, implementing a timeout check, queueing communication 
operations to avoid deadlock). BackBurner is a generic progress thread
developers can use to queue up work. BackBurner provides a fifo queue
for handing off operations and can be configured to use multiple
threads (each with its own work queue). The BackBurner worker thread
pulls a bundle of queue operations out at a time and then processes
each one in order.

BackBurner parses the Configuration object for the following settings:

| Property                | Type        | Default | Description                          |
| ------------------      | ----------- | ------- | ------------------------------------ |
| backburner.debug        | boolean     | false   | Display debug messages               |
| backburner.threads      | integer     | 1       | Number of worker threads to use      |



MPISyncStart
------------
FAODEL applications often need a simple way to pass initial configuration 
information between nodes. The MPISyncStart service examines a configuration
and updates certain properties (ending with "_mpi") with runtime information.

MPISyncStart parses the Configuration object for the following settings:

| Property                | Type        | Default | Description                          |
| ------------------      | ----------- | ------- | ------------------------------------ |
| mpisyncstart.debug      | boolean     | false   | Display debug messages               |
| mpisyncstart.enable     | boolean     | false   | Enable the service                   |
| dirman.root_node_mpi    | Rank        | -1      | Specify rank for dirman root node    |
| dirman.resources_mpi<>  | url node    | ""      | Specify resource using mpi ranks     |

