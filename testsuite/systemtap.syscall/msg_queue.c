/* COVERAGE: msgget msgsnd msgrcv msgctl */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
     
#define MSGTXTLEN 128   // msg text length

struct msg_buf {
    long mtype;
    char mtext[MSGTXTLEN];
} msg;

int
main(int argc, char **argv)
{
    int msgqid;

    // Test basic operation.

    // create a message queue.
    msgqid = msgget(IPC_PRIVATE, IPC_CREAT|IPC_EXCL|0600);
    //staptest// msgget (IPC_PRIVATE, IPC_CREAT|IPC_EXCL|0600) = NNNN
  
    // create a message to send
    msg.mtype = 1;
    strcpy(msg.mtext, "a text msg...\n");

    // send the message to queue
    msgsnd(msgqid, &msg, sizeof(msg), IPC_NOWAIT);
    //staptest// msgsnd (NNNN, XXXX, NNNN, IPC_NOWAIT) = NNNN

    // read the message from queue
    msgrcv(msgqid, &msg, sizeof(msg), 0, 0); 
    //staptest// msgrcv (NNNN, XXXX, NNNN, 0, 0x0) = NNNN

    // remove the queue
    msgctl(msgqid, IPC_RMID, NULL);
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = 0

    // Range checking.

    msgqid = msgget((key_t)-1, IPC_CREAT|IPC_EXCL|0600);
    //staptest// msgget (-1, IPC_CREAT|IPC_EXCL|0600) = NNNN

    msgsnd(-1, &msg, sizeof(msg), 0);
    //staptest// msgsnd (-1, XXXX, NNNN, 0x0) = NNNN

    msgsnd(msgqid, (void *)-1, sizeof(msg), 0);
#ifdef __s390__
    //staptest// msgsnd (NNNN, 0x[7]?[f]+, NNNN, 0x0) = NNNN
#else
    //staptest// msgsnd (NNNN, 0x[f]+, NNNN, 0x0) = NNNN
#endif

    msgsnd(msgqid, &msg, (size_t)-1, 0);
#if __WORDSIZE == 64
    //staptest// msgsnd (NNNN, XXXX, 18446744073709551615, 0x0) = NNNN
#else
    //staptest// msgsnd (NNNN, XXXX, 4294967295, 0x0) = NNNN
#endif

    msgsnd(msgqid, &msg, sizeof(msg), -1);
    //staptest// msgsnd (NNNN, XXXX, NNNN, [[[[MSG!!!!IPC]]]]_[^ ]+|XXXX) = NNNN

    msgrcv(-1, &msg, sizeof(msg), 0, 0); 
    //staptest// msgrcv (-1, XXXX, NNNN, 0, 0x0) = NNNN

    msgrcv(msgqid, (void *)-1, sizeof(msg), 0, 0); 
#ifdef __s390__
    //staptest// msgrcv (NNNN, 0x[7]?[f]+, NNNN, 0, 0x0) = NNNN
#else
    //staptest// msgrcv (NNNN, 0x[f]+, NNNN, 0, 0x0) = NNNN
#endif

    msgrcv(msgqid, &msg, (size_t)-1, 0, 0); 
#if __WORDSIZE == 64
    //staptest// msgrcv (NNNN, XXXX, 18446744073709551615, 0, 0x0) = NNNN
#else
    //staptest// msgrcv (NNNN, XXXX, 4294967295, 0, 0x0) = NNNN
#endif

    msgrcv(msgqid, &msg, sizeof(msg), -1, IPC_NOWAIT); 
    //staptest// msgrcv (NNNN, XXXX, NNNN, -1, IPC_NOWAIT) = NNNN

    msgrcv(msgqid, &msg, sizeof(msg), 0, -1); 
    //staptest// msgrcv (NNNN, XXXX, NNNN, 0, [[[[MSG!!!!IPC]]]]_[^ ]+|XXXX) = NNNN

    msgctl(msgqid, IPC_RMID, NULL);
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = NNNN

    msgqid = msgget(IPC_PRIVATE, -1);
    //staptest// msgget (IPC_PRIVATE, IPC_[^ ]+|XXXX|0777) = NNNN

    msgctl(msgqid, IPC_RMID, NULL);
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x0) = NNNN

    msgctl(msgqid, -1, NULL);
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?XXXX, 0x0) = NNNN

    msgctl(msgqid, IPC_RMID, (struct msqid_ds *)-1);
#ifdef __s390__
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x[7]?[f]+) = NNNN
#else
    //staptest// msgctl (NNNN, [[[[IPC_64|]]]]?IPC_RMID, 0x[f]+) = NNNN
#endif

    return 0;
}
