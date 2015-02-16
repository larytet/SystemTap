/* COVERAGE: shmget shmctl */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

int main()
{
    char *curdir;
    struct timeval tv;
    key_t ipc_key;
    int shmid;

    // Generate a System V IPC key (based on the current directory and time)
    curdir = getcwd(NULL, 0);
    gettimeofday(&tv, NULL);
    ipc_key = ftok(curdir, tv.tv_usec);

    // Why 64k? ppc64 has 64K pages. ia64 has 16k pages. x86_64/i686
    // has 4k pages. When we specify a size to shmget(), it must be a
    // multiple of the page size, so we use the biggest.
    //
    // The least significant 9 bites of the flags argument are the
    // permissions. 0600 is read/write permissions.
    shmid = shmget(ipc_key, 65535, (IPC_CREAT | IPC_EXCL | 0600));
    //staptest// shmget (NNNN, 65535, IPC_CREAT|IPC_EXCL|0600) = NNNN

    shmget(ipc_key, -1, (IPC_CREAT | IPC_EXCL | 0600));
#if __WORDSIZE == 64
    //staptest// shmget (NNNN, 18446744073709551615, IPC_CREAT|IPC_EXCL|0600) = -NNNN
#else
    //staptest// shmget (NNNN, 4294967295, IPC_CREAT|IPC_EXCL|0600) = -NNNN
#endif

    shmget(ipc_key, 65535, -1);
    //staptest// shmget (NNNN, 65535, IPC_[^ ]+|XXXX|0777) = -NNNN

    shmctl(shmid, IPC_RMID, NULL);
    //staptest// shmctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = NNNN

    shmid = shmget(-1, 65535, (IPC_CREAT | IPC_EXCL | 0600));
    //staptest// shmget (-1, 65535, IPC_CREAT|IPC_EXCL|0600) = NNNN

    shmctl(-1, IPC_RMID, NULL);
    //staptest// shmctl (-1, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = -NNNN

    shmctl(shmid, -1, NULL);
    //staptest// shmctl (NNNN, [[[[IPC_64|XXXX!!!!0x[f]+]]]], 0x0) = -NNNN

    shmctl(shmid, IPC_STAT, (struct shmid_ds *)-1);
#ifdef __s390__
    //staptest// shmctl (NNNN, [[[[IPC_64|]]]]?IPC_STAT, 0x[7]?[f]+) = -NNNN
#else
    //staptest// shmctl (NNNN, [[[[IPC_64|]]]]?IPC_STAT, 0x[f]+) = -NNNN
#endif

    shmctl(shmid, IPC_RMID, NULL);
    //staptest// shmctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = NNNN

    return 0;
}
