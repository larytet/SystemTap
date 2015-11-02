/* COVERAGE: rename renameat renameat2 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/fs.h>

// To test for glibc support for renameat():
//
// Since glibc 2.10:
//	_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L
// Before glibc 2.10:
//	_ATFILE_SOURCE

#define GLIBC_SUPPORT \
  (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L \
   || defined(_ATFILE_SOURCE))

// renameat2() was added in kernel 3.15. Since glibc support hasn't
// been added yet, we'll have to add our own.
#if defined(SYS_renameat2)
static inline int __renameat2(int olddirfd, const char *oldpath,
			      int newdirfd, const char *newpath,
			      unsigned int flags)
{
  return syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath, flags);
}
#endif

int main()
{
  int fd1;

  fd1 = creat("file1", S_IREAD|S_IWRITE);
  close (fd1);
  fd1 = creat("file3", S_IREAD|S_IWRITE);
  close (fd1);
  
  mkdir("dir1", S_IREAD|S_IWRITE|S_IEXEC);
  mkdir("dir3", S_IREAD|S_IWRITE|S_IEXEC);
  mkdir("dir5", S_IREAD|S_IWRITE|S_IEXEC);

  fd1 = creat("dir3/file", S_IREAD|S_IWRITE);
  close (fd1);

  rename("file1", "file2");
  //staptest// [[[[rename ("file1", "file2"!!!!renameat (AT_FDCWD, "file1", AT_FDCWD, "file2"]]]]) = 0

  rename("dir1", "dir2");
  //staptest// [[[[rename ("dir1", "dir2"!!!!renameat (AT_FDCWD, "dir1", AT_FDCWD, "dir2"]]]]) = 0

  // This will fail since the target isn't empty.
  rename("dir2", "dir3");
  //staptest// [[[[rename ("dir2", "dir3"!!!!renameat (AT_FDCWD, "dir2", AT_FDCWD, "dir3"]]]]) = -NNNN (ENOTEMPTY!!!!EEXIST)

  // This will fail since you can't rename a file to a directory.
  rename("file2", "dir2");
  //staptest// [[[[rename ("file2", "dir2"!!!!renameat (AT_FDCWD, "file2", AT_FDCWD, "dir2"]]]]) = -NNNN (EISDIR)

  // You can't rename a directory to a subdirectory of itself.
  rename("dir2", "dir2/dir4");
  //staptest// [[[[rename ("dir2", "dir2/dir4"!!!!renameat (AT_FDCWD, "dir2", AT_FDCWD, "dir2/dir4"]]]]) = -NNNN (EINVAL)

  // You can't rename a directory to a file.
  rename("dir2", "file2");
  //staptest// [[[[rename ("dir2", "file2"!!!!renameat (AT_FDCWD, "dir2", AT_FDCWD, "file2"]]]]) = -NNNN (ENOTDIR)

  rename("file2", (char *)-1);
#ifdef __s390__
  //staptest// rename ("file2", 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// [[[[rename ("file2", 0x[f]+!!!!renameat (AT_FDCWD, "file2", AT_FDCWD, 0x[f]+]]]]) = -NNNN (EFAULT)
#endif

  rename((char *)-1, "file2");
#ifdef __s390__
  //staptest// rename (0x[7]?[f]+, "file2") = -NNNN (EFAULT)
#else
  //staptest// [[[[rename (0x[f]+, "file2"!!!!renameat (AT_FDCWD, 0x[f]+, AT_FDCWD, "file2"]]]]) = -NNNN (EFAULT)
#endif

#if GLIBC_SUPPORT
  renameat(AT_FDCWD, "file2", AT_FDCWD, "file1");
  //staptest// renameat (AT_FDCWD, "file2", AT_FDCWD, "file1") = 0

  renameat(AT_FDCWD, "dir2", AT_FDCWD, "dir1");
  //staptest// renameat (AT_FDCWD, "dir2", AT_FDCWD, "dir1") = 0

  // This will fail since the target isn't empty.
  renameat(AT_FDCWD, "dir1", AT_FDCWD, "dir3");
  //staptest// renameat (AT_FDCWD, "dir1", AT_FDCWD, "dir3") = -NNNN (ENOTEMPTY!!!!EEXIST)

  // This will fail since you can't rename a file to a directory.
  renameat(AT_FDCWD, "file1", AT_FDCWD, "dir1");
  //staptest// renameat (AT_FDCWD, "file1", AT_FDCWD, "dir1") = -NNNN (EISDIR)

  // You can't rename a directory to a subdirectory of itself.
  renameat(AT_FDCWD, "dir1", AT_FDCWD, "dir1/dir4");
  //staptest// renameat (AT_FDCWD, "dir1", AT_FDCWD, "dir1/dir4") = -NNNN (EINVAL)

  // You can't rename a directory to a file.
  renameat(AT_FDCWD, "dir1", AT_FDCWD, "file1");
  //staptest// renameat (AT_FDCWD, "dir1", AT_FDCWD, "file1") = -NNNN (ENOTDIR)

  renameat(-1, "dir1", AT_FDCWD, "file1");
  //staptest// renameat (-1, "dir1", AT_FDCWD, "file1") = -NNNN

  renameat(AT_FDCWD, (char *)-1, AT_FDCWD, "file1");
#ifdef __s390__
  //staptest// renameat (AT_FDCWD, 0x[7]?[f]+, AT_FDCWD, "file1") = -NNNN
#else
  //staptest// renameat (AT_FDCWD, 0x[f]+, AT_FDCWD, "file1") = -NNNN
#endif

  renameat(AT_FDCWD, "dir1", -1, "file1");
  //staptest// renameat (AT_FDCWD, "dir1", -1, "file1") = -NNNN

  renameat(AT_FDCWD, "file1", AT_FDCWD, (char *)-1);
#ifdef __s390__
  //staptest// renameat (AT_FDCWD, "file1", AT_FDCWD, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// renameat (AT_FDCWD, "file1", AT_FDCWD, 0x[f]+) = -NNNN (EFAULT)
#endif
#endif

#if defined(SYS_renameat2)
  // the renameat2 syscall is not implemented on ppc64le (kernel-3.10.0-294).
  // arch/powerpc/include/asm/systbl.h has 'SYSCALL(ni_syscall) /* sys_renameat2 */'

  __renameat2(AT_FDCWD, "file3", AT_FDCWD, "file4", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "file3", AT_FDCWD, "file4", 0x0) = 0!!!!ni_syscall () = -NNNN]]]]

  __renameat2(AT_FDCWD, "dir5", AT_FDCWD, "dir6", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "dir5", AT_FDCWD, "dir6", 0x0) = 0!!!!ni_syscall () = -NNNN]]]]

  // This should fail since the target exists (when using RENAME_NOREPLACE).
#ifdef RENAME_NOREPLACE
  __renameat2(AT_FDCWD, "file4", AT_FDCWD, "file2", RENAME_NOREPLACE);
  //staptest// [[[[renameat2 (AT_FDCWD, "file4", AT_FDCWD, "file2", RENAME_NOREPLACE)!!!!ni_syscall ()]]]] = NNNN
#endif

  // This will fail since the target isn't empty.
  __renameat2(AT_FDCWD, "dir6", AT_FDCWD, "dir3", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", AT_FDCWD, "dir3", 0x0)!!!!ni_syscall ()]]]] = -NNNN (ENOTEMPTY!!!!EEXIST!!!!ENOSYS)

  // This will fail since you can't rename a file to a directory.
  __renameat2(AT_FDCWD, "file4", AT_FDCWD, "dir6", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "file4", AT_FDCWD, "dir6", 0x0)!!!!ni_syscall ()]]]] = -NNNN (EISDIR!!!!ENOENT!!!!ENOSYS)

  // Normally, this would fail since you can't rename a file to a
  // directory. But with RENAME_EXCHANGE, the kernel will atomically
  // exchange them. Note that since NFS filesystem's don't support
  // RENAME_EXCHANGE, this call can fail.
#ifdef RENAME_EXCHANGE
  __renameat2(AT_FDCWD, "file2", AT_FDCWD, "dir1", RENAME_EXCHANGE);
  //staptest// [[[[renameat2 (AT_FDCWD, "file2", AT_FDCWD, "dir1", RENAME_EXCHANGE) = NNNN!!!!ni_syscall () = -NNNN]]]]
#endif

  // You can't rename a directory to a subdirectory of itself.
  __renameat2(AT_FDCWD, "dir6", AT_FDCWD, "dir6/dir4", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", AT_FDCWD, "dir6/dir4", 0x0)!!!!ni_syscall ()]]]] = -NNNN (EINVAL!!!!ENOSYS)

  // You can't rename a directory to a file without RENAME_EXCHANGE.
  __renameat2(AT_FDCWD, "dir6", AT_FDCWD, "file4", 0);
#ifdef RENAME_EXCHANGE
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", AT_FDCWD, "file4", 0x0)!!!!ni_syscall ()]]]] = NNNN
#else
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", AT_FDCWD, "file4", 0x0)!!!!ni_syscall ()]]]] = -NNNN (ENOTDIR!!!!ENOSYS)
#endif

  __renameat2(-1, "dir6", AT_FDCWD, "file4", 0);
  //staptest// [[[[renameat2 (-1, "dir6", AT_FDCWD, "file4", 0x0)!!!!ni_syscall ()]]]] = -NNNN

  __renameat2(AT_FDCWD, (char *)-1, AT_FDCWD, "file4", 0);
#ifdef __s390__
  //staptest// renameat2 (AT_FDCWD, 0x[7]?[f]+, AT_FDCWD, "file4", 0x0) = -NNNN
#else
  //staptest// [[[[renameat2 (AT_FDCWD, 0x[f]+, AT_FDCWD, "file4", 0x0)!!!!ni_syscall ()]]]] = -NNNN
#endif

  __renameat2(AT_FDCWD, "dir6", -1, "file4", 0);
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", -1, "file4", 0x0)!!!!ni_syscall ()]]]] = -NNNN

  __renameat2(AT_FDCWD, "file4", AT_FDCWD, (char *)-1, 0);
#ifdef __s390__
  //staptest// renameat2 (AT_FDCWD, "file4", AT_FDCWD, 0x[7]?[f]+, 0x0) = -NNNN (EFAULT)
#else
  //staptest// [[[[renameat2 (AT_FDCWD, "file4", AT_FDCWD, 0x[f]+, 0x0)!!!!ni_syscall ()]]]] = -NNNN (EFAULT!!!!ENOSYS)
#endif

  __renameat2(AT_FDCWD, "dir6", AT_FDCWD, "file4", -1);
  //staptest// [[[[renameat2 (AT_FDCWD, "dir6", AT_FDCWD, "file4", RENAME_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN
#endif

  return 0;
}
