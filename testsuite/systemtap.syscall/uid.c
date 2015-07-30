/* COVERAGE: getuid geteuid getgid getegid setuid setresuid getresuid setgid */
/* COVERAGE: setresgid getresgid setreuid setregid setfsuid setfsgid */
#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/fsuid.h>

int main ()
{
  uid_t ruid, euid, suid;
  gid_t rgid, egid, sgid;

  ruid = getuid();
  //staptest// getuid () = NNNN
  
  euid = geteuid();
  //staptest// geteuid () = NNNN
  
  rgid = getgid();
  //staptest// getgid () = NNNN

  egid = getegid();
  //staptest// getegid () = NNNN

  setuid(4096);
  //staptest// setuid (4096) = NNNN

  setuid(-1);
  //staptest// setuid (-1) = NNNN

  seteuid(4097);
  //staptest// setresuid (-1, 4097, -1) = NNNN

  // We can't really test the following, since glibc handles it.
  // seteuid(-1);

  getresuid(&ruid, &euid, &suid);
  //staptest// getresuid (XXXX, XXXX, XXXX) = 0

  getresuid((uid_t *)-1, &euid, &suid);
#ifdef __s390__
  //staptest// getresuid (0x[7]?[f]+, XXXX, XXXX) = -NNNN (EFAULT)
#else
  //staptest// getresuid (0x[f]+, XXXX, XXXX) = -NNNN (EFAULT)
#endif

  getresuid(&ruid, (uid_t *)-1, &suid);
#ifdef __s390__
  //staptest// getresuid (XXXX, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
  //staptest// getresuid (XXXX, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

  getresuid(&ruid, &euid, (uid_t *)-1);
#ifdef __s390__
  //staptest// getresuid (XXXX, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// getresuid (XXXX, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif

  setgid(4098);
  //staptest// setgid (4098) = NNNN

  setgid(-1);
  //staptest// setgid (-1) = NNNN

  setegid(4099);
  //staptest// setresgid (-1, 4099, -1) = NNNN

  // We can't really test the following, since glibc handles it.
  // setegid(-1);

  getresgid(&rgid, &egid, &sgid);
  //staptest// getresgid (XXXX, XXXX, XXXX) = 0

  getresgid((gid_t *)-1, &egid, &sgid);
#ifdef __s390__
  //staptest// getresgid (0x[7]?[f]+, XXXX, XXXX) = -NNNN (EFAULT)
#else
  //staptest// getresgid (0x[f]+, XXXX, XXXX) = -NNNN (EFAULT)
#endif

  getresgid(&rgid, (gid_t *)-1, &sgid);
#ifdef __s390__
  //staptest// getresgid (XXXX, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
  //staptest// getresgid (XXXX, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

  getresgid(&rgid, &egid, (gid_t *)-1);
#ifdef __s390__
  //staptest// getresgid (XXXX, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// getresgid (XXXX, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif

  setreuid(-1, 5000);
  //staptest// setreuid (-1, 5000) = NNNN

  setreuid(5001, -1);
  //staptest// setreuid (5001, -1) = NNNN

  setregid(-1, 5002);
  //staptest// setregid (-1, 5002) = NNNN

  setregid(5003, -1);
  //staptest// setregid (5003, -1) = NNNN

  setfsuid(5004);
  //staptest// setfsuid (5004) = NNNN

  setfsuid(-1);
  //staptest// setfsuid (-1) = NNNN

  setfsgid(5005);
  //staptest// setfsgid (5005) = NNNN

  setfsgid(-1);
  //staptest// setfsgid (-1) = NNNN
  
  return 0;
}	
