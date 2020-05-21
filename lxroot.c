

//  -*- mode: c++ -*-


//  Copyright (c) 2020 Parke Bostrom, parke.nexus at gmail.com
//  Distributed under GPLv2 (see end of file) WITHOUT ANY WARRANTY.


#define  LXROOT_VERSION  "0.0.20200521.xxxx"


//  compile with:  g++  -Wall  -Werror  lxroot.c  -o lxroot


#include  <dirent.h>
#include  <errno.h>
#include  <fcntl.h>
#include  <limits.h>
#include  <pwd.h>
#include  <sched.h>
#include  <signal.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
#include  <sys/mount.h>
#include  <sys/stat.h>
#include  <sys/syscall.h>
#include  <sys/types.h>
#include  <sys/wait.h>


#include  <map>
#include  <string>
#include  <vector>


//  macro  printe  --------------------------------------------  macro  printe
#define  printe( ... )  fprintf ( stderr, __VA_ARGS__ );


void  usage  ()  {    //  ---------------------------------------------  usage
  printe (
"\n"
"usage:  lxroot  [-nprx]  [[.]/path]  [profile]  [ -- [env] command ]\n"
"\n"
"  -n     provide network access (via CLONE_NEWNET == 0)\n"
"  -p     provide access to pulseaudio (may only work on Ubuntu)\n"
"  -r     simulate root user (map uid and gid to zero)\n"
"  -x     provide X11 access (mount --bind /tmp/.X11-unix)\n"
"  env    name=value ...\n"

	  );  }


//  macro  die  --------------------------------------------------  macro  die
//  see  https://stackoverflow.com/q/5588855   regarding ##__VA_ARGS__
//  see  https://stackoverflow.com/a/11172679  regarding ##__VA_ARGS__
#define  die_pe( format, ... )  {					\
  fprintf ( stderr, "lxroot  error  " format "  ", ##__VA_ARGS__ );	\
  perror(NULL);								\
  exit ( 1 );  }


//  macro  die2  ------------------------------------------------  macro  die2
#define  die2( format, ... )  {						\
  fprintf ( stderr, "lxroot  error  " format "\n", ##__VA_ARGS__ );	\
  exit ( 1 );  }


template < typename T >    //  ----------------------------  template  assert2
T  assert2  ( T v, const char * file, const int line )  {
  if  ( (bool) v == false )  {
    printe ( "assert  failed  %s  %d\n", file, line );  abort();  }
  return  v;  }


//  macro  assert  --------------------------------------------  macro  assert
#define  assert( v )  assert2 ( v, __FILE__, __LINE__ )


void  print  ( const char * const * p )  {    //  -------------  argv :: print
  if  ( p )  {  printe ( "%s", *p );  p++;  }
  for  (  ;  p && *p;  p++  )  {  printe ( "  %s", *p );  }
  printe ( "\n" );  }


//  struct  str  ------------------------------------------------  struct  str

struct   mstr;                           //  mstr  =  mutable   string
typedef  const mstr  str;                //  str   =  immutable string
typedef  std :: vector < mstr >  vec;    //  vec   =  std :: vector < mstr >

struct  mstr  {

  const char *  s;
  int           n;

  mstr  ()  :  s ( nullptr ), n ( 0 )  {}

  mstr  ( const char * const src )  :
    s ( src ? strdup ( src ) : nullptr ),
    n ( src ? strlen ( src ) : 0 )  {}

  mstr  ( const char * const src, const int len )  :
    s ( src ? strndup ( src, len ) : nullptr ),
    n ( src ? strlen ( s ) : 0 )  {}

  mstr  ( mstr & a )  :
    s ( a.s ? strndup ( a.s, a.n ) : nullptr ),
    n ( a.s ? strlen ( s ) : 0 )  {}

  mstr  ( str & a )  :
    s ( a.s ? strndup ( a.s, a.n ) : nullptr ),
    n ( a.s ? strlen ( s ) : 0 )  {}

  mstr  ( str & a, str & b )  :
    s ( concat ( a, b ) ),
    n ( a.n + b.n )  {}

  ~mstr  ()  {
    if  ( s )  {  free ( (void*) s );  }  }

  mstr &  operator =  ( str & a )  {
    if  ( s )  {  free ( (void*) s );  s  =  nullptr;  n  =  0;  }
    s  =  a.s  ?  strndup ( a.s, a.n )  :  nullptr  ;
    n  =  a.s  ?  strlen ( s )  :  0  ;
    return  * this;  }

  bool  operator ==  ( const char * b )  const  {
    if  ( s == b )  {  return  true;  }
    if  ( s == nullptr  ||  b == nullptr )  {  return  false;  }
    return  strcmp ( s, b ) == 0;  }

  bool  operator !=  ( const char * b )  const  {
    return  not ( operator == ( b ) );  }

  bool  operator <  ( const char * b )  const  {
    if  ( s == b )  {  return  false;  }
    if  ( s == nullptr )  {  return  true;   }
    if  ( b == nullptr )  {  return  false;  }
    return  strcmp ( s, b ) < 0;  }

  operator const char *  ()  const  {  return  s;  }

  mstr  operator +  ( str & b )  const  {  return  mstr ( * this, b );  }


  const char *  begin  ()  const  {    //  --------------------  mstr :: begin
    return  s;  }


  static mstr  claim  ( const char * unowned )  {    //  ------  mstr :: claim
    mstr  rv;
    rv.s  =  unowned;
    rv.n  =  rv.s  ?  strlen ( rv.s )  :  0  ;
    return  rv;  }


  vec  descend  ()  const  {    //  -------------------------  mstr :: descend
    vec  rv;
    const char *  p  =  s;
    while  (  p  &&  *p  )  {
      while  (  *p == '/'  )  {  p ++;  }
      while  (  *p  &&  *p != '/'  )  {  p ++;  }
      str  path  ( s, p - s );
      rv .push_back ( path );  }
    return  rv;  }


  const char *  end  ()  const  {    //  ------------------------  mstr :: end
    return  s + n;  }


  mstr  head  ( const char * sep, int start = 0 )  const  {    //  -----  head
    if  ( s == nullptr )  {  return  nullptr;  }
    int  min  =  start < n  ?  start  :  n  ;
    const char *  found  =  strstr ( s + min, sep );
    if  ( found )  {  return  mstr ( s, found - s );  }
    return  * this;  }


  const char *  index  ( const int c )  const  {    //  -------  mstr :: index
    return  :: index ( s, c );  }


  int  spn  ( const char * accept, int start = 0 )  const  {    //  ------ spn
    int  min  =  start < n  ?  start  :  n  ;
    return  min + strspn ( s + min, accept );  }


  template < typename... T >    //  -------------------------  mstr :: sprintf
  void  snprintf  ( const char * format, size_t lim, T... args )  {
    char    buf_s[lim];  :: snprintf ( buf_s, lim, format, args... );
    size_t  buf_n  =  strlen ( buf_s );
    char *  new_s  =  (char*) malloc ( n + buf_n + 1 );
    new_s[0]  =  '\0';
    strncat ( new_s, s, n );
    strncat ( new_s, buf_s, buf_n);
    if  ( s )  {  free ( (void*) s );  }
    s  =  new_s;
    n  =  strlen ( s );  }


  bool  startswith  ( str & expect )  const  {    //  ----  mstr :: starstwith
    return  strncmp ( s, expect.s, expect.n ) == 0;  }


  const char *  find  ( const char * needle )  const  {    //  -  mstr :: find
    return  s  ?  strstr ( s, needle )  :  nullptr  ;  }


  mstr  sub  ( int len )  const  {    //  -----------------------  mstr :: sub
    if  ( len <= n )  {  return  s + len;  }
    return  nullptr;  }


  mstr  tail  ( const char * sep )  const  {    //  ------------  mstr :: tail
    if  ( s == nullptr )  {  return  nullptr;  }
    const char *  found  =  strstr ( s, sep );
    if  ( found )  {  return  found + strlen ( sep );  }
    return  nullptr;  }


  vec  tokens  ()  const  {    //  ---------------------------  mstr :: tokens
    vec  rv;
    const char *  p  =  s;
    while  (  p  &&  * p  )  {
      while  (  p  &&  * p == ' '  )  {  p ++;  }
      const char *  p2  =  p;
      while  (  p2  &&  * p2  &&  * p2 != ' '  )  {  p2 ++;  }
      if  ( p2 > p )  {
	mstr  token ( p, p2 - p );
	rv .push_back ( token );
	p  =  p2;  }  }
    return  rv;  }


private:


  const char *  concat  ( str & a, str & b )  {    //  ---------  mstr :: cat
    char *  buf  =  (char*)  malloc ( a.n + b.n + 1 );
    buf[0]  =  '\0';
    strncat ( buf, a.s, a.n );
    strncat ( buf, b.s, b.n );
    return  buf;  }


};    //  end  struct  mstr


str  nullstr  =  {  nullptr,  0  };    //  --------------------------  nullstr


mstr  operator +  ( const char * a, str & b )  {    //  ----------------  op +
  return  mstr ( a, b );  }


//  end  struct  str  ---------------------------------------  end  strct  str




struct  Lib  {    //  -------------------------------------------  struct  Lib


  static void  assert_is_dir    //  --------------------  Lib :: assert_is_dir
  ( const char * const path, const char * const m )  {
    if  ( is_dir ( path ) )  {  return;  }
    printe ( "lxroot  %s  directory not found  %s\n", m, path  );
    exit ( 1 );  }


  static bool  eq  ( const char * a, const char * b )  {    //  ---  Lib :: eq
    if  ( a == NULL  ||  b == NULL )  return  false;
    return  strcmp ( a, b ) == 0;  }


  static str  getcwd  ()  {    //  ----------------------------  Lib :: getcwd
    return  str :: claim ( get_current_dir_name() );  }


  static str  home  ()  {    //  --------------------------------  Lib :: home
    return  getenv ( "HOME" );  }


  static bool  is_dir  ( const char * path )  {    //  --------  Lib :: is_dir
    struct stat  st;
    if  (  path  &&  stat ( path, & st ) == 0  &&  st .st_mode & S_IFDIR  )  {
      return  1;  }
    errno  =  ENOENT;
    return  false;  }


  static bool  is_empty_dir  ( const char * path )  {    //  ---  is_empty_dir
    if  ( not is_dir ( path ) )  {  return  false;  }
    DIR * dirp  =  assert ( opendir ( path ) );
    for  (  struct dirent * p;  ( p = readdir ( dirp ) );  )  {
      const char *  s  =  p -> d_name;
      if  (  eq(s,".")  ||  eq(s,"..")  )  {  continue;  }
      closedir ( dirp );  return  false;  }
    closedir ( dirp );  return  true;  }


  static bool  is_file  ( const char * path )  {    //  ------  Lib :: is_file
    struct stat  st;
    if  (  path  &&  stat ( path, & st ) == 0  &&  st .st_mode & S_IFREG  )  {
      return  true;  }
    errno  =  ENOENT;
    return  false;  }


  static bool  is_link  ( const char * path )  {    //  ------  Lib :: is_link
    struct stat  st;
    if  (  path  &&  lstat ( path, & st ) == 0  &&  st .st_mode & S_IFLNK  )  {
      return  1;  }
    errno  =  ENOENT;
    return  0;  }


  static str  readlink  ( str path )  {    //  --------------  Lib :: readlink
    struct stat  st;
    if  (  lstat ( path, & st ) == 0  &&  st .st_mode & S_IFLNK  )  {
      ssize_t  lim  =  st .st_size + 2;
      char *   buf  =  (char*)  malloc ( lim );
      if  ( buf )  {
	memset ( buf, '\0', lim );
	ssize_t  len  =  :: readlink ( path, buf, lim );
	if  ( len == lim - 2 )  {
	  str  rv  =  buf;
	  free ( buf );
	  return  rv;  }  }
      printe ( "lxroot  readlink  failed  %s\n", path.s );
      exit ( 1 );  }
    return  nullptr;  }


  static str  realpath  ( const char * const path )  {    //  ------  realpath
    return  str :: claim ( :: realpath ( path, nullptr ) );  }


  static mstr  which  ( str & program )  {    //  --------------  Lib :: which
    mstr  s  =  getenv ( "PATH" );
    while  ( s )  {

      /*  20200416
      printf ( "\n" );
      printf ( "which  path  >%s<\n", s.s );
      mstr  a  =  s.head(":");
      printf ( "which  a     >%s<\n", a.s );
      mstr  b  =  "/";
      printf ( "which  b     >%s<\n", b.s );
      mstr  c  =  a + b;
      printf ( "which  c     >%s<\n", c.s );
      */

    mstr  path  =  s.head(":") + "/" + program;

    //  printf ( "which  head  >%s<\n", path.s );

    struct stat  st;
    if  (  stat ( path, & st ) == 0
	   &&  S_ISREG ( st .st_mode )
	   &&  access ( path, X_OK ) == 0  )  {  return  path;  }

    //  printf ( "which  tail  1  %s\n", s.s );
    s  =  s.tail(":");
    //  printf ( "which  tail  2  %s\n", s.s );

    ;;;  }
  return  NULL;  }


};    //  end  struct  Lib  --------------------------------  end  struct  Lib




class  Tokens  {    //  ---------------------------------------  class  Tokens

  vec     t;
  size_t  n  =  0;

public:

  Tokens  ( const char * const * argv, int skip = 0 )  {
    while  (  argv  &&  * argv  &&  skip-- > 0  )  {  argv++;  }
    read_from ( argv );  }


  Tokens  ( str & path )  {  read_from ( path );  }


  operator  bool  ()  const  {    //  ------------------------  Tokens :: bool
    return  n < t.size();  }


  void  bad_token  ( const char * m )  {    //  ---------  Tokens :: bad_token
    printe ( "tokens  bad token  %s  %lu  %lu  >%s<\n",
	     m, n, t.size(), s()  );
    exit ( 1 );  }


  const char *  index  ( const int c )  const  {    //  -----  Tokens :: index
    return  peek() .index ( c );  }


  bool  is  ( const char * expect )  {    //  ------------------  Tokens :: is
    if  (  n < t.size()  &&  t.at(n) == expect  )  {
	//  printe ( "tokens  is    %s\n", t.at(n).s );
	n ++;  return  true;  }
    return  false;  }


  bool  more  ()  const  {    //  ----------------------------  Tokens :: more
    return  n < t.size();  }


  str  next  ()  {    //  ------------------------------------  Tokens :: next
    if  ( n < t.size() )  {
      //  printe ( "tokens  next  %s\n", t.at(n).s );
      return  t.at(n++);  }
    return  nullptr;  }


  str &  peek  ()  const  {    //  ---------------------------  Tokens :: peek
    return  n < t.size()  ?  t.at(n)  :  nullstr  ;  }


  bool  peek  ( const char * expect )  const  {    //  -------  Tokens :: peek
    return  peek() == expect;  }


  void  read_from  ( const char * const * argv )  {    //  ---------  readfrom
    for  (  ;  * argv;  argv ++  )  {  t .push_back ( * argv );  }  }


  void  read_from  ( const char * path )  {    //  ------  Tokens :: read_from
    FILE *  f  =  fopen ( path, "r" );
    if  ( f == nullptr )  {  die_pe ( "read_from  %s", path );  }
    mstr    s;
    while  ( s  =  token_read ( f )  )  {  t .push_back ( s );  }
    assert ( fclose ( f ) == 0 );  }


  const char *  s  ()  const  {    //  --------------------------  Tokens :: s
    if  ( n < t.size() )  {  return  t.at(n).s;  }
    return  nullptr;  }


  size_t  spn  ( const char * accept )  {    //  --------------  Tokens :: spn
    return  peek() .spn ( accept );  }


  str  token_read  ( FILE * f )  {    //  ----------------  mstr :: token_read

    int  c;
    auto  next  =  [&]  ()  {  c  =  fgetc ( f );  return  c != EOF;  };
    auto  undo  =  [&]  ()  {  ungetc ( c, f );  };

    do  {  next();  }  while  (  c == ' '  ||  c == '\n'  ||  c == '\t'  );
    if  (  c ==  EOF  )  {  return  nullptr;  }
    //  20200423
    //  if  (  c == '\n'  )  {  * this  =  "\n";     return  * this;  }

    std :: string  buf  ( 1, c );
    while  ( next() )  {
      if  (  c == ' '  ||  c == '\n'  ||  c == '\t'  )  {  undo();  break; }
      buf .append ( 1, c );  }

    return  buf .length()  ?  buf .c_str()  :  nullptr  ;  }


};    //  end  class  Tokens  ----------------------------  end  class  Tokens




class  Vec  :  public std :: vector < mstr >  {    //    ---------  class  Vec

  //  Vec can cast to char**.  This is used with execve().

  const char * *  p  =  nullptr;
  std :: string   b;

public:

  Vec  ()  {}
  Vec  ( const char * const * p )  {
    for  (  ;  p  &&  * p;  p ++  )  {  push_back ( * p );  }  }

  ~Vec  ()  {
    if  ( p )  {  free ( p );  }  }


  operator  char * const *  ()  {    //  ---------------------  Vec :: op cast
    //  convert Vec to an array of char*
    if  ( p )  {  free ( p );  }
    p  =  (const char**) malloc ( (size() + 1 ) * ( sizeof ( char * ) ) );
    for  (  size_t n = 0;  n < size();  n ++  )  {  p[n]  =  at(n).s;  }
    p[size()]  =  nullptr;
    //  note:  we cast away const to allow passwing to execv().
    return  (char**) p;  }


  void  consume  ( Tokens & t )  {    //  --------------------  Vec :: consume
    while  ( t )  {  push_back ( t .next() );  }  }


  const char *  s  ()  {    //  ------------------------------------  Vec :: s
    b.clear();
    for  (  str & s  :  * this  )  {
      b.append ( s );  b.append ( "  " );  }
    if  ( b.size() > 1 )  {  b.resize ( b.size() - 2 );  }
    return  b.c_str();  }


};    //  end  class  Vec  ----------------------------------  end  class  Vec




class  Env  :  public std :: map < mstr, mstr >  {    //    ------  class  Env

  Vec  v;

public:

  operator  char * const *  ()  {    //  ---------------------  Env :: op cast
    v .clear();
    for  (  const auto & e  :  * this  )  {
      v .push_back ( e.first + "=" + e.second );  }
    return  v;  }


  str  get  ( str & name )  {    //  -----------------------------  Env :: get
    if  ( count ( name ) )  {  return  at ( name );  }
    return  nullstr;  }

};    //  end  class  Env  ----------------------------------  end  class  Env




struct  Profile  :  Lib  {    //  ---------------------------  struct  Profile


  struct  PBind  {
    bool  readonly   =  true;
    bool  recursive  =  false;
    mstr  src, dst;
    void  print  ()  const  {
      printe ( "pbind  %s  %s%s  %s\n",
	       readonly  ?  "ro"  :  "rw",  recursive  ?  "rec  "  :  "",
	       dst.s, src.s );  }  };

  mstr  name;
  mstr  exec_path;
  Vec   exec_argv;
  bool  opt_net    =  false;    //  provide network interfaces
  bool  opt_pulse  =  false;    //  mount /run/user/$UID/pulse
  bool  opt_root   =  false;    //  set uid/gid to zero
  bool  opt_trace  =  false;    //  log syscalls to stderr
  bool  opt_x11    =  false;    //  mount /tmp/.X11-unix
  Env   env;

  std :: vector < PBind >  binds;



  Profile  ()  {}
  /*  20200504
  Profile  ( const char * const * argv )  {  read_from_argv ( argv );  }
  Profile  ( Tokens & t )  {  read_from_config ( t );  }
  */


  operator  bool  ()  const  {  return  name;  }


  bool  has_root_bind  ()  const  {    //  ---------  Profile :: has_root_bind
    for  (  const PBind & b  :  binds  )  {
      if  ( b.dst == "/" )  {  return  true;  }  }
    return  false;  }


  bool  is_opt  ( str & s )  {    //  --------------------------------  is_opt
    return  (  s     .startswith ( "-" )
	       &&  s .spn ( "nprx", 1 ) == s.n  );  }


  void  print  ( const PBind & b, const char * s )  const  {    //  ---  print
    printe ( "%sbind  %2s  %s  %s\n", s, b.readonly  ?  ""  :  "rw",
	     b.dst.s, b.src.s );  }


  void  print  ()  const  {    //  -------------------------  Profile :: print
    printe ( "profile  %s\n", name.s );
    for  (  const PBind & b  :  binds  )  {  print ( b, "  " );  }
    printe ( "  net %d  pulse %d  x11 %d\n", opt_net, opt_pulse, opt_x11 );  }


  void  read_bind  ( Tokens & t )  {    //  ------------  Profile :: read_bind
    if  ( t.is("bind") )  {
      PBind  b;
      while  ( true )  {
	if       ( t.is("rec") )  {  b.recursive  =  true;   }
	else if  ( t.is("ro")  )  {  b.readonly   =  true;   }
	else if  ( t.is("rw")  )  {  b.readonly   =  false;  }
	else  {  break;  }  }
      b.dst  =  t.next();
      b.src  =  t.next();
      binds .push_back ( b );  }
    else  {  t .bad_token ( "profile  read_bind" );  }  }


  void  read_opts  ( Tokens & t )  {    //  ------------  Profile :: read_opts
    if  ( t.is("opts") )  {
      while  ( true )  {
	if       ( t.is("net")     )  {  opt_net    =  true;  }
	else if  ( t.is("x11")     )  {  opt_x11    =  true;  }
	else if  ( t.is("pulse")   )  {  opt_pulse  =  true;  }
	else  {  return;  }  }  }
    else  {  t .bad_token ( "profile  read_opts" );  }  }


  void  read_setenv  ( Tokens & t )  {    //  --------  Profile :: read_setenv
    auto  is_setenv  =  []  ( str & s )  {
      return  s.spn("./") == 0  and  s.index('=');  };    //  a kludge?
    while  ( is_setenv ( t.peek() ) )  {
      str  head  =  t .peek() .head ( "=" );
      str  tail  =  t .next() .tail ( "=" );
      //  printe ( "read_setenv  %s  %s\n", head.s, tail.s )
      env [ head ]  =  tail;  }  }


  void  read_argv_command  ( Tokens & t )  {    //  -------  read_argv_command
    read_setenv ( t );
    exec_path  =  t.peek();
    exec_argv .consume ( t );  }


  void  read_argv_option  ( Tokens & t )  {    //  ---------  read_argv_option
    if  ( is_opt (t.peek()) )  {
      if  ( t.index('n') )  {  opt_net    =  true;  }
      if  ( t.index('p') )  {  opt_pulse  =  true;  }
      if  ( t.index('r') )  {  opt_root   =  true;  }
      if  ( t.index('x') )  {  opt_x11    =  true;  }
      t.next();  }  }


  void  read_argv_path  ( Tokens & t )  {    //  -------------  read_argv_path
    str  path  =  realpath ( t.next() );
    if  ( is_dir ( path ) )  {
      PBind b;  b.src=path;  b.dst="/";  b.readonly=false;
      binds.push_back(b);  }
    else  {  die2 ( "read_argv_path  expected directory  %s", path.s );  }  }


  void  read_argv_profile  ( Tokens & t )  {    //  -------  read_argv_profile
    if  ( name )  { die2 ( "read_argv_profile  unexpected second profile  %s",
			   t.peek().s );  }
    name  =  t.next();  config_load_profile();  }


  void  read_argv  ( Tokens & t )  {    //  -----------------------  read_argv
    while  ( t )  {
      if       ( t.is("--")  )  {  read_argv_command(t);  }
      else if  ( t.spn("-")  )  {  read_argv_option(t);   }
      else if  ( t.spn("./") )  {  read_argv_path(t);     }
      else                      {  read_argv_profile(t);  }  }  }


  static void  config_read_profile  ( Tokens & t, Profile & p )  {    //  ----
    while  ( t )  {
      if       ( t.peek("bind" ) )  {  p.read_bind ( t );  }
      else if  ( t.peek("opts" ) )  {  p.read_opts ( t );  }
      else  {  break;  }  }  }


  void  config_load_profile  ()  {    //    -------------  config_load_profile
    Tokens  t  ( home() + "/.config/lxroot/config" );
    while  ( t )  {
      if  ( t.is("profile") )  {
	if  ( t.next() == name )  {
	  config_read_profile ( t, * this );  return;  }
	else  {  Profile p;  config_read_profile ( t, p );  }  }
      else  {  die2 ( "load_profile  parse error  %s", t.s() );  }  }
    die2  ( "load_config  profile not found  %s", name.s );  }




  /*  20200504
      Profile  p;  p .read_from_config ( config );
      //  20200504  warning  kludge  config will overwrite argv !!
      if  ( p .name == name )  {  * this  =  p;  }  }  }

  void  read_from_config  ( Tokens & t )  {    //  ---------  read_from_config
    if  ( t.is("profile") )  {

      name  =  t.next();
      while  ( t )  {
	if       ( t.peek("bind" ) )  {  read_bind ( t );  }
	else if  ( t.peek("opts" ) )  {  read_opts ( t );  }
	else  {  break;  }  }  }
    else  {  t .bad_token ( "profile  read_from" );  }  }
  */




/*  20200504

      if  ( is_opt (t.peek()) )  {
	if  ( t.index('n') )  {  opt_net    =  true;  }
	if  ( t.index('p') )  {  opt_pulse  =  true;  }
	if  ( t.index('r') )  {  opt_root   =  true;  }
	if  ( t.index('x') )  {  opt_x11    =  true;  }
	t.next();  }
      else if  ( t.is("exec") )  {
	exec_path  =  t.peek();
	exec_argv .consume ( t );  }
      else if  ( t.is("profile") )  {
	if       ( name == nullptr )  {  name  =  t.next();  }
	else if  ( t.is(name)      )  {  / * name matches * /  }
	else   {  die2 ( "unexpected arg  profile" );  }  }
      else  {
	die2 ( "profile  read_argv  bad_arg  %s", t.peek().s );  }  }
    //  20200504  warning  kludge  config will overwrite argv !!
    load_from_config();  }
*/




};    //  end  struct  Profile  ------------------------  end  struct  Profile




/*  20200504
class  Config  {    //  ---------------------------------------  class  Config


  std :: vector < Profile >  profiles;


public:


  Config  ( str & path )  {  read_from ( Tokens ( path ) );  }


  void  read_from  ( Tokens t )  {    //  ---------------  Config :: read_from
    while  ( t )  {  profiles .push_back ( Profile ( t ) );  }  }


  void  print  ()  const  {    //  --------------------------  Config :: print
    for  (  const Profile & p  :  profiles  )  {  p .print();  }  }


  const Profile *  profile  ( const char * name )  const  {    //  --  profile
    for  (  const Profile & p  :  profiles  )  {
      if  ( p .name == name )  {  return  & p;  }  }
    return  nullptr;  }


  void  read_from  ( Tokens & t )  {    //  -------------  Config :: read_from
    while  ( t )  {  profiles .push_back ( Profile ( t ) );  }  }


};    //  end  class Config  -----------------------------  end  class  Config
*/




class  Opendir  {    //  -------------------------------------  class  Opendir

  //  Opendir is a ranged-for wrapper around opendir() and readdir().
  //  usage:  for  ( const Opendir & entry : Opendir ( path ) )  {}

  DIR *  own     =  nullptr;    //  closed in destructor
  DIR *  borrow  =  nullptr;    //  not closed in destructor

public:

  struct dirent *  entp    =  nullptr;
  const char *     s       =  nullptr;

  Opendir  ()  {}
  Opendir  ( str & path )  {  own  =  opendir ( path.s );  }
  Opendir  ( const Opendir & other )  =  default;
  ~Opendir  ()  {  if  ( own )  {  closedir ( own );  }  }


  Opendir  begin  ()  {    //  -----------------------------  Opendir :: begin
    Opendir  rv;  rv .borrow  =  own;  return  ++ rv;  }
  Opendir  end    ()  {  return  Opendir();  }    //  --------  Opendir :: end
  bool  operator !=  ( const Opendir & other )  const  {    //  -------- op !=
    return  entp != other .entp; }
  const Opendir &  operator *  ()  const  {    //  ----------  Opendir :: op *
    return  * this;  }
  Opendir &  operator ++  ()  {    //  ----------------------  Opendir :: op +
    entp  =  readdir();  return  * this;  }


  bool  operator  ==  ( const char * const expect )  const  {    //  --  op ==
    return  s  &&  expect  &&  strcmp ( s, expect ) == 0;  }

  //  operator  const char *  ()  const  {  return  s;  }    //  --  op char *

  ino_t  inode  ()  const  {    //  ------------------------  Opendir :: inode
    return  entp  ?  entp -> d_ino  :  -1  ;  }

  bool  is_dir  ()  const  {    //  -----------------------  Opendir :: is_dir
    switch  ( assert ( entp ) -> d_type )  {
      case  DT_DIR:  return  true;  break;
      case  DT_UNKNOWN:
	printe ( "opendir  is_dir  error  type is dt_unknown\n" );
	exit ( 1 );  break;  }
    return  false;  }

  const char *  name  ()  const  {    //  -------------------  Opendir :: name
    return  entp  ?  entp -> d_name  :  nullptr  ;  }

  struct dirent *  readdir  ()  {    //  -----------------  Opendir :: readdir
    entp  =  borrow  ?  :: readdir ( borrow )  :  nullptr  ;
    s     =  entp  ?  entp -> d_name  :  nullptr;
    return  entp;  }


};    //  end  class  Opendir  ---------------------------  end  clas  Opendir




struct  State  {    //  ---------------------------------------  struct  State

  bool     trace_flag  =  false;
  pid_t    fork        =  -2;
  pid_t    wstatus     =  0;
  State *  st          =  nullptr;    //  st = this.  ( legacay kludge )

};    //  end  struct  State  ----------------------------  end  struct  State


//  system call wrappers  ------------------------------  system call wrappers


//  macro  trace  ----------------------------------------------  macro  trace
#define  trace( format, ... )  {			\
  if  ( st && st -> trace_flag )  {			\
    fprintf ( stderr, "lxroot  trace  "  format "\n",	\
              ##__VA_ARGS__ );  }  }


//  macro  try  --------------------------------------------------  macro  try
#define  try( function, st, format, ... )  {		\
  trace ( format, ##__VA_ARGS__ );		\
  if  ( function ( __VA_ARGS__ ) == 0 )  {  return;  }	\
  else  {  die_pe ( format, ##__VA_ARGS__ );  }  }


//  macro  try2  ------------------------------------------------  macro  try2
#define  try2( function, format, ... )  {		\
  if  ( function ( __VA_ARGS__ ) == 0 )  {  return;  }	\
  else  {  die_pe ( format, ##__VA_ARGS__ );  }  }


//  note  the below capitalized functions call exit(1) on error


void  Bind  ( const State * const  st,    //  --------------------------  Bind
	      const char * const   source,
	      const char * const   target,
	      const unsigned long  flags )  {

  Lib :: assert_is_dir ( source, "bind" );
  Lib :: assert_is_dir ( target, "bind" );

  auto  bind  =  [=]  ( const unsigned long flags )  {
    trace ( "bind     0x%04lx  %-5s  %s", flags, source, target );
    const int  rv  =  mount ( source, target, NULL, flags, NULL );
    if  ( rv == 0 )  {  return;  }
    printe ( "bind  log  %d\n", errno == EINVAL );
    if  (  errno == EINVAL
	   &&  ( flags & MS_BIND )
	   &&  ( not ( flags & MS_REC ) )
	   )  {
      printe (  "\n"  "lxroot  error  bind  0x%04lx  %-5s  %s\n",
		flags, source, target  );
      printe (  "  Note:  An unprivileged non-recursive bind mount() "
                "has failed with EINVAL.\n"
	        "         If apporpriate, try a recursive bind mount() "
		"instead, as follows:\n"
	        "           bind  rec  <dst>  <src>\n"
	        "         See \"man 2 mount\" for further details.\n" );
      exit ( 1 );  }
    die_pe ( "bind  0x%04lx  %-5s  %s", flags, source, target );  };

  const unsigned long  accept  =  MS_RDONLY  |  MS_REC;
  assert ( ( flags | accept ) == accept );

  bind ( MS_BIND | flags );
  if  ( flags & MS_RDONLY )  {
    bind ( MS_BIND | MS_RDONLY | MS_REMOUNT );  }  }


void  Chdir  ( const State * st, const char * path )  {    //  --------  Chdir
  try ( chdir, st, "chdir    %s", path );  }


void  Chroot  ( const State * st, const char * new_root )  {    //  --  Chroot
  try ( chroot, st, "chroot   %s", new_root );  }


void  Close  ( int fd )  {    //  -------------------------------------  Close
  try2 ( close, "close  %d", fd );  }


void  Execve  ( const State *       st,    //  -----------------------  Execve
		str &  pathname,
		Vec &  argv,
		const char * const  envp[] )  {
  trace ( "execve   %s  %s\n", pathname.s, argv.s() );
  execve ( pathname, argv, (char**) envp );
  //  execve only returns on failure, so ...
  die_pe ( "execve  %s", pathname.s );  }


void  Fork  ( State * st )  {    //  -----------------------------------  Fork
  if  ( st -> fork != -2 )  {  die_pe ( "extra fork?" );  }
  if  ( ( st -> fork = fork() ) >= 0 )  {
    trace ( "fork     (fork returned %d)", st -> fork );
    return;  }
  die_pe ( "fork" );  }


void  Mkdir  ( const State *  st,    //  ------------------------------  Mkdir
	       const char *   path,
	       const mode_t   mode )  {
  if  ( Lib :: is_dir ( path ) )  {  return;  }
  try ( mkdir, st, "mkdir    %s  %o", path, mode );  }


void  Mount  ( const State *  st,    //  ------------------------------  Mount
	       const char *   source,
	       const char *   target,
	       const char *   filesystemtype )  {
  trace ( "mount    %s  %s  %s", source, target, filesystemtype );
  if  ( mount ( source, target, filesystemtype, 0, NULL ) == 0 )  {  return;  }
  die_pe ( "mount  %s  %s  %s",  source, target, filesystemtype );  }


void  Open  ( int *          fd,
	      const char *   pathname,
	      const int      flags )  {
  if  ( ( * fd = open ( pathname, flags ) ) >= 0 )  {  return;  }
  die_pe ( "open  %s  %d", pathname, flags );  }


void  Pivot  ( const State *  st,    //  ------------------------------  Pivot
	       const char *   new_root,
	       const char *   put_old )  {
  trace ( "pivot    %s  %s", new_root, put_old );
  if  ( syscall ( SYS_pivot_root, new_root, put_old ) == 0 )  {  return;  }
  die_pe ( "pivot  %s  %s", new_root, put_old );  }


void  Rmdir  ( const State * st, const char * pathname )  {    //  ----  Rmdir
  try ( rmdir, st, "rmdir    %s", pathname );  }


void  Umount2  ( const State *  st,    //  --------------------------  Umount2
		 const char *   target,
		 int            flags )  {
  try ( umount2, st, "umount2  %s  0x%x", target, flags );  }


void  Unshare  ( const State * st, const int flags )  {    //  ------  Unshare
  try ( unshare, st, "unshare  0x%08x", flags );  }


pid_t  Wait  ( State * st )  {    //  ----------------------------------  Wait
  trace ( "wait     (parent calls wait)" );
  pid_t  pid  =  wait ( & st -> wstatus );
  if  ( pid > 0 )  {
    trace ( "wait     wait returned  pid %d  status 0x%x",
		  pid, st -> wstatus );
    return  pid;  }
  die_pe ( "wait" );  }


void  Write  ( int fd, const void * buf, ssize_t count )  {    //  ----  Write
  assert ( count >= 0 );
  if  ( write ( fd, buf, count ) == count )  {  return;  }
  die_pe ( "write  %d  %ld", fd, count );  }


void  Makedirs  ( const State * st,
		  str & path, const mode_t mode )  {    //  --------  Makedirs

    if  ( Lib :: is_dir ( path ) )  {  return;  }

    for  ( str & s : str(path) .descend() )  {
      if  ( not Lib :: is_dir ( s ) )  {
	printe ( "lxroot  mkdir  %s\n", s.s );
	Mkdir ( st, s, mode );  }  }  }


//  end  wrappers  --------------------------------------------  end  wrappers




struct  Launcher  :  State, Profile  {    //  --------------  struct  Launcher


  uid_t  uid          =  getuid();
  gid_t  gid          =  getgid();
  mstr   pivot        =  home()  +  "/.lxroot/.pivot";
  mstr   put_old;


  void  do_unshare  ()  {    //  ---------------------  Launcher :: do_unshare
    int  clone_flags  =  0;
    clone_flags  |=  binds.size()  ?  CLONE_NEWNS  :  0  ;
    clone_flags  |=  CLONE_NEWPID;
    clone_flags  |=  CLONE_NEWUSER;
    clone_flags  |=  opt_net  ?  0  :  CLONE_NEWNET  ;
    Unshare ( this, clone_flags );  }


  void  do_uid_map  ()  {    //  ---------------------  Launcher :: do_uid_map

    //  see  https://lwn.net/Articles/532593/

    uid_t  un_uid  =  opt_root  ?  0  :  uid  ;
    gid_t  un_gid  =  opt_root  ?  0  :  gid  ;

    char  u_map[80];
    char  g_map[80];
    int   fd;

    snprintf ( u_map, sizeof u_map, "%u %u 1\n", un_uid, uid );
    snprintf ( g_map, sizeof g_map, "%u %u 1\n", un_gid, gid );

    trace ( "uid_map  %u %u 1  deny  %u %u 1",
	    un_uid,  uid,  un_gid,  gid );

    Open   (  & fd,  "/proc/self/uid_map",  O_RDWR    );
    Write  (  fd,  u_map,  strlen ( u_map )           );
    Close  (  fd                                      );
    Open   (  & fd,  "/proc/self/setgroups",  O_RDWR  );
    Write  (  fd, "deny", 4                           );
    Close  (  fd                                      );
    Open   (  & fd,  "/proc/self/gid_map",  O_RDWR    );
    Write  (  fd, g_map, strlen ( g_map )             );
    Close  (  fd                                      );  }


  void  bind_prepare  ()  {    //  -----------------  Launcher :: bind_prepare
    for  ( const PBind & b : binds )  {
      if  ( b.dst == "/" )  {  pivot  =  b.src;  break;  }  }
    Makedirs ( st, pivot, 0700 );  }


  void  bind_opt_pulse  ()  {    //  -------------  Launcher :: bind_opt_pulse
    if  ( opt_pulse )  {
      str  xdg_dir  =  getenv ( "XDG_RUNTIME_DIR" );
      if  ( xdg_dir )  {
	str  pulse_dir  =  xdg_dir + "/pulse";
	Bind ( st, pulse_dir, pivot + pulse_dir, 0 );
	return;  }
      else  {
	die2 ( "bind_opt_pulse()  XDG_RUNTIME_DIR not set\n" );  }  }  }


  void  bind_opt_x11  ()  {    //  -----------------  Launcher :: bind_opt_x11
    if  ( opt_x11  )  {
      Bind(  st,  "/tmp/.X11-unix", pivot + "/tmp/.X11-unix", 0  );  }  }


  void  bind_opts  ()  {    //  -----------------------  Launcher :: bind_opts
    bind_opt_pulse();
    bind_opt_x11();  }


  void  bind_skel_prepare  ()  {    //  -------  Launcher :: bind_skel_prepare

    str  skel  =  home() + "/.lxroot/.skel";

    if  ( not is_dir ( skel ) )  {
      assert ( :: mkdir ( skel, 0755 ) == 0 );  }

    if  ( not is_empty_dir ( skel ) )  {  return;  }

    auto  mkdir  =  [&]  ( const char * s )  {
      if  ( is_dir ( str("/") + s ) )  {
	str  path  =  skel + "/" + s;
	printe ( "mkdir    %s\n", path.s );
	assert ( :: mkdir ( path, 0755 ) == 0 );  }  };

    auto  symlink  =  [&]  ( const char * s )  {
      if  ( is_link ( str("/") + s ) )  {
	str  text  =  str("usr/") + s;
	str  path  =  skel + "/" + s;
	printe ( "symlink  %s\n", path.s );
	assert ( :: symlink ( text, path ) == 0 );  }  };

    str  dirs   =  " boot dev etc home opt proc run srv sys usr tmp var ";
    str  links  =  " bin lib lib32 lib64 libx32 sbin ";

    for  (  str & s  :  dirs  .tokens()  )  {  mkdir   ( s );  }
    for  (  str & s  :  links .tokens()  )  {  symlink ( s );  }  }


  bool  bind_ubuntu  ()  {    //  -------------------  Launcher :: bind_ubuntu

    if  ( has_root_bind() )  {  return  false;  }

    bind_skel_prepare();

    str  skel   =  home() + "/.lxroot/.skel";
    str  bind   =  "  /boot  /etc  /opt  /usr  ";
    //  20200430
    //  str  check  =  bind  +  "  /dev  /proc  /sys  ";

    Bind(  st,  skel,  pivot,  MS_RDONLY  );

    for  ( str & dir  :  bind .tokens()  )  {
      Bind(  st,  dir,  pivot + dir,  MS_RDONLY  );  }

    /*  20200430
    str  src  =  home() + "/.lxroot/" + name;
    Bind(  st,  src + "/home",  pivot + "/home",  0       );
    Bind(  st,  src + "/tmp",   pivot + "/tmp",   0       );  */
    bind_pbinds();

    Bind(  st,  "/dev",         pivot + "/dev",   MS_REC  );
    Bind(  st,  "/sys",         pivot + "/sys",   MS_REC  );

    bind_opts();

    return  true;  }


  /*  20200423  keep bind_auto?
  void  bind_auto  ( str & src )  {    //  ------------  Launcher :: bind_auto
    printe ( "bind_auto  %s\n", src.s );
    if  (  is_dir ( src )  &&  is_dir ( dst + src )  )  {
      Bind(  st,  src,  dst + src,  MS_BIND  );  }  }
  */


  /*  20200423
  void  bind_pulse  ()  {    //  ---------------------  Launcher :: bind_pulse
    mstr  path;  path .snprintf ( "/run/user/%d/pulse", 80, uid );
    for  (  str & s  :  path .descend()  )  {
      printf ( "  %s\n", s.s );
      ;;;  }
    ;;;; }
  */


  /*  20200423
  void  bind_x11  ()  {    //  -------------------------  Launcher :: bind_x11
    printe ( "bind_opt_x11  %d\n", opt_x11 );
    if  ( opt_x11 )  {  bind_auto ( "/tmp/.X11-unix" );  }  }
  */


  str  bind_src ( str & src, str & dst )  {    //  -----  Launcher :: bind_src
    assert ( dst .startswith ( "/" ) );
    if  ( src == "." )  {  return  home() + "/.lxroot/" + name + dst;  };
    if  ( src .startswith ( "/" ) )  {  return  src;  }
    return  home() + "/.lxroot/" + src;  }


  void  bind_pbinds  ()  {    //  -------------------  Launcher :: bind_pbinds
    for  (  const PBind & b  :  binds  )  {
      str            src    =  bind_src ( b.src, b.dst );
      unsigned long  flags  =  0;
      flags  |=  (  b.recursive  ?  MS_REC     :  0  );
      flags  |=  (  b.readonly   ?  MS_RDONLY  :  0  );
      Bind(  st,  src,  pivot + b.dst,  flags  );  }  }


  void  do_bind  ()  {    //  ---------------------------  Launcher :: do_bind
    bind_prepare();
    if  ( bind_ubuntu() )  {  return;  }
    bind_pbinds();
    Bind(  st,  "/dev",  pivot + "/dev",  MS_REC  );
    Bind(  st,  "/sys",  pivot + "/sys",  MS_REC  );
    bind_opts();  }


  void  pivot_prepare  ( str & pivot )  {    //  -----------  pivot_preapre
    getpwent();    //  load getpwent()'s shared libraries *before* pivot !!
    endpwent();
    //  verify that at pivot has at least one sub-direcotry (for put_old)
    for  (  const Opendir & e  :  Opendir ( pivot )  )  {
      if  (  e == "."  ||  e == ".."  )  {  continue;  }
      if  (  e.is_dir()  )  {
	put_old  =  str ( "/", e.name() );  return;  }  }
    printe ( "pivot_prepare  error  pivot contains no directories\n" );
    exit ( 1 );  }


  void  do_pivot  ()  {    //  -------------------------  Launcher :: do_pivot
    pivot_prepare ( pivot );
    Pivot(   st,  pivot,  pivot + put_old  );
    Chdir(   st,  "/"  );
    Chroot(  st,  "/"  );  }


  void  do_fork  ()  {    //  ---------------------------  Launcher :: do_fork
    Fork ( st );
    if  ( st -> fork == 0 )  {  return;  }    //  child returns
    if  ( st -> fork >  0 )  {    //  parent waits for child to exit
      Wait ( st );
      if  ( WIFEXITED ( st -> wstatus ) )  {
	exit ( WEXITSTATUS ( st -> wstatus ) );  }
      fprintf ( stderr, "lxroot  warning  child exited abnormally\n" );
      exit ( 1 );  }  }


/*  20200520
    if  ( env .count ( "SHELL" ) )  {
      str  shell  =  env .at ( "SHELL" );
      if  ( access ( shell.s, X_OK ) == 0 )  {
	exec_path  =  shell;
	exec_argv .push_back ( shell );
	return;  }  }

    if  ( access ( "/bin/bash", X_OK ) == 0 )  {
      exec_path  =  "/bin/bash";
      exec_argv .push_back ( "bash" );  }
    else if  ( access ( "/bin/sh", X_OK ) == 0 )  {
      exec_path  =  "/bin/sh";
      exec_argv .push_back ( "sh" );  }
    else  {
      printe ( "lxroot  not found  /bin/sh\n" );
      exit ( 1 );  }

    if  (  (  eq ( getenv("TERM"), "xterm-256color" )    //  set PS1
	      &&  env .count ( "SHELL" )
	      &&  env [ "SHELL" ] == "/bin/bash"  )
	   ||  true  )  {    //  todo  adjust conditions
      mstr  prompt;
      const char *  format  =
	"\\n\\[\\e[0;96m\\]\\u  lx(%s)  \\W\\[\\e[0;39m\\]  "  ;
      prompt .snprintf ( format, 128, name.s );
      env [ "PS1" ]  =  prompt;  }
 }
*/


  void  exec_prepare_argv  ()  {    //  -------  Launcher :: exec_prepare_argv

    auto  try_shell  =  [this]  ( str & shell )  {

      if  ( exec_path or not shell )  {  return;  };

      if  ( is_file ( shell )  and  access ( shell.s, X_OK ) == 0 )  {

	exec_path  =  shell;
	exec_argv .push_back ( shell );

	if  ( shell == "/bin/bash" )  {
	  exec_argv .push_back ( "--norc" );
	  mstr  prompt;
	  const char *  format  =
	    "\\n\\[\\e[0;96m\\]\\u  lx(%s)  \\W\\[\\e[0;39m\\]  "  ;
	  //  todo  fix name when there is no profile.
	  prompt .snprintf ( format, 128, name.s );
	  env [ "PS1" ]  =  prompt;  }

	if  ( shell == "/bin/sh" )  {
	  mstr  prompt;
	  str   user  =  env.get("USER")  ?  env.get("USER")  :  "user"  ;
	  const char *  format  = "\n%s  lx(%s)  $  "  ;
	  prompt .snprintf ( format, 128, user.s, "todo" );
	  env [ "PS1" ]  =  prompt;  }  }  };

    try_shell ( env .get ( "SHELL" ) );
    try_shell ( "/bin/bash" );
    try_shell ( "/bin/sh" );  }


  void  exec_prepare_env  ()  {    //  ---------  Launcher :: exec_prepare_env

    mstr             s;
    const uid_t      uid    =  getuid();
    struct passwd *  pwent  =  nullptr;

    while  ( ( pwent = getpwent() ) )  {    //  parse /etc/passwd
      if ( pwent -> pw_uid == uid )  {  break;  }  }
    if  ( pwent )  {
      env [ "HOME"    ]  =  pwent -> pw_dir;
      env [ "LOGNAME" ]  =  pwent -> pw_name;
      env [ "SHELL"   ]  =  pwent -> pw_shell;
      env [ "USER"    ]  =  pwent -> pw_name;
      if  (  binds.size()  &&  is_dir ( pwent -> pw_dir ) )  {
	Chdir ( st, pwent -> pw_dir );  }  }
    endpwent();

    if  ( s = getenv ( "TERM" ) )  {  env [ "TERM" ]  =  s;  }

    if  ( opt_pulse )  {    //  set XDG_RUNTIME_DIR
      str  xdg_dir  =  getenv ( "XDG_RUNTIME_DIR" );
      if  ( xdg_dir )  {  env [ "XDG_RUNTIME_DIR" ]  =  xdg_dir;  }  }

    if  ( opt_x11 && ( s = getenv ( "DISPLAY" ) ) )  {    //  set DISPLAY
      env [ "DISPLAY" ]  =  s;  }



    if  ( env .count ( "PATH" ) == 0 )  {
      env [ "PATH" ]  =
	(  "/usr/local/bin:"   "/usr/bin:"   "/bin:"
	   "/usr/local/sbin:"  "/usr/sbin:"  "/sbin"  );  }

    ;;;  }


  void  do_exec  ()  {    //  ---------------------------  Launcher :: do_exec

    //  We try to do as much as possible before calling fork().  Howewer...
    //  It seems /proc can only be mounted:
    //    1) after fork() (which sort of makes sense), and
    //    2) before unmounting put_old (which is mildly surprising?).

    //  Therefore, we mount() /proc and umount2() put_old here, in the
    //  child process, to ensure both calls finish before execve() is
    //  called.

    if  ( binds.size() )  {
      Mount    (  st,  "proc",  "/proc",  "proc"  );
      Umount2  (  st,  put_old,  MNT_DETACH       );  }

    exec_prepare_env();
    exec_prepare_argv();
    Execve ( st, exec_path, exec_argv, env );  }


  void  launch  ( Tokens & argv )  {    //  --------------  Launcher :: launch

    Profile :: read_argv ( argv );
    st          =  this;
    trace_flag  =  opt_trace;

    //  20200507
    //  printe ("\n");  Profile :: print();  printe ("\n");
    //  trace_flag  =  true;

    do_unshare();  do_uid_map();
    if  ( binds.size() )  {  do_bind();  do_pivot();  }
    do_fork();  do_exec();  }


};    //  end  struct  Launcher  ----------------------  end  struct  Launcher




struct  Lxroot  :  Lib  {    //  ------------------------------  class  Lxroot

  //  20200430
  //  str       home_lxroot  =  home() + "/.lxroot";
  //  str       cwd          =  getcwd();

  //  Vec       args;    //  20200427

  /*  20200427
  str  arg  ( size_t n )  {
    if  ( n < args .size() )  {  return  args .at ( n );  }
    return  nullptr;  }
  */

  /*  20200427
  void  home_init  ( const char * home )  {    //  ------  Lxroot :: home_init
    if  ( not is_dir ( home ) )  {
      assert ( :: mkdir ( home, 0755 ) == 0 );  }  }
  */

  /*  20200427  perhaps this should be renamed to skel_init  ??
    */

/*  20200420  I will probably refactor these.
  void  inside_dot_lxroot  ()  {    //  -------------------  inside_dot_lxroot
    if  ( cwd .startswith ( home_lxroot + "/" ) )  {
      str  new_root  =  cwd .head ( "/", home_lxroot.n + 1 );
      str  new_dir   =  cwd.sub ( new_root.n );
      //  20200420
      //  launch ( new_root, new_dir );
      ;;;  }  }

  void  inside_symlink_lxroot  ()  {    //  -----------  inside_symlink_lxroot
    for  (  const Opendir & entry  :  Opendir ( home_lxroot )  )  {
      if  (  entry == "."  ||  entry == ".."  )  {  continue;  }
      str  rp  =  realpath ( home_lxroot + "/" + entry.s );
      if  ( cwd .startswith ( rp ) )  {
	str  new_dir  =  cwd .sub ( rp.n );
	//  20200420
	//  launch ( rp, new_dir );
	;;;  }  }  }

  void  inside_home  ()  {    //  -------------------------------  inside_home
   if  ( getcwd() .startswith ( home() ) )  {
     printe ( "lxroot  inside_home  not implemented\n" );
     exit ( 2 );  }  }
*/


/*  20200504
  int  main_20200504  ( const int argc, const char * const argv[] )  {    //  ---  main

    //  20200429  I am unhappy with the poor readability of this function.

    auto  body  =  [&]  ()  {

      if  ( argc <= 1 )  {  return;  }
      Profile p ( argv + 1 );
      if  ( p.name == nullptr )  {  return;  }
      Config c ( home() + "/.config/lxroot/config" );
      const Profile *  pp  =  c .profile ( p.name );
      if  ( pp == nullptr )  {
	die2 ( "lxroot  profile not found  %s\n", p.name.s );  }
      p  =  * pp;

      p .read_from_argv ( argv + 1 );

      p .print();

      Launcher  l;  l .launch ( p );
      printe ( "lxroot  launch() return unexpectedly\n" );
      exit ( 1 );  };

    body();  usage();  return  1;  }


  int  main  ( const int argc, const char * const argv[] )  {    //  ---  main
    return  1;
  }
  */

};    //  end  class  Lxroot  ---------------------------------  class  Lxroot




int  main  ( const int argc, const char * const argv[] )  {    //  -----  main
  if  ( argc == 1 )  {  usage();  return  1;  }
  Launcher l;  Tokens t (argv,1);  l.launch(t);
  printe ( "lxroot  error  launch() return unexpectedly\n" );
  return  1;  }




//    lxroot.c  -  a Linux rootless alternative to /usr/sbin/chroot
//
//    Copyright (c) 2020 Parke Bostrom, parke.nexus at gmail.com
//
//    This program is free software; you can redistribute it and/or
//    modify it under the terms of version 2 of the GNU General Public
//    License as published by the Free Software Foundation.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public
//    License along with this program; if not, write to the Free
//    Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//    Boston, MA 02110-1301 USA.
