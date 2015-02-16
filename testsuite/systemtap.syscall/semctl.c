/* COVERAGE: semctl */

#define _GNU_SOURCE
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

union semun
{
    int val;			    /* Value for SETVAL */
    struct semid_ds *buf;	    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;	    /* Array for GETALL, SETALL */
    struct seminfo  *__buf;	    /* Buffer for IPC_INFO (Linux-specific) */
};

int main()
{
    char *curdir;
    struct timeval tv;
    key_t ipc_key;
    int semid;
    union semun arg;
    struct semid_ds buf;
    struct seminfo info_buf;
    unsigned short sem_array[10];
    int i;

    // Generate a System V IPC key (based on the current directory and time)
    curdir = getcwd(NULL, 0);
    gettimeofday(&tv, NULL);
    ipc_key = ftok(curdir, tv.tv_usec);

    // Create a set of 10 semaphores.
    semid = semget(ipc_key, 10, 0666 | IPC_CREAT);
    //staptest// semget (NNNN, 10, IPC_CREAT|0666) = NNNN

    // IPC_STAT: Copy information about the semaphores into
    // arg.buf. The 2nd argument is ignored.
    arg.buf = &buf;
    semctl(semid, 0, IPC_STAT, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_STAT, XXXX) = NNNN

    // IPC_SET: Write the values of some members of semid_ds into the
    // kernel. The 2nd argument is ignored.
    buf.sem_perm.mode = 0777;
    semctl(semid, 0, IPC_SET, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_SET, XXXX) = NNNN

    // IPC_INFO: Return info about semaphore limits.  The 2nd argument
    // is ignored.
    arg.__buf = &info_buf;
    semctl(semid, 0, IPC_INFO, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_INFO, XXXX) = NNNN

    // SEM_INFO: Return info about semaphore limits.  The 2nd argument
    // is ignored.
    arg.__buf = &info_buf;
    semctl(semid, 0, SEM_INFO, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?SEM_INFO, XXXX) = NNNN

    // SEM_STAT...

    // GETALL: Return current value for all semaphores. The 2nd
    // argument is ignored.
    arg.array = sem_array;
    semctl(semid, 0, GETALL, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?GETALL, XXXX) = 0

    // GETNCNT: Return number of processes waiting on the semnum-th semaphore.
    semctl(semid, 1, GETNCNT);
    //staptest// semctl (NNNN, 1, [[[[IPC_64|]]]]?GETNCNT, XXXX) = 0

    // GETPID

    // GETVAL: Return semval for the semnum-th semaphore.
    semctl(semid, 2, GETVAL);
    //staptest// semctl (NNNN, 2, [[[[IPC_64|]]]]?GETVAL, XXXX) = NNNN

    // GETZCNT

    // SETALL: Set the value for all the semaphores in the set. The
    // 2nd argument is ignored.
    for (i = 0; i < 10; i++)
	sem_array[i] = 1;
    arg.array = sem_array;
    semctl(semid, 0, SETALL, arg);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?SETALL, XXXX) = 0
    
    // SETVAL: Set the value for the semnum-th semaphore.
    arg.val = 0;
    semctl(semid, 3, SETVAL, arg);
    //staptest// semctl (NNNN, 3, [[[[IPC_64|]]]]?SETVAL, XXXX) = 0

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    // Create a set of 10 semaphores.
    semid = semget(IPC_PRIVATE, 10, 0666 | IPC_CREAT);
    //staptest// semget (IPC_PRIVATE, 10, IPC_CREAT|0666) = NNNN

    // Range checking.
    semctl(-1, 0, IPC_STAT, arg);
    //staptest// semctl (-1, 0, [[[[IPC_64|]]]]?IPC_STAT, XXXX) = -NNNN

    semctl(semid, -1, GETPID);
    //staptest// semctl (NNNN, -1, [[[[IPC_64|]]]]?GETPID, XXXX) = -NNNN

    semctl(semid, 0, -1);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?XXXX, XXXX) = -NNNN

    // glibc is messing around with this one on compat systems, so
    // don't bother.
    //semctl(-1, 0, IPC_STAT, (union semun)-1);

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    return 0;
}
