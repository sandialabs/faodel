
# Debug Options

## Simple Debug Enables
Many FAODEL components support the debug flag. In particular:
```
bootstrap.debug    true
webhook.debug      true
lunasa.debug       true
opbox.debug        true
mpisyncstart.debug true
dirman.debug       true
kelpie.debug       true 
```

## Delaying Exit
It can be beneficial to use webhook as a means of querying the state of
a running (or halted) simulation. You can either halt or delay shutdown
via bootstrap options. Also, you can display a large "ok" message on a 
successful exit to make it more obvious how the sim ended.

```
bootstrap.status_on_shutdown            true

bootstrap.halt_on_shutdown              true
bootstrap.sleep_seconds_before_shutdown 60
```

# Defining DHT Resources Using MPISyncStart

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

# I/O Modules (IOMs)

## Simple IOMs
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
