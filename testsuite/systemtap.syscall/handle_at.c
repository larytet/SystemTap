/* COVERAGE:  name_to_handle_at open_by_handle_at */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>

int main()
{
// glibc added support for name_to_handle_at() and open_by_handle_at()
// in glibc 2.14.
#if __GLIBC_PREREQ(2, 14)
    struct file_handle *fhp;
    int fd;
    int mount_id;

    fd = creat("foobar", S_IREAD|S_IWRITE);
    close(fd);

    /* Allocate an initial file_handle structure. */
    fhp = malloc(sizeof(*fhp));

    /* Make an initial call to name_to_handle_at() to discover
     * the real size required for the file_handle structure. */
    fhp->handle_bytes = 0;
    name_to_handle_at(AT_FDCWD, "foobar", fhp, &mount_id, 0);
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", XXXX, XXXX, 0x0) = NNNN

    /* Reallocate the file_handle structure with the correct size. */
    fhp = realloc(fhp, sizeof(*fhp) + fhp->handle_bytes);

    /* Get a file handle. */
    name_to_handle_at(AT_FDCWD, "foobar", fhp, &mount_id, 0);
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", XXXX, XXXX, 0x0) = NNNN
    
    /* Now that we have a file handle, open up the file using the handle. */
    fd = open_by_handle_at(AT_FDCWD, fhp, O_RDONLY);
    //staptest// open_by_handle_at (AT_FDCWD, XXXX, O_RDONLY) = NNNN

    close(fd);
    //staptest// close (NNNN) = NNNN

    /* Limit testing. */
    name_to_handle_at(-1, "foobar", fhp, &mount_id, AT_EMPTY_PATH);
    //staptest// name_to_handle_at (-1, "foobar", XXXX, XXXX, AT_EMPTY_PATH) = NNNN

    name_to_handle_at(AT_FDCWD, (char *)-1, fhp, &mount_id, AT_SYMLINK_FOLLOW);
#ifdef __s390__
    //staptest// name_to_handle_at (AT_FDCWD, 0x[7]?[f]+, XXXX, XXXX, AT_SYMLINK_FOLLOW) = NNNN
#else
    //staptest// name_to_handle_at (AT_FDCWD, 0x[f]+, XXXX, XXXX, AT_SYMLINK_FOLLOW) = NNNN
#endif

    name_to_handle_at(AT_FDCWD, "foobar", (struct file_handle *)-1, &mount_id, 0);
#ifdef __s390__
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", 0x[7]?[f]+, XXXX, 0x0) = NNNN
#else
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", 0x[f]+, XXXX, 0x0) = NNNN
#endif

    name_to_handle_at(AT_FDCWD, "foobar", fhp, (int *)-1, 0);
#ifdef __s390__
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", XXXX, 0x[7]?[f]+, 0x0) = NNNN
#else
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", XXXX, 0x[f]+, 0x0) = NNNN
#endif

    name_to_handle_at(AT_FDCWD, "foobar", fhp, &mount_id, -1);
    //staptest// name_to_handle_at (AT_FDCWD, "foobar", XXXX, XXXX, AT_[^ ]+|XXXX) = NNNN

    fd = open_by_handle_at(-1, fhp, O_RDONLY);
    //staptest// open_by_handle_at (-1, XXXX, O_RDONLY) = NNNN

    fd = open_by_handle_at(AT_FDCWD, (struct file_handle *)-1, O_RDONLY);
    //staptest// open_by_handle_at (AT_FDCWD, 0x[f]+, O_RDONLY) = NNNN

    fd = open_by_handle_at(AT_FDCWD, fhp, -1);
    //staptest// open_by_handle_at (AT_FDCWD, XXXX, O_[^ ]+|XXXX) = NNNN

#endif
    return 0;
}
