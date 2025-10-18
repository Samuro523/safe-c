
// bionic.h  -  android's linux-like calls

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

// liblog.so : logging

enum android_LogPriority { ANDROID_LOG_UNKNOWN, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG,
                           ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT };

[extern "liblog.so"]
int __android_log_write (android_LogPriority prio, char* tag, char* text);

//---------------------------------------------------------------------

// libc.so : time

const int CLOCK_REALTIME  = 0;
const int CLOCK_MONOTONIC = 1;

struct TIME
{
  int8 sec, nsec;  // nsec is 10e-9
}

[extern "libc.so"]
int clock_gettime (int clock, out TIME time);

const int TIMER_ABSTIME = 1;

[extern "libc.so"]
int clock_nanosleep (    int clock_id,
                         int flags,
                     ref TIME time,
                         int8 remaining = 0);

struct tm
{
  int tm_sec;  /** Seconds, 0-60. (60 is a leap second.) */
  int tm_min;  /** Minutes, 0-59. */
  int tm_hour;  /** Hours, 0-23. */
  int tm_mday;  /** Day of month, 1-31. */
  int tm_mon;/** Month of year, 0-11. (Not 1-12!) */
  int tm_year;/** Years since 1900. (So 2023 is 123, not 2023!) */
  int tm_wday;  /** Day of week, 0-6. (Sunday is 0, Saturday is 6.) */
  int tm_yday;  /** Day of year, 0-365. */
  int tm_isdst;  /** Daylight savings flag, positive for DST in effect, 0 for DST not in effect, and -1 for unknown. */
  long tm_gmtoff;  /** Offset from UTC (GMT) in seconds for this time. */
  char* tm_zone;  /** Name of the timezone for this time. */
}

[extern "libc.so"]
void gmtime_r (long *timep, tm *result);

[extern "libc.so"]
void localtime_r (long *timep, tm *result);

//---------------------------------------------------------------------

// libc.so : memory allocation

// returns 0 if OK, otherwise EINVAL or ENOMEM
[extern "libc.so"]
int posix_memalign (byte** memptr, long alignment, long size);

// not recommanded, available only since Android Version 9  API 28
// [extern "libc.so"]
// byte *aligned_alloc (long alignment, long size);

//---------------------------------------------------------------------

// libc.so : signal handler

struct siginfo   // 128 bytes
{
  int      si_signo;
  int      si_errno;  // generally unused
  int      si_code;
  byte[32] _sifields;    // 32 bytes
  byte[128 - 44] _si_pad;
}

typedef int8[1] sigset_t;

struct stack_t   // 24 bytes
{
  int8    ss_sp;
  int8    ss_flags;
  int8    ss_size;
}

struct sigcontext   // size 4384 bytes
{
  int8       fault_address;
  int8       regs[31];
  int8       sp;
  int8       pc;
  int8       pstate;
  int8       padding;
  byte[4096] __reserved;  // offset 176 size 4096
}

struct ucontext  // 4560 bytes
{
  long         uc_flags;
  ucontext     *uc_link;
  stack_t      uc_stack;     // offset 16  size 24
  sigset_t     uc_sigmask;   // size 8
  byte[128]    __padding;    // offset 48 size 120 + 8 padding
  sigcontext   uc_mcontext;  // offset 176 size 4384
}

typedef [callback] void SA_SIGACTION (int sig, siginfo si, ucontext context);

const int SA_NOCLDSTOP = 1;          // for signum SIGCHLD, do not receive notification when child processes stop or resume
const int SA_NOCLDWAIT = 2;          // for signum SIGCHLD, do not transform children into zombies when they terminate.
const int SA_SIGINFO   = 4;
const int SA_NODEFER   = 0x40000000; // signal which triggered the handler will not be blocked inside handler
const int SA_ONSTACK   = 0x08000000; // call the signal handler on an alternate signal stack provided by sigaltstack(2).

struct sigaction_t   // 32 bytes
{
  int          sa_flags;      // SA_SIGINFO
  SA_SIGACTION sa_sigaction;
  sigset_t     sa_mask;       // mask of signals to block (ignore)
  byte*        obsolete;
}

[extern "libc.so"]
int sigemptyset (sigset_t set);

const int SIGILL  =  4;
const int SIGBUS  =  7;
const int SIGFPE  =  8;
const int SIGSEGV = 11;

[extern "libc.so"]
int sigaction (int         signum,   // SIGSEGV
               sigaction_t act,
               byte*       oldact = null);

//---------------------------------------------------------------------

// libdl.so : shared library loader

struct Dl_info
{
  char *dli_fname;  // Pathname of shared object that contains address
  long dli_fbase;   // Base address at which shared object is loaded
  char *dli_sname;  // Name of symbol whose definition overlaps addr
  char *dli_saddr;  // Exact address of symbol named in dli_sname
}

[extern "libdl.so"]
int dladdr (long addr, out Dl_info info);

//---------------------------------------------------------------------

struct dl_phdr_info
{
  long     dlpi_addr;  /* Base address of object */
  char*    dlpi_name;  /* (Null-terminated) name of object */
  byte*    dlpi_phdr;  /* Pointer to array of ELF program headers for this object */
  uint2    dlpi_phnum; /* # of items in dlpi_phdr */

  /* The following fields were added in glibc 2.4, after the first
    version of this structure was available.  Check the size
    argument passed to the dl_iterate_phdr callback to determine
    whether or not each later member is available.  */

  long dlpi_adds;  /* Incremented when a new object may have been added */
  long dlpi_subs;  /* Incremented when an object may have been removed */
  long dlpi_tls_modid; /* If there is a PT_TLS segment, its module ID as used in TLS relocations, else zero */
  byte *dlpi_tls_data;  /* The address of the calling thread's instance of this module's PT_TLS segment, if it has
                           one and it has been allocated in the calling thread, otherwise a null pointer */
}

typedef [callback] int dl_iterate_phdr_callback (dl_phdr_info info, long size, byte *data);

[extern "libdl.so"]
int dl_iterate_phdr (dl_iterate_phdr_callback func, byte* data);

//---------------------------------------------------------------------

const int _SC_PAGESIZE = 0x0027;

[extern "libc.so"]
long sysconf (int name);

const int MS_ASYNC = 1;

[extern "libc.so"]
int msync (long addr, long length, int flags);

//---------------------------------------------------------------------

const int O_RDONLY    = 0;
const int O_WRONLY    = 1;
const int O_RDWR      = 2;
const int O_CREAT     = 0x40;
const int O_EXCL      = 0x80;
const int O_TRUNC     = 0x200;
const int O_NONBLOCK  = 0x800;
const int O_APPEND    = 0x400;
const int O_DIRECTORY = 0x10000;
const int O_CLOEXEC   = 0x80000;


[extern "libc.so"]
int umask (int mask);

[extern "libc.so"]
int* __errno ();

[extern "libc.so"]
int open (char *pathname, int flags, uint mode);

[extern "libc.so"]
int read (int fd, byte* buffer, long size);

[extern "libc.so"]
int write (int fd, byte* buffer, long size);

[extern "libc.so"]
long lseek (int fd, long offset, int whence);

struct timespec
{
  long tv_sec;   /** Number of seconds. */
  long tv_nsec;  /** Number of nanoseconds. Must be less than 1,000,000,000. */
}

const uint S_IFDIR = 0x4000;   // directory
const uint S_IFREG = 0x8000;   // file

struct _stat
{
  long st_dev;
  byte __pad0[4];
  uint __st_ino;
  uint st_mode;   // flags S_IFREG for file, S_IFDIR for directory
  int st_nlink;
  int st_uid;
  int st_gid;
  long st_rdev;
  byte __pad3[4];
  long st_size;      // file size, in bytes
  long st_blksize;
  long st_blocks;
  timespec st_atim;
  timespec st_mtim; // This is the time of last modification of file data.
  timespec st_ctim;
  long st_ino;
}

[extern "libc.so"]
int stat (char *pathname, _stat* statbuf);

[extern "libc.so"]
int fstat (int fd, _stat *statbuf);

const long UTIME_OMIT = ((1L << 30) - 2L); // Used in the tv_nsec field to _not_ set that time

[extern "libc.so"]
int futimens(int fd, timespec* times);

[extern "libc.so"]
int close (int fd);


const int LOCK_SH = 1;
const int LOCK_EX = 2;
const int LOCK_NB = 4;
const int LOCK_UN = 8;

[extern "libc.so"]
int flock(int fd, int op);    // lock entire file


const int F_ULOCK = 0;
const int F_LOCK  = 1;
const int F_TLOCK = 2;
const int F_TEST  = 3;

[extern "libc.so"]
int lockf (int fd, int op, long size);   // lock zone in file


const int F_DUPFD = 0;
const int F_GETFD = 1;
const int F_SETFD = 2;
const int F_GETFL = 3;
const int F_SETFL = 4;
const int F_GETLK = 5;
const int F_SETLK = 6;
const int F_SETLKW = 7;
const int F_SETOWN = 8;
const int F_GETOWN = 9;
const int F_SETSIG = 10;
const int F_GETSIG = 11;
const int F_SETOWN_EX = 15;
const int F_GETOWN_EX = 16;
const int F_GETOWNER_UIDS = 17;
const int F_OFD_GETLK = 36;
const int F_OFD_SETLK = 37;
const int F_OFD_SETLKW = 38;
const int F_OWNER_TID = 0;
const int F_OWNER_PID = 1;
const int F_OWNER_PGRP = 2;

struct f_owner_ex
{
  int type;
  int pid;
}

const int FD_CLOEXEC = 1;
const int F_RDLCK = 0;
const int F_WRLCK = 1;
const int F_UNLCK = 2;

const int LOCK_MAND = 32;
const int LOCK_READ = 64;
const int LOCK_WRITE = 128;
const int LOCK_RW = 192;

struct Flock
{
  int2 l_type;
  int2 l_whence;
  int4 padding1;
  int8 l_start;
  int8 l_len;
  int4 l_pid;
  int4 padding2;
}

[extern "libc.so"]
int fcntl(int fd, int op, long arg);

[extern "libc.so"]
int fsync (int fd);

[extern "libc.so"]
int rename (char *oldpath, char *newpath);

[extern "libc.so"]
int chmod (char *name, int mode);

[extern "libc.so"]
int fchmod (int fd, int mode);

[extern "libc.so"]
int unlink (char *pathname);

[extern "libc.so"]
char *getcwd (char* buf, uint size);

[extern "libc.so"]
int chdir (char* path);

[extern "libc.so"]
byte *opendir (char* name);

[extern "libc.so"]
int closedir (byte* dirp);


const byte DT_DIR = 4; // directory.
const byte DT_REG = 8; // regular file

struct dirent
{
  long  d_ino;
  long  d_off;
  uint2 d_reclen;
  byte  d_type;       // flags DT_DIR or DT_REG
  char  d_name[256];  // don't take sizeof
  byte  padding[5];
}

[extern "libc.so"]
dirent* readdir (byte* dirp);

[extern "libc.so"]
int mkdir (char* pathname, int mode = 0x1FF);  // rwx 0777

[extern "libc.so"]
int rmdir (char* pathname);


struct _statvfs
{
  long f_bsize;   // Block size
  long f_frsize;  // Fragment size
  long f_blocks;  // Total size of filesystem in `f_frsize` blocks
  long f_bfree;   // Number of free blocks
  long f_bavail;  // Number of free blocks for non-root
  long f_files;   // Number of inodes
  long f_ffree;   // Number of free inodes
  long f_favail;  // Number of free inodes for non-root
  long f_fsid;    // Filesystem id
  long f_flag;    // Mount flags. (See `ST_` constants)
  long f_namemax; // Maximum filename length
  uint __f_reserved[6];
}

[extern "libc.so"]
int statvfs (char* path, _statvfs* buf);

int negative_errno ();

[extern "libc.so"]
int getpid();

[extern "libc.so"]
int gettid();

[extern "libc.so"]
int readlink (char* pathname, char* buf, uint bufsiz);


[extern "libc.so"]
int eventfd (uint initval, int flags = O_CLOEXEC);

struct pollfd
{
 int   fd;         // file descriptor
 short events;     // requested events
 short revents;    // returned events
}

const short POLLIN  = 0x1;
const short POLLOUT = 0x4;
const short POLLHUP = 0x10;

[extern "libc.so"]
int poll (pollfd* fds, int nfds, int timeout);



struct pthread_mutex_t   { byte[40] _private; }
typedef int8 pthread_mutexattr_t;

const int PTHREAD_MUTEX_RECURSIVE = 1;

[extern "libc.so"]
int pthread_mutexattr_init (pthread_mutexattr_t *attr);

[extern "libc.so"]
int pthread_mutexattr_settype (pthread_mutexattr_t *attr, int type);

[extern "libc.so"]
int pthread_mutexattr_destroy (pthread_mutexattr_t *attr);

[extern "libc.so"]
int pthread_mutex_init (pthread_mutex_t* mutex,  pthread_mutexattr_t* attr);

[extern "libc.so"]
int pthread_mutex_lock (pthread_mutex_t* mutex);

[extern "libc.so"]
int pthread_mutex_unlock (pthread_mutex_t* mutex);

[extern "libc.so"]
int pthread_mutex_destroy (pthread_mutex_t* mutex);


typedef int SOCKET;  // socket handle

[extern "libc.so"]
int socket (int domain, int type, int protocol);

const uint2 AF_INET     = 2;
const uint2 AF_INET6    = 10;

const int IPPROTO_IP   = 0;

const int IPPROTO_ICMP = 1;
const int IPPROTO_TCP  = 6;
const int IPPROTO_UDP  = 17;
const int IPPROTO_IPV6 = 41;

const int SOCK_STREAM = 1;
const int SOCK_DGRAM  = 2;
const int SOCK_NONBLOCK = O_NONBLOCK;
const int SOCK_CLOEXEC  = O_CLOEXEC;

const int SOL_SOCKET   = 1;

const int SOL_DEBUG    = 1;
const int SO_REUSEADDR = 2;
const int SO_TYPE      = 3;
const int SO_ERROR     = 4;
const int SO_DONTROUTE = 5;
const int SO_BROADCAST = 6;
const int SO_SNDBUF    = 7;
const int SO_RCVBUF    = 8;
const int SO_KEEPALIVE = 9;
const int SO_OOBINLINE = 10;
const int SO_NO_CHECK  = 11;
const int SO_PRIORITY  = 12;
const int SO_LINGER    = 13;
const int SO_SNDBUFFORCE = 32;
const int SO_RCVBUFFORCE = 33;


const int TCP_NODELAY = 1;

const int IP_TOS      = 1;
const int IP_TTL      = 2;
const int IP_HDRINCL  = 3;
const int IP_OPTIONS  = 4;
const int IP_ROUTER_ALERT = 5;
const int IP_RECVOPTS = 6;
const int IP_RETOPTS  = 7;
const int IP_PKTINFO  = 8;
const int IP_PKTOPTIONS = 9;
const int IP_MTU_DISCOVER = 10;
const int IP_RECVERR = 11;
const int IP_RECVTTL = 12;
const int IP_RECVTOS = 13;
const int IP_MTU     = 14;


const int IPV6_RECVPKTINFO = 49;
const int IPV6_PKTINFO = 50;


typedef byte[4] in_addr;
typedef byte[16] in6_addr;


packed struct sockaddr_in   // 16 bytes
{
  uint2   sin_family;
  uint2   sin_port;
  in_addr sin_addr;
  byte[8] __pad;
}

packed struct sockaddr_in6   // 28 bytes
{
  uint2    sin6_family;
  uint2    sin6_port;
  uint4    sin6_flowinfo;
  in6_addr sin6_addr;
  uint4    sin6_scope_id;
}

struct addrinfo
{
  uint      ai_flags;     /* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
  uint      ai_family;    /* PF_xxx */
  int       ai_socktype;  /* SOCK_xxx */
  int       ai_protocol;  /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
  uint      ai_addrlen;   /* length of ai_addr */
  uint      filler;
  char*     ai_canonname; /* canonical name for hostname */
  byte*     ai_addr;      /* binary address */
  addrinfo* ai_next;      /* next structure in linked list */
}

[extern "libc.so"]
 int getaddrinfo (char*      node,
                  char*      service,
                  addrinfo*  hints,
                  addrinfo** res);

[extern "libc.so"]
void freeaddrinfo (addrinfo* res);

[extern "libc.so"]
int getnameinfo (byte* addr,
                 uint  addrlen,
                 char* host,
                 uint  hostlen,
                 char* serv,
                 uint  servlen,
                 int   flags);


[extern "libc.so"]
int setsockopt (int socket, int level, int option_name, byte* option_value, uint option_len);



struct in_pktinfo   // 12 bytes
{
  int     ipi_ifindex;
  in_addr ipi_spec_dst;
  in_addr ipi_addr;
}

struct in6_pktinfo  // 20 bytes
{
  in6_addr ipi6_addr;
  int      ipi6_ifindex;
}

[extern "libc.so"]
int bind (int sockfd, byte* addr, uint addrlen);

[extern "libc.so"]
int listen (int sockfd, int backlog);

[extern "libc.so"]
int accept (int sockfd, byte* addr, uint* addrlen);

[extern "libc.so"]
int connect (int sockfd, byte *addr, uint addrlen);

struct iovec
{
  byte* iov_base;
  long  iov_len;
}

struct msghdr
{
  byte*  msg_name;
  uint   msg_namelen;
  uint   filler1;
  iovec* msg_iov;
  long   msg_iovlen;
  byte*  msg_control;
  long   msg_controllen;
  int    msg_flags;
  int    filler2;
}

[extern "libc.so"]
int send (int sockfd, byte* buf, uint len, int flags);

[extern "libc.so"]
int sendto (int sockfd, byte* buf, uint len, int flags, byte* dest_addr, uint addrlen);

[extern "libc.so"]
int sendmsg (int sockfd, msghdr* msg, int flags);

[extern "libc.so"]
int recv  (int sockfd, byte* buf, uint size, int flags);

[extern "libc.so"]
int recvfrom (int sockfd, byte* buf, uint size, int flags, byte* src_addr, uint* addrlen);

[extern "libc.so"]
int recvmsg (int sockfd, msghdr *msg, int flags);


[extern "libc.so"]
int getsockname (int sockfd, byte* addr, uint* addrlen);

[extern "libc.so"]
int getpeername (int sockfd, byte* addr, uint* addrlen);


[extern "libc.so"]
int epoll_create1 (int flags = O_CLOEXEC);

union epoll_data_t
{
  byte* ptr;
  long  g64;
}

struct epoll_event
{
  uint4        events;
  uint4        filler;
  epoll_data_t data;
}

const int EPOLL_CTL_ADD = 1;
const int EPOLL_CTL_DEL = 2;
const int EPOLL_CTL_MOD = 3;

const uint EPOLLIN    = 0x00000001;  // read
const uint EPOLLPRI   = 0x00000002;  // out-of-band data on a TCP socket
const uint EPOLLOUT   = 0x00000004;  // write
const uint EPOLLERR   = 0x00000008;  // error
const uint EPOLLHUP   = 0x00000010;  // Hang up happened
const uint EPOLLRDHUP = 0x00002000;  // close

[extern "libc.so"]
int epoll_ctl (int epfd, int op, int fd, epoll_event event);

[extern "libc.so"]
int epoll_wait (int epfd, epoll_event* events, int maxevents, int timeout);

//---------------------------------------------------------------------

[extern "libc.so"]
byte *malloc (uint size);

[extern "libc.so"]
uint malloc_usable_size (byte *p);

[extern "libc.so:free"]
void freem (byte *p);

[extern "libc.so"]
byte *realloc (byte *p, uint size);

//---------------------------------------------------------------------

[extern "libc.so"]
char* getenv (char *name);

//---------------------------------------------------------------------

// the least significant byte of status (i.e., status & 0xFF) is returned to the parent
// avoid calling this, it causes a bad user alert.

[extern "libc.so"]
void exit (int status);

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------

const int EPERM     = 1;  // Operation not permitted
const int ENOENT    = 2;  // No such file or directory
const int ESRCH     = 3;  // No such process
const int EINTR     = 4;  // Interrupted system call
const int EIO       = 5;  // I/O error
const int ENXIO     = 6;  // No such device or address
const int E2BIG     = 7;  // Arg list too long
const int ENOEXEC   = 8;  // Exec format error
const int EBADF     = 9;  // Bad file number
const int ECHILD    = 10;  // No child processes
const int EAGAIN    = 11;  // Try again
const int ENOMEM    = 12;  // Out of memory
const int EACCES    = 13;  // Permission denied
const int EFAULT    = 14;  // Bad address
const int ENOTBLK   = 15;  // Block device required
const int EBUSY     = 16;  // Device or resource busy
const int EEXIST    = 17;  // File exists
const int EXDEV     = 18;  // Cross-device link
const int ENODEV    = 19;  // No such device
const int ENOTDIR   = 20;  // Not a directory
const int EISDIR    = 21;  // Is a directory
const int EINVAL    = 22;  // Invalid argument
const int ENFILE    = 23;  // File table overflow
const int EMFILE    = 24;  // Too many open files
const int ENOTTY    = 25;  // Not a typewriter
const int ETXTBSY   = 26;  // Text file busy
const int EFBIG     = 27;  // File too large
const int ENOSPC    = 28;  // No space left on device
const int ESPIPE    = 29;  // Illegal seek
const int EROFS     = 30;  // Read-only file system
const int EMLINK    = 31;  // Too many links
const int EPIPE     = 32;  // Broken pipe
const int EDOM      = 33;  // Math argument out of domain of func
const int ERANGE    = 34;  // Math result not representable
const int EDEADLK    = 35;  // Resource deadlock would occur
const int ENAMETOOLONG   = 36;  // File name too long
const int ENOLCK     = 37;  // No record locks available
const int ENOSYS     = 38;  // Function not implemented
const int ENOTEMPTY  = 39;  // Directory not empty
const int ELOOP      = 40;  // Too many symbolic links encountered
const int EWOULDBLOCK  = 41;  // Operation would block
const int ENOMSG     = 42;  // No message of desired type
const int EIDRM      = 43;  // Identifier removed
const int ECHRNG     = 44;  // Channel number out of range
const int EL2NSYNC   = 45;  // Level 2 not synchronized
const int EL3HLT     = 46;  // Level 3 halted
const int EL3RST     = 47;  // Level 3 reset
const int ELNRNG     = 48;  // Link number out of range
const int EUNATCH    = 49;  // Protocol driver not attached
const int ENOCSI     = 50;  // No CSI structure available
const int EL2HLT     = 51;  // Level 2 halted
const int EBADE      = 52;  // Invalid exchange
const int EBADR      = 53;  // Invalid request descriptor
const int EXFULL     = 54;  // Exchange full
const int ENOANO     = 55;  // No anode
const int EBADRQC    = 56;  // Invalid request code
const int EBADSLT    = 57;  // Invalid slot
const int EDEADLOCK  = 58;  // File locking deadlock error
const int EBFONT     = 59;  // Bad font file format
const int ENOSTR     = 60;  // Device not a stream
const int ENODATA    = 61;  // No data available
const int ETIME      = 62;  // Timer expired
const int ENOSR      = 63;  // Out of streams resources
const int ENONET     = 64;  // Machine is not on the network
const int ENOPKG     = 65;  // Package not installed
const int EREMOTE    = 66;  // Object is remote
const int ENOLINK    = 67;  // Link has been severed
const int EADV       = 68;  // Advertise error
const int ESRMNT     = 69;  // Srmount error
const int ECOMM      = 70;  // Communication error on send
const int EPROTO     = 71;  // Protocol error
const int EMULTIHOP  = 72;  // Multihop attempted
const int EDOTDOT    = 73;  // RFS specific error
const int EBADMSG    = 74;  // Not a data message
const int EOVERFLOW  = 75;  // Value too large for defined data type
const int ENOTUNIQ   = 76;  // Name not unique on network
const int EBADFD     = 77;  // File descriptor in bad state
const int EREMCHG    = 78;  // Remote address changed
const int ELIBACC    = 79;  // Can not access a needed shared library
const int ELIBBAD    = 80;  // Accessing a corrupted shared library
const int ELIBSCN    = 81;  // .lib section in a.out corrupted
const int ELIBMAX    = 82;  // Attempting to link in too many shared libraries
const int ELIBEXEC   = 83;  // Cannot exec a shared library directly
const int EILSEQ     = 84;  // Illegal byte sequence
const int ERESTART   = 85;  // Interrupted system call should be restarted
const int ESTRPIPE   = 86;  // Streams pipe error
const int EUSERS     = 87;  // Too many users
const int ENOTSOCK   = 88;  // Socket operation on non-socket
const int EDESTADDRREQ   = 89;  // Destination address required
const int EMSGSIZE       = 90;  // Message too long
const int EPROTOTYPE     = 91;  // Protocol wrong type for socket
const int ENOPROTOOPT     = 92;  // Protocol not available
const int EPROTONOSUPPORT  = 93;  // Protocol not supported
const int ESOCKTNOSUPPORT  = 94;  // Socket type not supported
const int EOPNOTSUPP       = 95;  // Operation not supported on transport endpoint
const int EPFNOSUPPORT     = 96;  // Protocol family not supported
const int EAFNOSUPPORT     = 97;  // Address family not supported by protocol
const int EADDRINUSE       = 98;  // Address already in use
const int EADDRNOTAVAIL    = 99;  // Cannot assign requested address
const int ENETDOWN         = 100; // Network is down
const int ENETUNREACH      = 101; // Network is unreachable
const int ENETRESET        = 102; // Network dropped connection because of reset
const int ECONNABORTED     = 103; // Software caused connection abort
const int ECONNRESET       = 104; // Connection reset by peer
const int ENOBUFS          = 105; // No buffer space available
const int EISCONN          = 106; // Transport endpoint is already connected
const int ENOTCONN         = 107; // Transport endpoint is not connected
const int ESHUTDOWN        = 108; // Cannot send after transport endpoint shutdown
const int ETOOMANYREFS     = 109; // Too many references: cannot splice
const int ETIMEDOUT        = 110; // Connection timed out
const int ECONNREFUSED     = 111; // Connection refused
const int EHOSTDOWN        = 112; // Host is down
const int EHOSTUNREACH     = 113; // No route to host
const int EALREADY         = 114; // Operation already in progress
const int EINPROGRESS      = 115; // Operation now in progress
const int ESTALE           = 116; // Stale NFS file handle
const int EUCLEAN          = 117; // Structure needs cleaning
const int ENOTNAM          = 118; // Not a XENIX named type file
const int ENAVAIL          = 119; // No XENIX semaphores available
const int EISNAM           = 120; // Is a named type file
const int EREMOTEIO        = 121; // Remote I/O error

