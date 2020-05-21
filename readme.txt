
rapt is an experimental rootless package manager for .deb packages.

rapt allows a non-root user to create a Debian or Ubuntu software
environment in a specified directory.

The goal of rapt is to allow non-root users to:

  -  install, build, and run .deb packages
  -  create clean, controlled, and isolated environments for compiling
       and running software
  -  run legacy Debian/Ubuntu software on a modern host system

rapt is lightweight.  As such, rapt is not intended to compete with
Snap, Docker, or any Linux container system that requires root access.

rapt depends on Lua 5.3 (and possibly also on other common packages).


----  rapt's components

rapt consists of 3 files.

1)  rapt.lua is a wrapper around apt-get and dpkg.

2)  lush.lua is a pure Lua library for writing "shell script" style
      progrems in Lua.  rapt.lua uses lush.lua.

3)  lxroot is a program for creating and entering transient Linux user
      namespaces.  lxroot is similar to chroot.  However, lxroot does
      not require root access.  lxroot is written in C++.


----  rapt's architecture

rapt is a wrapper around apt-get and dpkg.  In order to understand
rapt's architecture, it helps to understand apt-get and dpkg.

apt-get
  -  downloads list of packages
  -  analyzes dependiences between those packages
  -  downloads the packages
  -  runs dpkg to install (or remove) the packages
  -  apt-get fails when run as root in a user namespace

dpkg
  -  installs and configures packages
  -  must run as root (or as simulated root in a user namespace)

rapt runs apt-get as non-root to:
  -  downoald packages
  -  provide installation instructions

rapt then:
  -  interprets the instructions provided by apt-get
  -  runs dpkg in a Linux user namespace to actually install the
       packages.

Some .deb packages require that certain files be owned by a certain
user or group.  However, the Linux kernel prohibits ownership changes
inside a user namespace.  rapt works around this limitation by, when
needed, temporarily editing /etc/group and /etc/passwd (inside the
user namespace) to set all uids and gids to zero.


----  demonstration

This demonstration was developed and tested on Ubuntu 20.04.

$  git  clone  https://github.com/parke/rapt.git
$  cd  rapt
$  make  rapt-demo

The above command will:

  -  Compile bin/lxroot.
  -  Bootstrap a Debian/Ubuntu environment, by default in ~/rapt-demo.
  -  Install a few convenience packages in the environment.
  -  Copy rapt's files into ~/rapt in the environment.
  -  Run an interactive bash shell inside the environment.

If you are so inclined, you can create a second environment nested
inside of the first environment.  To do this, simply run:

$  make  rapt-demo

Wait for interactive bash shell to appear, then run:

$  cd  rapt
$  make  rapt-demo

Creating a single software environment will use 315+ MB of disk space.
Creating a second, nested software environment will use an additional
315+ MB of disk space.

Examine the Makefile to see the specific commands.  To view help/usage
information, simply run either rapt.lua or lxroot without arguments.

Happy rapting!
