Docker Images for FAODEL Development
====================================

**!!EXPERIMENTAL!!** This directory contains new docker work that is only
a preview. It has not been extensively tested and is provided for 
information purposes only. 




This project builds a docker container that holds a compiler and TPLs that
are needed for building FAODEL. It installs LLNL's SPACK tool in order to
build each component. The project uses a series of containers in order to
break the build up into different stages so the base containers can be used
elsewhere. The current containers are:

1. faodel-spack-base: This container adds a few tools to the stock centos7 
   container and then installs spack into /spack. Spack uses the OS's gcc4
   to build gcc8 and then uses gcc8 to build gcc8 (the intermediate 
   gcc4-built libs are then removed to save space and focus on a single 
   environment).

2. faodel-spack-tools: This container uses spack to build several common 
   developer tools, such as cmake, doxygen, git, and ninja.

3. faodel-spack-tpls: This container builds several tpls that are used in
   FAODEL, including boost, cereal, googletest, libfabric, and openmpi.


The spack installation is in `/spack`. To make it easier to have all the 
tools loaded in one place, we have also created a spack view in `/install`.
This directory is a symlink farm that makes it so we only have to add
`/install/bin` to the path to get access to all the tools. 


Updating Settings
-----------------
Most of the build settings for the docker containers is stored inside the
first container (`faodel-spack-base`). If you're building this yourself,
things you might want to change are:

- **GCC_VERSION**: You can specify the version of GCC you want to use in
  Spack notation (eg gcc@8.2.0). Switching versions has not been tested.
- **Proxy**: We normally build behind a proxy. In addition to updating
  the host's docker proxy settings, we plug the proxy into the environment
  for the base docker container. 
- **SPACK_ROOT**: This setting determines where the spack installation gets
  placed in the image. We typically use `/spack` to make it easy to locate.
  However, you might want to place it somewhere else (eg `/home/user/spack`)
  if you intend to transplant the install into a non-container filesystem.
- **SPACK_INSTALL**: While Spack or environment modules provide a way to
  modify your path, we often just want all the installs to live in a single
  place so we can modify our path variables once. We use a `Spack view` to
  create a link farm for all the bins and libs of the tools. The 
  `SPACK_INSTALL` variable sets where this link farm lives (eg, `/install`). 
  You may want to change this for the same reasons as modifying SPACK_ROOT.
- **SPACK_INSECURE**: By default, spack will do https transfers via curl
  and abort if the ssl/tls information isn't correct.  If you set 
  `SPACK_INSECURE` to `TRUE`, it will configure Spack to disable ssl 
  verification. This is necessary in situations where you're behind a corporate
  gateway that intercepts https traffic and messes with certificates.
- **SPACK_MIRROR**: Spack can use a source package mirror to improve build
  times. If you've created a mirror, set the web address here.


Using an Image
--------------
Once you have an image, you can run it in docker. The simplest thing to do is
just try out the image:

```
$ docker run -it faodel-spack-tpls bash
[root@0f3df67c19d4 /]# spack find
[root@0f3df67c19d4 /]# gcc --version
[root@0f3df67c19d4 /]# ls /install
[root@0f3df67c19d4 /]# pkg-config --cflags libfabric
[root@0f3df67c19d4 /]# spack install htop
[root@0f3df67c19d4 /]# spack load htop
```

The spack installation should be functional in the image, so you can install
additional tools as needed. 

Note: anything installed in a container disappears after the container is closed, 
      so remember to update the scripts that build the container.

Copying From the Container
--------------------------
The simplest thing that can be done with the image is just copy all the
files out of it to your local machine.

```
$ docker run faodel-spack-tpls
$ docker ps -a | head -n 2  # get the container id.. eg 6bef61e13d27
$ docker cp 6bef61e13d27:/install /install
$ docker cp 6bef61e13d27:/spack   /spack
```


Working with Gitlab
-------------------
One of the reasons why you might want to build the dependencies in containers
is that you can then push them up to a docker image repo and then pull them
down to run on different systems. Gitlab provides a built-in container 
repository that makes it easy to share images with users. 

Gitlab's project registry page has more information about how to push/pull
docker images. To summarize, tag your images with the gitlab project url,
use docker's login command to authenticate, and then push/pull the last
image that you want to use. eg:


```
docker login gitlab.mycompany.com:3434
docker pull gitlab.mycompany.com:3434/faodel/faodel/faodel-spack-tpls:latest
```


If you do update the containers, you can push them up to the registry via:

```
docker login gitlab.mycompany.com:3434
docker push gitlab.mycompany.com:3434/faodel/faodel/faodel-spack-tpls:latest
```

Future Work
-----------
The current work is a crude implementation that is intended to illustrate
how the software stack can be built with Spack. We intend to smooth this
work out in later releases and use Compose to simplify the builds.



