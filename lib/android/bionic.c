
// bionic.c

//--------------------------------------------------------------------------------

void check()
{
#begin unsafe
  assert siginfo'size == 128;
  assert stack_t'size == 24;
  assert sigcontext'size == 4384;
  assert ucontext'size == 4560;
  assert sigaction_t'size == 32;
  assert dl_phdr_info'size == 64;
  assert _stat'size == 128;
  assert dirent'size == 280;
  assert _statvfs'size == 112;

  assert in_addr'size == 4;
  assert in6_addr'size == 16;
  assert sockaddr_in'size == 16;
  assert sockaddr_in6'size == 28;
  assert in_pktinfo'size == 12;
  assert in6_pktinfo'size == 20;
  assert epoll_event'size == 16;
  assert msghdr'size == 56;
  assert addrinfo'size == 48;

#end unsafe
}

//--------------------------------------------------------------------------------

public
int negative_errno ()
{
#begin unsafe
  int rc = *__errno ();
#end unsafe
  
  if (rc > 0)
    return -rc;   // make negative
  
  if (rc == 0)
    rc = -1;    // don't return zero as it would signal no error
  
  return rc;
}

//--------------------------------------------------------------------------------


