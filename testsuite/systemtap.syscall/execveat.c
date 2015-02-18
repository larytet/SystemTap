#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#if !defined(SYS_execveat) && defined(__NR_execveat)
#define SYS_execveat __NR_execveat
#endif

int main() {
#ifdef SYS_execveat
  syscall(SYS_execveat, -1, "/bin/true", -1L, NULL, NULL);
  //staptest// execveat (-1 "/bin/true"  0x0) = -NNNN (EFAULT)
  syscall(SYS_execveat, -1, "/bin/true", NULL, -1L, NULL);
  //staptest// execveat (-1 "/bin/true"  0x0) = -NNNN (EFAULT)
  syscall(SYS_execveat, -1, "/bin/true", NULL, NULL, -1);
  //staptest// execveat (-1 "/bin/true"  AT_SYMLINK_NOFOLLOW|AT_REMOVEDIR|AT_SYMLINK_FOLLOW|AT_NO_AUTOMOUNT|AT_EMPTY_PATH|XXXX) = -NNNN
  syscall(SYS_execveat, AT_FDCWD, "", NULL, NULL, NULL);
  //staptest// execveat (AT_FDCWD ""  0x0) = -NNNN (ENOENT)
  syscall(SYS_execveat, -1, -1L, NULL, NULL, NULL);
#if __WORDSIZE == 64
  //staptest// execveat (-1 [16]?[f]+  0x0) = -NNNN (EFAULT)
#else
  //staptest// execveat (-1 [8]?[f]+  0x0) = -NNNN (EFAULT)
#endif
  syscall(SYS_execveat, -1, "/bin/true", NULL, NULL, NULL);
  //staptest// execveat (-1 "/bin/true"  0x0) = NNNN
#endif
  return 0;
}

