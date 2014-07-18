#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
 
int main()
{
    kill(getpid(), SIGSTOP);
    getpid();
    return 0;
}
