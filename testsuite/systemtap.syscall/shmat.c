/* COVERAGE: shmat shmdt */
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
    void *addr;

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

    addr = shmat(shmid, NULL, SHM_RDONLY);
    //staptst// shmat (NNNN, 0x0, SHM_RDONLY) = NNNN
    
    addr = shmat(shmid, addr, SHM_REMAP);
    //staptst// shmat (NNNN, XXXX, SHM_REMAP) = NNNN

    shmdt(addr);
    //staptest// shmdt (XXXX) = NNNN

    shmat(-1, NULL, SHM_RND);
    //staptst// shmat (NNNN, 0x0, SHM_RND) = -NNNN
    
    shmat(shmid, (void *)-1, SHM_RND);
    //staptst// shmat (NNNN, 0xf000, SHM_RND) = NNNN

    shmdt((void *)-1);
#ifdef __s390__
    //staptest// shmdt (0x[7]?[f]+) = NNNN
#else
    //staptest// shmdt (0x[f]+) = NNNN
#endif

    shmctl(shmid, IPC_RMID, NULL);
    //staptest// shmctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = NNNN

    return 0;
}
