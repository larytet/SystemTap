/* COVERAGE: tee splice vmsplice */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

#define TEST_BLOCK_SIZE (1<<17) /* 128K */

int main() {

    int pipes[2];
    struct iovec v;
    static char buffer[TEST_BLOCK_SIZE];
    v.iov_base = buffer;
    v.iov_len = TEST_BLOCK_SIZE;
    pipe(pipes);

    // ------- tee ----------

    tee(0, 0, 0, 0);
    //staptest// tee (0, 0, 0, 0x0) = 0

    tee(-1, 0, 0, 0);
    //staptest// tee (-1, 0, 0, 0x0)

    tee(0, -1, 0, 0);
    //staptest// tee (0, -1, 0, 0x0)

    tee(0, 0, (unsigned long)-1, 0);
#if __WORDSIZE == 64
    //staptest// tee (0, 0, 18446744073709551615, 0x0)
#else
    //staptest// tee (0, 0, 4294967295, 0x0)
#endif

    tee(0, 0, 0, -1);
    //staptest// tee (0, 0, 0, 0x[f]+)

    // ------- splice -------

    splice(0, NULL, 0, NULL, 0, SPLICE_F_MOVE);
    //staptest// splice (0, 0x0, 0, 0x0, 0, SPLICE_F_MOVE) = 0

    splice(-1, NULL, 0, NULL, 0, SPLICE_F_MOVE);
    //staptest// splice (-1, 0x0, 0, 0x0, 0, SPLICE_F_MOVE)

    splice(0, (loff_t *)-1, 0, NULL, 0, SPLICE_F_MOVE);
#ifdef __s390__
    //staptest// splice (0, 0x[7]?[f]+, 0, 0x0, 0, SPLICE_F_MOVE)
#else
    //staptest// splice (0, 0x[f]+, 0, 0x0, 0, SPLICE_F_MOVE)
#endif

    splice(0, NULL, -1, NULL, 0, SPLICE_F_MOVE);
    //staptest// splice (0, 0x0, -1, 0x0, 0, SPLICE_F_MOVE)

    splice(0, NULL, 0, (loff_t *)-1, 0, SPLICE_F_MOVE);
#ifdef __s390__
    //staptest// splice (0, 0x0, 0, 0x[7]?[f]+, 0, SPLICE_F_MOVE)
#else
    //staptest// splice (0, 0x0, 0, 0x[f]+, 0, SPLICE_F_MOVE)
#endif

    splice(0, NULL, 0, NULL, -1, SPLICE_F_MOVE);
#if __WORDSIZE == 64
    //staptest// splice (0, 0x0, 0, 0x0, 18446744073709551615, SPLICE_F_MOVE)
#else
    //staptest// splice (0, 0x0, 0, 0x0, 4294967295, SPLICE_F_MOVE)
#endif

    splice(0, NULL, 0, NULL, 0, -1);
    //staptest// splice (0, 0x0, 0, 0x0, 0, SPLICE_F_MOVE|SPLICE_F_NONBLOCK|SPLICE_F_MORE|SPLICE_F_GIFT|XXXX)


    // ------- vmsplice -----

    vmsplice(pipes[1], &v, 1, SPLICE_F_MOVE);
    //staptest// vmsplice (NNNN, XXXX, 1, SPLICE_F_MOVE) = NNNN

    vmsplice(-1, &v, 1, SPLICE_F_MOVE);
    //staptest// vmsplice (-1, XXXX, 1, SPLICE_F_MOVE)

    vmsplice(pipes[1], (const struct iovec *)-1, 1, SPLICE_F_MOVE);
#ifdef __s390__
    //staptest// vmsplice (NNNN, 0x[7]?[f]+, 1, SPLICE_F_MOVE)
#else
    //staptest// vmsplice (NNNN, 0x[f]+, 1, SPLICE_F_MOVE)
#endif

    vmsplice(pipes[1], &v, -1, SPLICE_F_MOVE);
#if __WORDSIZE == 64
    //staptest// vmsplice (NNNN, XXXX, 18446744073709551615, SPLICE_F_MOVE)
#else
    //staptest// vmsplice (NNNN, XXXX, 4294967295, SPLICE_F_MOVE)
#endif

    vmsplice(pipes[1], &v, 1, -1);
    //staptest// vmsplice (NNNN, XXXX, 1, SPLICE_F_MOVE|SPLICE_F_NONBLOCK|SPLICE_F_MORE|SPLICE_F_GIFT|XXXX)

    return 0;
}
