/* COVERAGE: io_setup io_submit io_getevents io_cancel io_destroy */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>


int main() {
	aio_context_t ctx;
	struct iocb cb;
	struct iocb *cbs[1];
	char data[8];
	struct io_event events[1];
	int ret;
	int fd;

	fd = open("testfile", O_RDWR | O_CREAT, 0644);
	ctx = 0;

	// ----- test normal operation

	ret = syscall(__NR_io_setup, 128, &ctx);
	//staptest// io_setup (128, XXXX) = NNNN
	if (ret < 0) {
		perror("io_setup failed");
		return -1;
	}

	strcpy(data, "testdata");
	memset(&cb, 0, sizeof(cb));
	cb.aio_fildes = fd;
	cb.aio_lio_opcode = IOCB_CMD_PWRITE;
	cb.aio_buf = (unsigned long)data;
	cb.aio_offset = 0;
	cb.aio_nbytes = 8;
	cbs[0] = &cb;

	ret = syscall(__NR_io_submit, ctx, 1, cbs);
        //staptest// io_submit (NNNN, 1, XXXX) = NNNN
	if (ret != 1) {
		perror("io_sumbit failed");
		return  -1;
	}

	ret = syscall(__NR_io_getevents, ctx, 1, 1, events, NULL);
        //staptest// io_getevents (NNNN, 1, 1, XXXX, NULL) = NNNN
	printf("%d\n", ret);

	ret = syscall(__NR_io_destroy, ctx);
        //staptest// io_destroy (NNNN) = NNNN
	if (ret < 0) {
		perror("io_destroy failed");
		return -1;
	}


	// ----- test nasty things

        syscall(__NR_io_setup, (unsigned)-1, &ctx);
        //staptest// io_setup (4294967295, XXXX) = NNNN

        syscall(__NR_io_setup, 1, (aio_context_t *)-1);
#ifdef __s390__
        //staptest// io_setup (1, 0x[7]?[f]+) = NNNN
#else
        //staptest// io_setup (1, 0x[f]+) = NNNN
#endif

        syscall(__NR_io_submit, (aio_context_t)-1, 1, cbs);
#if __WORDSIZE == 64
        //staptest// io_submit (18446744073709551615, 1, XXXX) = NNNN
#else
        //staptest// io_submit (4294967295, 1, XXXX) = NNNN
#endif

        syscall(__NR_io_submit, 0, (long)-1, cbs);
        //staptest// io_submit (0, -1, XXXX) = NNNN

        syscall(__NR_io_submit, 0, 1, (struct iocb **)-1);
#ifdef __s390__
        //staptest// io_submit (0, 1, 0x[7]?[f]+) = NNNN
#else
        //staptest// io_submit (0, 1, 0x[f]+) = NNNN
#endif

        syscall(__NR_io_getevents, (aio_context_t)-1, 1, 1, events, NULL);
#if __WORDSIZE == 64
        //staptest// io_getevents (18446744073709551615, 1, 1, XXXX, NULL) = NNNN
#else
        //staptest// io_getevents (4294967295, 1, 1, XXXX, NULL) = NNNN
#endif

        syscall(__NR_io_getevents, 0, (long)-1, 1, events, NULL);
        //staptest// io_getevents (0, -1, 1, XXXX, NULL) = NNNN

        syscall(__NR_io_getevents, 0, 1, (long)-1, events, NULL);
        //staptest// io_getevents (0, 1, -1, XXXX, NULL) = NNNN

        syscall(__NR_io_getevents, 0, 1, 1, (struct io_event *)-1, NULL);
#ifdef __s390__
        //staptest// io_getevents (0, 1, 1, 0x[7]?[f]+, NULL) = NNNN
#else
        //staptest// io_getevents (0, 1, 1, 0x[f]+, NULL) = NNNN
#endif

        syscall(__NR_io_getevents, 0, 1, 1, events, (struct timespec *)-1);
#ifdef __s390__
        //staptest// io_getevents (0, 1, 1, XXXX, 0x[7]?[f]+) = NNNN
#else
        //staptest// io_getevents (0, 1, 1, XXXX, 0x[f]+) = NNNN
#endif

        syscall(__NR_io_cancel, 1, (struct iocb *)1, (struct io_event *)1);
        //staptest// io_cancel (1, 0x1, 0x1) = NNNN

        syscall(__NR_io_cancel, (aio_context_t)-1, (struct iocb *)1, (struct io_event *)1);
#if __WORDSIZE == 64
        //staptest// io_cancel (18446744073709551615, 0x1, 0x1) = NNNN
#else
        //staptest// io_cancel (4294967295, 0x1, 0x1) = NNNN
#endif

        syscall(__NR_io_cancel, 1, (struct iocb *)-1, (struct io_event *)1);
#ifdef __s390__
        //staptest// io_cancel (1, 0x[7]?[f]+, 0x1) = NNNN
#else
        //staptest// io_cancel (1, 0x[f]+, 0x1) = NNNN
#endif

        syscall(__NR_io_cancel, 1, (struct iocb *)1, (struct io_event *)-1);
#ifdef __s390__
        //staptest// io_cancel (1, 0x1, 0x[7]?[f]+) = NNNN
#else
        //staptest// io_cancel (1, 0x1, 0x[f]+) = NNNN
#endif

	ret = syscall(__NR_io_destroy, (aio_context_t)-1);
#if __WORDSIZE == 64
        //staptest// io_destroy (18446744073709551615) = NNNN
#else
        //staptest// io_destroy (4294967295) = NNNN
#endif

	close(fd);

	return 0;
}



