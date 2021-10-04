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

BackBurner has a few mechanisms for controlling how workers are notified of
new work:
- **Pipe** (default): A pipe is used to pass notifications of new work to the
  workers. While this increases latencies, it allows the kernel to make a
  decision about when to schedule the worker thread.
- **Polling**: Workers continuously check their queue sizes. This method 
  improve latencies, but it wastes a tremendous amount of CPU time.
- **Sleep Polling**: Workers do a sleep operation before they poll for new
  work. Users can adjust the polling rate by changing the sleep time. Given
  that a worker still processes all the entries that are available when it
  wakes up, this approach can be useful for batching up work.


BackBurner parses the Configuration object for the following settings:

| Property                       | Type                         | Default | Description                                   |
| ------------------------------ | ---------------------------- | ------- | --------------------------------------------- |
| backburner.debug               | boolean                      | false   | Display debug messages                        |
| backburner.threads             | integer                      | 1       | Number of worker threads to use               |
| backburner.notification_method | pipe, polling, sleep_polling | pipe    | Controls how workers are notified of new work |
| backburner.sleep_polling_time  | time (us)                    | 100us   | Set polling delay in sleep_polling mode       |

MPISyncStart
------------
FAODEL applications often need a simple way to pass initial configuration 
information between nodes. The MPISyncStart service examines a configuration
and updates certain properties (ending with "_mpi") with runtime information.

For properties that allow a user to specify a list of nodes (eg
`dirman.resources_mpi`), the parser supports a few ways to build node lists:

- Rank IDs: The actual rank numbers, such as `0` or `0,1,2` or `0-3`
- "middle": the middle rank. eg in an 8 rank job, `0-middle` would be the first half
- "end": the last rank in the job. eg in an 8 rank job, `end` would be 7.
- "all": all of the ranks, eg in an 8 rank job `all` would be 0-7.

MPISyncStart parses the Configuration object for the following settings:

| Property                | Type        | Default | Description                          |
| ------------------      | ----------- | ------- | ------------------------------------ |
| mpisyncstart.debug      | boolean     | false   | Display debug messages               |
| mpisyncstart.enable     | boolean     | false   | Enable the service                   |
| dirman.root_node_mpi    | Rank        | -1      | Specify rank for dirman root node    |
| dirman.resources_mpi[]  | url node(s) | ""      | Specify resource using mpi ranks     |

Note: Bootstrap has an `mpisyncstop.enable` option that performs a barrier on shutdown.

