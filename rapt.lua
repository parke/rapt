#! /usr/bin/lua5.3


--  Copyright (c) 2020 Parke Bostrom, parke.nexus at gmail.com
--  Distributed under GPLv2 (see end of file) WITHOUT ANY WARRANTY.


--  version  0.0.20200521


function  usage  ()    ------------------------------------------------  usage
  print  [[

usage:  rapt  [ options ]  command  [ package ... ]

  commands
    init
    update
    download
    install
    upgrade
    remove
    purge

  options
    --root  path]]  end


local  lush  =  require 'lush' .import()    ----------------------------  lush
local  g     =  _G


function  apt_conf  ()    ------------------------------------------  apt_conf
  --  20200517  maybe I can remove Debug::NoLocking ??
  --  //  Debug::NoLocking    "true";
  local  apt_conf  =  expand  [[
Dir                     "$root/";
Dir::State::status      "$root/var/lib/dpkg/status";]] .. '\n'
  cat { conf, update=apt_conf, trace=true }  end


function  apt_get  ( args )    --------------------------------------  apt_get
  assert ( type(args) == 'string' )
  local  head  =  root  and  'apt-get  -c $conf  '  or   'apt-get  '
  return  head .. args  end


function  arg_assert  ( t, n )    --------------------------------  arg_assert
  local  rv  =  t [ n ]
  if  rv == nil  then  die ( 'arg not found  %s', n )  end
  return  rv  end


function  args_parse  ( args )    --------------------------------  args_parse

  g.names  =  g.names  or  {}
  g.opts   =  g.opts   or  {}

  local function  opt_set  ( arg )
    local  m  =  arg : match '^--(%S+)'
    if  m  then  g.opts [ m ]  =  true
    else  error ( 'opt_set  bad arg  ' .. arg )  end  end

  local  opts      =  [[  --download-only  ]]
  local  commands  =  [[
    bootstrap  init  install  list  purge  remove  update  upgrade  ]]

  local  n         =  1
  while  n <= # args  do
    local  arg, n1  =  args[n], nil
    if  arg == '--root'  then
      root  =  arg_assert ( args, n+1 )  ;  n1  =  n + 2
    elseif  has ( opts, arg )  then  opt_set ( arg )
    elseif  command  then  table .insert ( names, arg )
    elseif  has ( commands, arg )  then  command  =  arg
    else  print ( 'rapt  bad arg  ' .. arg )  ;  os .exit ( 1 )  end
    n  =  n1  and  n1  or  n + 1  end  end


function  deb_file ( pkg, ver, arch )    ---------------------------  deb_file
  local  rv  =  ('%s_%s_%s.deb') : format ( pkg, ver, arch )
  return  ( rv : gsub ( ':', '%%3a' ) )  end


function  deb_path ( root, pkg, ver, arch )    ---------------------  deb_path
  local  archives  = '/var/cache/apt/archives'
  local  deb_file  =  deb_file ( pkg, ver, arch )
  return  ('%s%s/%s') : format ( root, archives, deb_file )  end


function  die  ( s, ... )    --------------------------------------------  die
  if  select ( '#', ... ) > 0
    then  print ( s : format ( ... ) )
    else  print ( s )  end
  os .exit ( 1 )  end


function  do_configure_again  ( names, stdout )    -------  do_configure_again
  if  stdout == true  then  return  true  end
  assert ( type(names)  == 'table'  )
  assert ( type(stdout) == 'string' )
  local  again, insert  =  false, table .insert
  local  pattern        =  '\n  Package ([^%s:]+)%S* is not configured yet.\n'
  for  name  in  stdout : gmatch ( pattern )  do
    if  not has ( names, name )  then
      insert ( names, name )  ;  again  =  true  end  end
  return  again  end


function  do_configure_chown_error  ( stdout )    ----------------------------
  assert ( type(stdout) == 'string' )
  local  pattern  =
    "\nchown: changing ownership of '[^'\n]*': Invalid argument\n"
  return  stdout : find ( pattern )  end


function  note_names_configured  ( names )    ----------  note_names_configured
  for n, name  in  ipairs ( names )  do
    local  pkg  =  sim_pkgs [ name ]
    if  pkg  then  pkg .is_configured  =  true
    else
      print ( 'note  pkg not found  %s', name )
        for  k  in  pairs ( sim_pkgs )  do  echo '  $k'  end
        os .exit ( 1 )  end  end  end


function  do_configure  ( names )    ---------------------------  do_configure

  local  pkg  =  sim_pkgs [ names ]
  if  pkg  and  pkg .is_configured  then  return  end

  names  =  split ( names )
  local  command, stdout, success  =  nil, true, nil
  while  do_configure_again ( names, stdout )  do
    echo  '\nconfigure  $names'
    command  =  '-' .. dpkg '--configure $names  2>&1'

    --  first attempt
    stdout, success  =  cap ( command )
    if  success  then
      print ( stdout )
      note_names_configured ( names )
      return  end

    --  second attempt
    if  do_configure_chown_error ( stdout )  then
      gids_zero()
      stdout, success  =  cap ( command )
      gids_restore()
      if  success  then
        print ( stdout )
        note_names_configured ( names )
        return  end  end  end

  echo  ''
  echo  'configure  $names  error'
  echo  ( '  command  ' .. command )
  echo  '  stdout'
  print ( stdout )
  os .exit ( 1 )  end


function  do_remove  ( name )    ----------------------------------  do_remove
  echo  '\nremove  $name'
  trace ( dpkg '--remove  $name' )  end


function  do_unpack_ownership_error  ( stdout )    ---  unpack_ownership_error
  assert ( type(stdout) == 'string' )
  local  pattern  =
    "\n error setting ownership of '[^'\n]*': Invalid argument\n"
  return  stdout : find ( pattern )  end


function  do_unpack  ( names )    ------------------------------------  unpack

  echo  '\nunpack  $names'

  for  name  in  each ( split ( names ) )  do

    local  path     =  sim_pkgs [ name ] .path
    local  command  =  '-' .. dpkg '--unpack $path  2>&1'
    local  stdout, success

    --  first attempt
    stdout, success  =  cap ( command )
    if  success  then  print ( stdout )  ;  return  end

    if  do_unpack_ownership_error ( stdout )  then
      --  second attempt
      gids_zero()
      stdout, success  =  cap ( command )
      printf ( 'unpack  %s  second attempt  %s\n', name, success )
      gids_restore()
      if  success  then  print ( stdout )  ;  return  end  end

    echo  ''
    echo  'unpack  $name  error'
    echo  ( '  command  ' .. command )
    echo  '  stdout'
    print ( stdout )
    os .exit ( 3 )  end  end


function  download  ( command, names )    --------------------------  download
  assert ( has ( 'install  upgrade', command ) )
  names  =  split ( names or g.names )
  trace ( apt_get '$command  --download-only  $names' )  end


function  dpkg  ( args )    --------------------------------------------  dpkg
  if  root  then
    --  local  path     =  'PATH=/usr/bin:/usr/sbin:/bin:/sbin'
    --  local  command  =  'lxroot  -r $root  --  %s  /usr/bin/dpkg  '
    --  return  command : format ( path ) .. args
    return      'lxroot  -r  $root  --  /usr/bin/dpkg  ' .. args
  else  return  'lxroot  -r         --  /usr/bin/dpkg  ' .. args  end  end


function  extract  ()    --------------------------------------------  extract
  for  n, act  in  ipairs ( sim_acts )  do
    if  act.op == 'Inst'  then
      local  path  =  sim_pkgs [ act.name ] .path
      print  ''
      trace  'dpkg  --extract  $root$path  $root'
      end  end  end


function  gids_restore  ()    ----------------------------------  gids_restore


  local  etc          =  '$root/etc'
  local  group        =  cat '$etc/group'
  local  group_zero   =  cat '$etc/group-zero'
  local  passwd       =  cat '$etc/passwd'
  local  passwd_zero  =  cat '$etc/passwd-zero'

  if  group == group_zero  and  passwd == passwd_zero  then
    --  if both group and psswd are unchanged, then restoration is simple
    sh  'mv  $etc/group-raw   $etc/group'
    sh  'mv  $etc/passwd-raw  $etc/passwd'
    sh  'rm  $etc/group-zero'
    sh  'rm  $etc/passwd-zero'

  else    --  restoration is more complicated
    die  'gids_restore  case not implemented'  end  end


function  gids_zero  ()    ----------------------------------------  gids_zero

  local  etc  =  '$root/etc'

  assert ( not is '-f $etc/group-raw'   )
  assert ( not is '-f $etc/group-zero'  )
  assert ( not is '-f $etc/passwd-raw'  )
  assert ( not is '-f $etc/passwd-zero' )

  local  group        =  cat '$etc/group'
  local  passwd       =  cat '$etc/passwd'
  local  group_zero   =  group  : gsub ( '(\n%S-:x:)(%d+)(:)',     '%10%3'   )
  local  passwd_zero  =  passwd : gsub ( '(\n%S-:x:)(%d+:%d+)(:)', '%10:0%3' )

  sh  'mv  -n  $etc/group   $etc/group-raw'
  sh  'mv  -n  $etc/passwd  $etc/passwd-raw'

  assert ( not is '-f $etc/group' )
  assert ( not is '-f $etc/passwd' )

  cat { '$etc/group',       write = group_zero  }
  cat { '$etc/group-zero',  write = group_zero  }
  cat { '$etc/passwd',      write = passwd_zero }
  cat { '$etc/passwd-zero', write = passwd_zero }  end



function  init  ()    --------------------------------------------------  init

  if  not root  then  return  end

  sh  'mkdir  -p  $root/dev'
  sh  'mkdir  -p  $root/etc/apt/apt.conf.d'
  sh  'mkdir  -p  $root/etc/apt/preferences.d'
  sh  'mkdir  -p  $root/etc/apt/sources.list.d'
  sh  'mkdir  -p  $root/proc'
  sh  'mkdir  -p  $root/sys'
  sh  'mkdir  -p  $root/tmp'
  sh  'mkdir  -p  $root/usr/local/bin'
  sh  'mkdir  -p  $root/usr/local/share/lua/5.3'
  sh  'mkdir  -p  $root/var/cache/apt/archives/partial'
  sh  'mkdir  -p  $root/var/lib/apt/lists/partial'
  sh  'mkdir  -p  $root/var/lib/dpkg'
  sh  'mkdir  -p  $root/var/lib/dpkg/info'
  sh  'mkdir  -p  $root/var/lib/dpkg/updates'
  sh  'mkdir  -p  $root/var/log'

  sh  'cp     -n  /etc/apt/sources.list   $root/etc/apt/'
  sh  'rsync  -a  /etc/apt/trusted.gpg.d  $root/etc/apt/'
  --sh  'cp     -n  /etc/group              $root/etc/'
  --sh  'cp     -n  /etc/passwd             $root/etc/'
  sh  'cp     -n  /etc/resolv.conf        $root/etc/'
  sh  'cp     -n  lush.lua                $root/usr/local/share/lua/5.3/'
  sh  'cp     -n  bin/lxroot              $root/usr/local/bin/'
  sh  'cp     -n  rapt.lua                $root/usr/local/bin/rapt'

  init_etc_passwd()
  --  sh  'touch      $root/etc/group'
  sh  'touch      $root/etc/gshadow'
  --  sh  'touch      $root/etc/passwd'
  sh  'touch      $root/etc/shadow'
  sh  'touch      $root/var/lib/dpkg/status'
  end


function  init_etc_passwd  ()    ----------------------------  init_etc_passwd
  local  uid    =  cap 'id -u'
  local  gid    =  cap 'id -g'
  local  user   =  cap 'id -un'
  local  group  =  cap 'id -gn'
  if  not is '-f $root/etc/group'  then
    cat { '$root/etc/group', write = expand [[
root:x:0:
mail:x:8:
shadow:x:42:
utmp:x:43:
staff:x:50:
$group:x:$gid
]]  }  end
  if  not is '-f $root/etc/passwd'  then
    cat { '$root/etc/passwd', write = expand [[
root:x:0:0:root:/root:/bin/bash
$user:x:$uid:$gid::/home/$user:/bin/bash
]]  }  end  end


function  simulate  ( command, names, noisy )    -------------------  simulate

  names           =  split ( names )
  local  command2  =  apt_get '$command  --simulate  $names'
  print ( lush .expand_command ( command2 ) )
  local  iter     =  popen ( command2 )

  local  acts, pkgs, insert  =  {}, {}, table .insert
  for  line  in  iter  do

    if  noisy  then  print ( line )  end

    --  local  pattern  =  '^([IC][no][sn][tf]) (%S+) %((%S+) %S+ %[(%w+)%]%)'
    local  pattern  =  '^([IC][no][sn][tf]) (%S+) %((%S+) [^[]*%[(%w+)%]%)'
    local  op, name, ver, arch  =  line : match ( pattern )
    if  op == 'Inst'  or  op == 'Conf'  then
      insert ( acts, { op=op, name=name, ver=ver } )
      if  pkgs [ name ] == nil  then
        local pkg  =  { name=name, ver=ver, arch=arch }
        pkg .path  =  deb_path ( '', name, ver, arch )
        insert ( pkgs, pkg )  ;  pkgs [ name ]  =  pkg  end  end
    local  pattern        =  '^(Remv) (%S+) %[(.-)%]'
    local  op, name, ver  =  line : match ( pattern )
    if  op == 'Remv'  then
      insert ( acts, { op=op, name=name, ver=ver } )  end  end

  g .sim_acts  =  acts
  g .sim_pkgs  =  pkgs  end


function  simulate_run  ( command, names )    ------------------  simulate_run
  if  opts .download_only  then  return  end
  simulate ( command, names )
  for  n, act  in  ipairs ( sim_acts )  do
    if      act .op == 'Inst'  then  do_unpack    ( act .name )
    elseif  act .op == 'Conf'  then  do_configure ( act .name )
    elseif  act .op == 'Remv'  then  do_remove    ( act. name )
    else    die ( 'bad action  ' .. act. op )  end  end  end


function  simulate_print ( command, names )    ---------------  simulatu_print
  simulate ( command, names, 'noisy' )
  for  n, act  in  ipairs ( sim_acts or {} )  do
    printf ( '  %s  %s\n', act.op, act.name )  end  end


--  commands  ------------------------------------------------------  commands


function  update  ()    ----------------------------------------------  update
  trace ( apt_get 'update' )  end




function  bootstrap  ()    ----------------------------------------  bootstrap

  --  20200519
  --
  --  dpkg --unpack <any>  requires:
  --    diff      ->  /usr/bin/diff   ->  diffutils
  --    ldconfig  ->  /sbin/ldconfig  ->  libc-bin
  --    rm        ->  /bin/rm         ->  coreutils
  --    sh        ->  /bin/sh         ->  dash
  --
  --  dpkg --unpack libc  requires:
  --    sed       ->  /bin/sed        ->  sed
  --
  --  dpkg --configure dash  requires
  --    awk       ->  /usr/bin/awk    ->  mawk (must be configured!)
  --
  --  dpkg --configure passwd  requires
  --    grep      ->  /bin/grep       ->  grep
  --
  --  apt-get update  requires  ( without find yields gpg errors !!! )
  --    find      ->  /usr/bin/find   ->  findutils
  --
  --
  --  note:  dash requires debconf.  debconf recommends apt-utils.
  --
  --  note:  an obsolete(?) note said (incorrectly?) that libstdc++6
  --         and ca-certificates both require findutils.
  --
  --  note:  an obsolete(?) note said (incorrectly?) that libssl1
  --         requires sysvinit-utils.
  --
  --  idea:  perhaps we could use busybox-static instead?
  --
  --  note:  I also enjoy:  bash  less  ncurses-base  ncurses-bin

  local  phase1  =  'lua5.3  mawk  '
  local  phase2  =  'dpkg  coreutils  dash  diffutils  findutils  grep  ' ..
                    'libc-bin  sed  '

  if  true  then    --  can be set to false for testing during development
    update()
    download    ( 'install', phase1 .. phase2 )
    simulate    ( 'install', phase1 .. phase2 )
    extract()  end
  simulate_run  ( 'install', phase1 )
  simulate_run  ( 'install', phase2 )
  end


function  install  ()    --------------------------------------------  install
  download     ( 'install' )
  simulate_run ( 'install', names )  end


function  list  ()    --------------------------------------------------  list
  trace ( dpkg '--no-pager  --list' )  end


function  upgrade  ()    --------------------------------------------  upgrade
  download ( 'upgrade' )
  simulate ( 'upgrade', names )  end


function  remove  ()    ----------------------------------------------  remove
  simulate_run ( 'remove', names )  end


function  purge  ()    ------------------------------------------------  purge
  simulate_print ( 'purge', names )  end


function  main  ( args )    --------------------------------------------  main

  args_parse ( args )

  if  root  then
    g .conf  =  root .. '/tmp/apt.conf'
    init()
    apt_conf()  end

  if  command  then
    if  g [ command ]  then  g [ command ] ()
    else  die ( 'bad command  %s', command )  end
  else  usage()  end  end


main ( arg )




--    rapt.lua  -  a rootless Debian-style package manager
--
--    Copyright (c) 2020 Parke Bostrom, parke.nexus at gmail.com
--
--    This program is free software; you can redistribute it and/or
--    modify it under the terms of version 2 of the GNU General Public
--    License as published by the Free Software Foundation.
--
--    This program is distributed in the hope that it will be useful,
--    but WITHOUT ANY WARRANTY; without even the implied warranty of
--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--    GNU General Public License for more details.
--
--    You should have received a copy of the GNU General Public
--    License along with this program; if not, write to the Free
--    Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
--    Boston, MA 02110-1301 USA.
