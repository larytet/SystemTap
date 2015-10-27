/* COVERAGE: mount oldumount umount */
#include <sys/types.h>
#include <sys/mount.h>

#ifndef MNT_FORCE
#define MNT_FORCE    0x00000001      /* Attempt to forcibily umount */
#endif

#ifndef MNT_DETACH
#define MNT_DETACH   0x00000002      /* Just detach from the tree */
#endif

#ifndef MNT_EXPIRE
#define MNT_EXPIRE   0x00000004      /* Mark for expiry */
#endif

int main()
{
  mount ("mount_source", "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments");
  //staptest// mount ("mount_source", "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = -NNNN (ENOENT)

  umount("umount_target");
  //staptest// umount ("umount_target", 0x0) = -NNNN (ENOENT!!!!EPERM)

  umount2("umount2_target", MNT_FORCE);
  //staptest// umount ("umount2_target", MNT_FORCE) = -NNNN (ENOENT!!!!EPERM)

  umount2("umount2_target", MNT_DETACH);
  //staptest// umount ("umount2_target", MNT_DETACH) = -NNNN (ENOENT!!!!EPERM)

  umount2("umount2_target", MNT_EXPIRE);
  //staptest// umount ("umount2_target", MNT_EXPIRE) = -NNNN (ENOENT!!!!EPERM)

  // Limits testing.
  mount ((char *)-1, "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments");
#ifdef __s390__
  //staptest// mount (0x[7]?[f]+, "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#else
  //staptest// mount (0x[f]+, "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#endif

  mount ("mount_source", (char *)-1, "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments");
#ifdef __s390__
  //staptest// mount ("mount_source", 0x[7]?[f]+, "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#else
  //staptest// mount ("mount_source", 0x[f]+, "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#endif
  
  mount ("mount_source", "mount_target", (char *)-1, MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments");
#ifdef __s390__
  //staptest// mount ("mount_source", "mount_target", 0x[7]?[f]+, MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#else
  //staptest// mount ("mount_source", "mount_target", 0x[f]+, MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, "some arguments") = NNNN
#endif

  mount ("mount_source", "mount_target", "ext2", (unsigned long)-1, "some arguments");
  // We've got a problem here. On a 32-bit kernel (i686 for instance),
  // MAXSTRINGLEN is only 256. Passing a -1 as the mount flags value
  // produces a string of around 225 characters on newer kernels. So,
  // the full argument output gets truncated. So, we'll make the end
  // of the arguments optional.
  //
  //staptest// mount ("mount_source", "mount_target", "ext2", MS_[^ ]+[[[[|XXXX, "some arguments"]]]]?) = NNNN

  mount ("mount_source", "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, (void *)-1);
#ifdef __s390__
  //staptest// mount ("mount_source", "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, 0x[7]?[f]+) = NNNN
#else
  //staptest// mount ("mount_source", "mount_target", "ext2", MS_NOSUID|MS_NOATIME|MS_NODIRATIME|MS_BIND, 0x[f]+) = NNNN
#endif

  umount2((char *)-1, MNT_FORCE);
#ifdef __s390__
  //staptest// umount (0x[7]?[f]+, MNT_FORCE) = NNNN
#else
  //staptest// umount (0x[f]+, MNT_FORCE) = NNNN
#endif

  umount2("umount2_target", -1);
  //staptest// umount ("umount2_target", MNT_[^ ]+|XXXX) = NNNN

  return 0;
}
