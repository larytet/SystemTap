/* COVERAGE: fanotify_init fanotify_mark */

#define _GNU_SOURCE
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

// glibc added support for fanotify_init() and fanotify_mark() in glibc 2.13.
#if __GLIBC_PREREQ(2, 13)
#include <sys/fanotify.h>

#define EVENT_MAX 1024
/* size of the event structure, not counting name */
#define EVENT_SIZE  (sizeof (struct fanotify_event_metadata))
/* reasonable guess as to size of 1024 events */
#define EVENT_BUF_LEN        (EVENT_MAX * EVENT_SIZE)

#define BUF_SIZE 256
static char fname[BUF_SIZE] = "testfile";
static int fd, fd_notify;

static char event_buf[EVENT_BUF_LEN];

int main()
{
    fd = creat(fname, S_IREAD|S_IWRITE);
    close(fd);

    /* Note that the fanotify calls require root access or the
     * CAP_SYS_ADMIN capability. */

    fd_notify = fanotify_init(FAN_CLASS_NOTIF|FAN_NONBLOCK, O_RDONLY);
    //staptest// fanotify_init (FAN_CLASS_NOTIF|FAN_NONBLOCK, O_RDONLY) = NNNN

    fanotify_mark(fd_notify, FAN_MARK_ADD,
		  FAN_ACCESS|FAN_MODIFY|FAN_CLOSE|FAN_OPEN, AT_FDCWD, fname);
    //staptest// fanotify_mark (NNNN, FAN_MARK_ADD, FAN_ACCESS|FAN_MODIFY|FAN_CLOSE_WRITE|FAN_CLOSE_NOWRITE|FAN_OPEN, AT_FDCWD, "testfile") = NNNN

    // Now, modify the test file.
    fd = open(fname, O_WRONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"testfile", O_WRONLY) = NNNN

    write(fd, fname, strlen(fname));
    //staptest// write (NNNN, "testfile", NNNN) = NNNN

    close(fd);
    //staptest// close (NNNN) = 0

    // Read list of events.
    read(fd_notify, event_buf, EVENT_BUF_LEN);
    //staptest// read (NNNN, XXXX, NNNN) = NNNN

    // A real program would process the list of events. We're not
    // going to bother.

    close(fd_notify);
    //staptest// close (NNNN) = NNNN

    /* Limit testing. */
    fanotify_init(-1, O_RDONLY);
    //staptest// fanotify_init (XXXX|FAN_[^ ]+|XXXX, O_RDONLY) = -NNNN

    // Here's we're passing an invalid flags value (we hope) to make
    // sure this fails.
    fanotify_init(0x80000000, -1);
    //staptest// fanotify_init (FAN_CLASS_NOTIF|0x80000000, O_[^ ]+|XXXX) = -NNNN

    fanotify_mark(-1, FAN_MARK_REMOVE, FAN_ACCESS, AT_FDCWD, fname);
    //staptest// fanotify_mark (-1, FAN_MARK_REMOVE, FAN_ACCESS, AT_FDCWD, "testfile") = -NNNN

    fanotify_mark(-1, -1, FAN_MODIFY, AT_FDCWD, fname);
    //staptest// fanotify_mark (-1, FAN_[^ ]+|XXXX, FAN_MODIFY, AT_FDCWD, "testfile") = -NNNN

    fanotify_mark(-1, FAN_MARK_FLUSH, -1, AT_FDCWD, fname);
    //staptest// fanotify_mark (-1, FAN_MARK_FLUSH, FAN_[^ ]+|XXXX, AT_FDCWD, "testfile") = -NNNN

    fanotify_mark(-1, FAN_MARK_ADD|FAN_MARK_DONT_FOLLOW, FAN_CLOSE_WRITE, -1LL, fname);
    //staptest// fanotify_mark (-1, FAN_MARK_ADD|FAN_MARK_DONT_FOLLOW, FAN_CLOSE_WRITE, -1, "testfile") = -NNNN

    fanotify_mark(-1, FAN_MARK_ADD|FAN_MARK_MOUNT, FAN_CLOSE_NOWRITE, AT_FDCWD, (char *)-1);
#ifdef __s390__
    //staptest// fanotify_mark (-1, FAN_MARK_ADD|FAN_MARK_MOUNT, FAN_CLOSE_NOWRITE, AT_FDCWD, 0x[7]?[f]+) = -NNNN
#else
    //staptest// fanotify_mark (-1, FAN_MARK_ADD|FAN_MARK_MOUNT, FAN_CLOSE_NOWRITE, AT_FDCWD, 0x[f]+) = -NNNN
#endif

    return 0;
}

#else
int main()
{
    return 0;
}
#endif
