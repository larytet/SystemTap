/* COVERAGE: semop semtimedop */

#define _GNU_SOURCE
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

int main()
{
    char *curdir;
    struct timeval tv;
    key_t ipc_key;
    int semid;
    struct sembuf sops[10];
    struct timespec ts;

    // Generate a System V IPC key (based on the current directory and time)
    curdir = getcwd(NULL, 0);
    gettimeofday(&tv, NULL);
    ipc_key = ftok(curdir, tv.tv_usec);

    // Create a set of 10 semaphores.
    semid = semget(ipc_key, 10, 0666 | IPC_CREAT);
    //staptest// semget (NNNN, 10, IPC_CREAT|0666) = NNNN

    // Since:
    //   semop(X, Y, Z)
    // can be implemented by
    //   semtimedop(X, Y, Z, NULL)
    // We have to accept both in the output.

    // Modify the first 2 semaphores with semop().
    sops[0].sem_num = 0;
    sops[0].sem_op = 1;
    sops[0].sem_flg = IPC_NOWAIT;
    sops[1].sem_num = 1;
    sops[1].sem_op = 1;
    sops[1].sem_flg = 0;
    semop(semid, sops, 2);
    //staptest// [[[[semop (NNNN, XXXX, 2)!!!!semtimedop (NNNN, XXXX, 2, NULL)]]]] = 0

    sops[0].sem_num = 2;
    sops[0].sem_op = 1;
    sops[0].sem_flg = IPC_NOWAIT;
    sops[1].sem_num = 3;
    sops[1].sem_op = 1;
    sops[1].sem_flg = 0;
    semtimedop(semid, sops, 2, NULL);
    //staptest// semtimedop (NNNN, XXXX, 2, NULL) = 0

    // Modify the fifth semaphore with semtimedop().
    sops[0].sem_num = 5;
    sops[0].sem_op = 1;
    sops[0].sem_flg = IPC_NOWAIT;
    ts.tv_sec = 5;
    ts.tv_nsec = 0;
    semtimedop(semid, sops, 1, &ts);
    //staptest// semtimedop (NNNN, XXXX, 1, \[5.000000000\]) = 0

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    // Create a set of 10 semaphores.
    semid = semget(IPC_PRIVATE, 10, 0666 | IPC_CREAT);
    //staptest// semget (IPC_PRIVATE, 10, IPC_CREAT|0666) = NNNN

    // Range checking.
    semop(-1, sops, 1);
    //staptest// sem[[[[timed]]]]?op (-1, XXXX, 1[[[[, NULL]]]]?) = NNNN

    semop(semid, (struct sembuf *)-1, 1);
#ifdef __s390__
    //staptest// sem[[[[timed]]]]?op (NNNN, 0x[7]?[f]+, 1[[[[, NULL]]]]?) = -NNNN
#else
    //staptest// sem[[[[timed]]]]?op (NNNN, 0x[f]+, 1[[[[, NULL]]]]?) = -NNNN
#endif

    semop(semid, NULL, -1);
    //staptest// sem[[[[timed]]]]?op (NNNN, 0x0, 4294967295[[[[, NULL]]]]?) = -NNNN

    semtimedop(-1, sops, 1, &ts);
    //staptest// semtimedop (-1, XXXX, 1, \[5.000000000\]) = NNNN

    semtimedop(semid, (struct sembuf *)-1, 1, &ts);
#ifdef __s390__
    //staptest// semtimedop (NNNN, 0x[7]?[f]+, 1, \[5.000000000\]) = NNNN
#else
    //staptest// semtimedop (NNNN, 0x[f]+, 1, \[5.000000000\]) = NNNN
#endif

    semtimedop(semid, NULL, -1, &ts);
    //staptest// semtimedop (NNNN, 0x0, 4294967295, \[5.000000000\]) = NNNN

    semtimedop(semid, sops, 1, (struct timespec *)-1);
#ifdef __s390__
    //staptest// semtimedop (NNNN, XXXX, 1, 0x[7]?[f]+) = NNNN
#else
    //staptest// semtimedop (NNNN, XXXX, 1, 0x[f]+) = NNNN
#endif

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    return 0;
}
