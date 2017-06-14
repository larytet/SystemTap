/* COVERAGE: mq_close mq_getattr mq_getsetattr mq_notify mq_open mq_receive mq_send mq_setattr mq_timedreceive mq_timedsend mq_unlink */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <sys/syscall.h>

#define QUEUE_NAME  "/test_queue"
#define MSG_LEN     7
#define MSG         "message"

static void sighandler(int signum) {
    // It appears that sometimes this printf output can confuse the
    // testsuite, so let's avoid it.
#if 0
    printf("Received: NOTIFICATION\n");
#endif
}

static inline int stp_mq_getsetattr(mqd_t mqdes, struct mq_attr *newattr,
				    struct mq_attr *oldattr)
{
    return syscall(__NR_mq_getsetattr, mqdes, newattr, oldattr);
}

static inline int stp_mq_notify(mqd_t mqdes, const struct sigevent *sevp)
{
    return syscall(__NR_mq_notify, mqdes, sevp);
}

static inline int stp_mq_unlink(const char *name)
{
    return syscall(__NR_mq_unlink, name);
}

int main() {

    mqd_t mq_server, mq_server_valid, mq_client, mq;
    struct mq_attr attr;
    char buffer[MSG_LEN + 1];
    ssize_t bytes_read;
    struct sigevent sev;
    struct timespec tsp;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MSG_LEN;
    attr.mq_curmsgs = 0;




    // ------- test normal operation

    mq_server = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    // Occasionally QUEUE_NAME isn't paged in yet here, so accept a
    // string or an address. By the time of the next mq_open, it has
    // been paged in.
    //staptest// mq_open ([[[["test_queue"!!!!XXXX]]]], O_RDONLY|O_CREAT, 0644, XXXX) = NNNN

    stp_mq_getsetattr(mq_server, &attr, &attr);
    //staptest// mq_getsetattr (NNNN, XXXX, XXXX) = 0

    signal(SIGUSR1, sighandler);
    sev.sigev_signo = SIGUSR1;
    sev.sigev_notify = SIGEV_SIGNAL;
    mq_notify(mq_server, &sev);
    //staptest// mq_notify (NNNN, XXXX) = 0

    mq_client = mq_open(QUEUE_NAME, O_WRONLY);
    //staptest// mq_open ("test_queue", O_WRONLY) = NNNN

    mq_send(mq_client, MSG, MSG_LEN, 0);
    //staptest// mq_timedsend (NNNN, XXXX, NNNN, 0, XXXX) = 0

    mq_close(mq_client);
    //staptest// close (NNNN) = 0

    bytes_read = mq_receive(mq_server, buffer, MSG_LEN, NULL);
    //staptest// mq_timedreceive (NNNN, XXXX, NNNN, 0x0, 0x0) = NNNN

    buffer[bytes_read] = '\0';
    printf("Received: %s\n", buffer);




    // ------- test nasty things

    mq_server_valid = mq_server;
    mq_server = -1;

    mq_open(QUEUE_NAME, (int)-1, 0644, &attr);
    //staptest// mq_open ("test_queue", O_RDONLY|O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_APPEND|O_NONBLOCK|O_SYNC|O_ASYNC|O_DIRECT|O_LARGEFILE|O_DIRECTORY|O_NOFOLLOW|O_NOATIME[[[[|O_CLOEXEC]]]]?[[[[|O_PATH]]]]?|XXXX, 0644, XXXX) = NNNN (EEXIST)

    mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, (mode_t)-1, &attr);
#if __WORDSIZE == 64
    //staptest// mq_open ("test_queue", O_RDONLY|O_CREAT, 037777777777, XXXX) = NNNN
#else
    // 32-on-64 gets the mode value passed in 16-bit 'compat_mode_t',
    //staptest// mq_open ("test_queue", O_RDONLY|O_CREAT, [[[[0177777!!!!037777777777]]]], XXXX) = NNNN
#endif

    mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, -1);
#ifdef __s390__
    //staptest// mq_open ("test_queue", O_RDONLY|O_CREAT, 0644, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_open ("test_queue", O_RDONLY|O_CREAT, 0644, 0x[f]+) = NNNN
#endif

    stp_mq_getsetattr(mq_server, &attr, &attr);
    //staptest// mq_getsetattr (NNNN, XXXX, XXXX) = NNNN

    stp_mq_getsetattr(-1, &attr, &attr);
    //staptest// mq_getsetattr (-1, XXXX, XXXX) = NNNN

    stp_mq_getsetattr(mq_server, (struct mq_attr *)-1, &attr);
#ifdef __s390__
    //staptest// mq_getsetattr (NNNN, 0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// mq_getsetattr (NNNN, 0x[f]+, XXXX) = NNNN
#endif

    stp_mq_getsetattr(mq_server, &attr, (struct mq_attr *)-1);
#ifdef __s390__
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[f]+) = NNNN
#endif

    mq_notify(-1, &sev);
    //staptest// mq_notify (-1, XXXX) = NNNN

    stp_mq_notify(mq_server, (const struct sigevent *)-1);
#ifdef __s390__
    //staptest// mq_notify (NNNN, 0x[7]?[f]+) = NNNN (EFAULT)
#else
    //staptest// mq_notify (NNNN, 0x[f]+) = NNNN (EFAULT)
#endif

    mq_timedsend(-1, MSG, MSG_LEN, 0, &tsp);
    //staptest// mq_timedsend (-1, XXXX, 7, 0, XXXX) = NNNN

    mq_timedsend(mq_server, (const char *)-1, MSG_LEN, 0, &tsp);
#ifdef __s390__
    //staptest// mq_timedsend (NNNN, 0x[7]?[f]+, 7, 0, XXXX) = NNNN
#else
    //staptest// mq_timedsend (NNNN, 0x[f]+, 7, 0, XXXX) = NNNN
#endif

    mq_timedsend(mq_server, MSG, -1, 0, &tsp);
#if __WORDSIZE == 64
    //staptest// mq_timedsend (NNNN, XXXX, 18446744073709551615, 0, XXXX) = NNNN
#else
    //staptest// mq_timedsend (NNNN, XXXX, 4294967295, 0, XXXX) = NNNN
#endif

    mq_timedsend(mq_server, MSG, MSG_LEN, -1, &tsp);
    //staptest// mq_timedsend (NNNN, XXXX, 7, 4294967295, XXXX) = NNNN

    mq_timedsend(mq_server, MSG, MSG_LEN, 0, (const struct timespec *)-1);
#ifdef __s390__
    //staptest// mq_timedsend (NNNN, XXXX, 7, 0, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_timedsend (NNNN, XXXX, 7, 0, 0x[f]+) = NNNN
#endif

    mq_timedreceive(-1, MSG, MSG_LEN, 0, &tsp);
    //staptest// mq_timedreceive (-1, XXXX, 7, 0x0, XXXX) = NNNN

    mq_timedreceive(mq_server, (char *)-1, MSG_LEN, 0, &tsp);
#ifdef __s390__
    //staptest// mq_timedreceive (NNNN, 0x[7]?[f]+, 7, 0x0, XXXX) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, 0x[f]+, 7, 0x0, XXXX) = NNNN
#endif

    mq_timedreceive(mq_server, MSG, -1, 0, &tsp);
#if __WORDSIZE == 64
    //staptest// mq_timedreceive (NNNN, XXXX, 18446744073709551615, 0x0, XXXX) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, XXXX, 4294967295, 0x0, XXXX) = NNNN
#endif

    mq_timedreceive(mq_server, MSG, MSG_LEN, (unsigned *)-1, &tsp);
#ifdef __s390__
    //staptest// mq_timedreceive (NNNN, XXXX, 7, 0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, XXXX, 7, 0x[f]+, XXXX) = NNNN
#endif

    mq_timedreceive(mq_server, MSG, MSG_LEN, 0, (const struct timespec *)-1);
#ifdef __s390__
    //staptest// mq_timedreceive (NNNN, XXXX, 7, 0x0, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, XXXX, 7, 0x0, 0x[f]+) = NNNN
#endif

    mq_send(-1, MSG, MSG_LEN, 0);
    //staptest// mq_timedsend (-1, XXXX, NNNN, 0, XXXX) = NNNN

    mq_send(mq_server, (const char *)-1, MSG_LEN, 0);
#ifdef __s390__
    //staptest// mq_timedsend (NNNN, 0x[7]?[f]+, NNNN, 0, XXXX) = NNNN
#else
    //staptest// mq_timedsend (NNNN, 0x[f]+, NNNN, 0, XXXX) = NNNN
#endif

    mq_send(mq_server, MSG, -1, 0);
#if __WORDSIZE == 64
    //staptest// mq_timedsend (NNNN, XXXX, 18446744073709551615, 0, XXXX) = NNNN
#else
    //staptest// mq_timedsend (NNNN, XXXX, 4294967295, 0, XXXX) = NNNN
#endif

    mq_send(mq_server, MSG, MSG_LEN, -1);
    //staptest// mq_timedsend (NNNN, XXXX, NNNN, 4294967295, 0x0) = NNNN

    mq_receive(-1, buffer, MSG_LEN, NULL);
    //staptest// mq_timedreceive (-1, XXXX, NNNN, 0x0, 0x0) = NNNN

    mq_receive(mq_server, (char *)-1, MSG_LEN, NULL);
#ifdef __s390__
    //staptest// mq_timedreceive (NNNN, 0x[7]?[f]+, NNNN, 0x0, 0x0) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, 0x[f]+, NNNN, 0x0, 0x0) = NNNN
#endif

    mq_receive(mq_server, buffer, -1, NULL);
#if __WORDSIZE == 64
    //staptest// mq_timedreceive (NNNN, XXXX, 18446744073709551615, 0x0, 0x0) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, XXXX, 4294967295, 0x0, 0x0) = NNNN
#endif

    mq_receive(mq_server, buffer, MSG_LEN, (unsigned *)-1);
#ifdef __s390__
    //staptest// mq_timedreceive (NNNN, XXXX, NNNN, 0x[7]?[f]+, 0x0) = NNNN
#else
    //staptest// mq_timedreceive (NNNN, XXXX, NNNN, 0x[f]+, 0x0) = NNNN
#endif

    mq_getattr(-1, (struct mq_attr *)NULL);
    //staptest// mq_getsetattr (-1, XXXX, XXXX) = NNNN

    mq_getattr(mq_server, (struct mq_attr *)-1);
#ifdef __s390__
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[f]+) = NNNN
#endif

    mq_setattr(-1, (struct mq_attr *)NULL, (struct mq_attr *)NULL);
    //staptest// mq_getsetattr (-1, XXXX, XXXX) = NNNN

    mq_setattr(mq_server, (struct mq_attr *)-1, (struct mq_attr *)NULL);
#ifdef __s390__
    //staptest// mq_getsetattr (NNNN, 0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// mq_getsetattr (NNNN, 0x[f]+, XXXX) = NNNN
#endif

    mq_setattr(mq_server, (struct mq_attr *)NULL, (struct mq_attr *)-1);
#ifdef __s390__
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[7]?[f]+) = NNNN
#else
    //staptest// mq_getsetattr (NNNN, XXXX, 0x[f]+) = NNNN
#endif

    mq_close((mqd_t)-1);
    //staptest// close (-1) = NNNN

    // glibc wrapper would cause SEGV in case of bad file descriptor,
    // so we use syscall():
    stp_mq_unlink((const char *)-1);
#ifdef __s390__
    //staptest// mq_unlink (0x[7]?[f]+) = NNNN
#else
    //staptest// mq_unlink (0x[f]+) = NNNN
#endif

    mq_server = mq_server_valid;



    // ------- close the shop

    mq_close(mq_server);
    //staptest// close (NNNN) = 0

    mq_unlink(QUEUE_NAME);
    //staptest// mq_unlink ("test_queue") = 0

    return 0;
}
