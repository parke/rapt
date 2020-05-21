

root=$(HOME)/rapt-demo


define  help_txt

usage:  make  rapt-demo    -    build a Debain environment and enter it
        make  lxroot       -    enter the already built Debian environment

        note:  the Debian envionment will be built in:
                 $(root)
               the Debian environment may use 315+ MB of disk space.
endef
export  help_txt


#  rapt needs bin/lxroot to be in the PATH
PATH:=$(PATH):$(PWD)/bin


help:
	@  echo  "$$help_txt"


rapt-demo:  rhome=$(root)$(HOME)
rapt-demo:  pkgs=bash  less  make  ncurses-base  ncurses-bin  rsync

rapt-demo:  bin/lxroot

	@  #  bootstrap the Debian environment
	@  echo
	./rapt.lua  --root $(root)  bootstrap

	@  #  install some convenience packages
	@  echo
	./rapt.lua  --root $(root)  install  $(pkgs)

	@  #  install rapt files to allow recursive nesting
	@  echo
	mkdir  -p  $(rhome)/rapt/bin
	cp  Makefile  lush.lua  lxroot.c  rapt.lua  $(rhome)/rapt
	cp  bin/lxroot  $(rhome)/rapt/bin

	@  #  enter the Debian environment
	@  echo
	bin/lxroot  -n  $(root)


lxroot:  bin/lxroot
	bin/lxroot  -n  $(root)


bin/lxroot:  Makefile  lxroot.c  bin
	g++  -g  -Wall  -Werror  lxroot.c  -o $@


bin:
	mkdir  $@
