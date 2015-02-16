/* COVERAGE: semget */

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

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    // Create a set of 10 semaphores.
    semid = semget(IPC_PRIVATE, 10, 0666 | IPC_CREAT);
    //staptest// semget (IPC_PRIVATE, 10, IPC_CREAT|0666) = NNNN

    // Delete the set of semaphores (the 2nd argument is ignored).
    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = 0

    // Range checking
    semid = semget(-1, 10, 0666 | IPC_CREAT);
    //staptest// semget (-1, 10, IPC_CREAT|0666) = NNNN

    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = NNNN

    semid = semget(IPC_PRIVATE, -1, 0666 | IPC_CREAT);
    //staptest// semget (IPC_PRIVATE, -1, IPC_CREAT|0666) = NNNN

    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = NNNN

    semid = semget(IPC_PRIVATE, 10, -1);
    //staptest// semget (IPC_PRIVATE, 10, IPC_[^ ]+|XXXX|0777) = NNNN

    semctl(semid, 0, IPC_RMID);
    //staptest// semctl (NNNN, 0, [[[[IPC_64|]]]]?IPC_RMID, XXXX) = NNNN

    return 0;
}
