Whookie: A Library for Remote Debug and Control
===============================================

Whookie is a library that provides a simple, web-based API for
interacting with a running applications. Developers register _hooks_
with Whookie that respond to web requests. Whenever the application
receives an http request for a particular hook, it passes the hook a
list of URL arguments that were supplied in the request and expects
the hook to generate an html response that is shipped back to the
requester. Whookie is used in Faodel for debugging purposes
(eg getting status information about the rank) as well as simple
communication (eg, passing RDMA keys between a pair of nodes during
connect in OpBox).

**Note:** The bulk of the networking code for Whookie comes from an
example provided with Boost. Please see TPL License Information below
for more details.


Bootstrap and Initialization
----------------------------
Whookie has code that will automatically register the service with
Bootstrap to simplify configuring and starting it. In order to start
the Whookie service a user must issue a boostrap::Init() with a proper
Configuration.


Networks, Ports, and nodeid_t
-----------------------------
A nodeid for an application is built from the node's IP address and
the TCP socket that Whookie uses for incoming connections. Since most
HPC systems have multiple NICs, it is often necessary to specify which
NIC should be used for communication. A user may pass address
information in via Configuration by either setting the whookie.address
to a specific value (generally tedious), or by supplying a
comma-separated list of nic names (eg "ib0,eth0,lo") to query for an
address. 

A user may specify the network port Whookie should listen to by
setting the whookie.port value in Configuration. If another
application is currently using the port at the node (eg, when two
whookie applications start on the same node and use the default port
number), Whookie will increment the port number until it finds an
unused value. As such, it should be expected that a service will not
always have the port number that was originally supplied in the
Configuration.

The Faodel nodeid_t is a simple data type that contains both the IP
address and port value for the Whookie server. Once Whookie is
initialized, a user may query it to determine what the nodeid_t is for
this instance. Often, nodes need to exchange nodeid_t values through
out-of-band means (eg mpi broadcast or the opbox dirman service) in
order to connect with each other.

Server: Registering a Hook
--------------------------
A developer can create a new hook simply by registering a lambda for
the hook with Whookie. The simplest example is:

```
whookie::Server::registerHook("/bob", [] (const map<string,string> &args, stringstream &results) {
      html::mkHeader(results, "Bob's Page");
      html::mkTable(results, args, "Bobs args");
      html::mkFooter(results);
    });
```

Whookie passes a map<string,string> of arguments that were embedded in
the http request (eg /bob&item1=apple&item2=orange would have the
k/v pairs item1/apple and item2/orange). Results are passed back
through a stringstream that a user appends with data.

**warning**: Passing binary data back as a result will likely cause
errors at the client code, as the default reader terminates parsing
the result when two empty lines are sent back. Users should encode
their data in a way that protects it from this pitfall. A future
version will provide a binary reader that avoids this problem.


Server: HTML Helpers
--------------------
Hooks that are intended to provide information back to users need html
formatting in order to render properly in a browser. While users are
free to generate responses themselves or with TPL HTML libraries,
Whookie provides a few simple classes for generating html-marked up
data. These commands append formatting to a stingstream the hook is
going to pass back as a result (eg, namespace html contains mkHeader,
mkTable, and mkFooter).

Whookie also provides a class named **ReplyStream** that is useful for
formatting tables of information or standard blocks of text.

Client: General Use
-------------------
Simple client code is provided to make it easy to retrieve data from a
server. A client can request data as follows:

```
string data;
whookie::retrieveData(host, port, path, &data);
```

Issues
======
While generally stable, we have observed the following issues with
Whookie in some instances:

- **Multi-Rank on XC40:** When running multiple Whookie-equipped
    applications on a single node in the Cray XC40, there are
    asynchronous i/o problems that cause crashes. Until this is fixed we
    recommend running one rank per node.
- **Binary Data:** The current client code terminates reading a reply when
    it sees two empty lines. If the hook is sending binary data, it is
    possible that the data may trigger an early finish. The work around
    for this problem is to serialize data into a text-based form with no
    line skips. A future implementation will handle binary data
    transfers. 
- **Single Implementation:** The current version only provides a single
    backend for the server (Boost). Our intention is to implement
    alternate backends to provide other users with other options.
- **Name:** "Whookie" is a name that is widely used for other things. We
    will rename this component in the next release. 


Build and Configuration Settings
================================

Build Dependencies
------------------

Whookie has the following build dependencies:

| Dependency     | Information                         |
| -------------- | ----------------------------------- |
| FAODEL:SBL     | Uses logging capabilities for boost |
| FAODEL:Common  | Uses bootstrap and nodeid_t         |
| Boost          | Uses boost and systems components   |


Compile-Time Options
--------------------

Whookie does not currently implement any flags at compile time.

Run-Time Options
----------------

Whookie examines Configuration for the following flags when it is
initialized:

| Property           | Type        | Default             | Description                                  |
| ------------------ | ----------- | ------------------- | -------------------------------------------- |
| whookie.debug      | boolean     | false               | Display debug messages                       |
| whookie.interfaces | string list | eth0,lo             | Order of interfaces to get IP from           |
| whookie.app_name   | string      | Whookie Application | Name of the application to display up front  |
| whookie.address    | string      | 0.0.0.0             | The IP address the app would like to bind to |
| whookie.port       | integer     | 1990                | The desired port to use                      |


TPL License Information
=======================

This core of this code is based almost entriely on a Boost example
written by Christopher M. Kohlhoff, which is distributed under the
Boost Software License (version 1.0). The original boost asio example
can be found at:

http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/examples/cpp11_examples.html

