# NNTI: The Nessie Network Transport Interface

The Nessie Network Transport Interface (NNTI) provides a common API 
for HPC interconnects.

## External Libraries

NNTI requires the SBL library which can be found at https://gitlab.sandia.gov/nessie-dev/sbl.

## Example Configure Script

An example do-configure script can be found in ${NNTI_SRC}/examples/scripts.

## Interjob Communication

Interjob communication is the ability of two or more jobs to 
communicate.  The jobs would typically exist in different MPI 
communicators and therefore unable to send and receive data.  Using 
low-level interconnect libraries, NNTI allows jobs to bypass this 
restriction.  This next secions describe interjob communication 
for each of the NNTI transports.

### Infiniband (ibverbs)

The ibverbs transport doesn't require any special configuration to 
enable interjob communication.  As long as a route exists between the 
two machines where the processes are running, any two processes can 
communicate using send or RDMA.


### Cray UGNI

In order for two processes to communicate using libugni, their 
Communication Domains must be created with the same protection tag (ptag) 
and cookie.  The default behavior is to have a system-wide unique ptag 
and cookie assigned to all process in a job when it is launched and those 
processes can only use that ptag/cookie pair.  This prevents accidental 
or malicious corruption or theft of data by another job.  To support the 
cooperative jobs use-case, Cray introduced Dynamic RDMA Credentials 
(DRC).  DRC allows an application to use a ptag/cookie pair other than 
the one assigned at launch.  The DRC must be created and permissions 
granted by an authorized admin.  Any process that uses the ptag/cookie 
pair associated with the DRC can communicate as if they are were in the 
same job.

To use DRC with NNTI, first contact the target machine's admin team to 
acquire a DRC.  The admins will send back a DRC ID.  Then add a fragment 
like this to each job's Configuration:

```
nnti.transport.credential_id  <DRC_ID>
```


### MPI

The MPI transport does not support interjob communication.

