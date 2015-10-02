#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <signal.h>

int main(int argc, char **argv)
{
    DIR *rootdir;
    DIR *tmpdir;
    int pipefds[2];
    int sock;

    rootdir = opendir("/");
    tmpdir = opendir(argv[1]);
    pipe(pipefds);
    sock = socket(AF_UNIX, SOCK_STREAM, 0);

    close(sock);
    close(pipefds[0]);
    close(pipefds[1]);
    closedir(rootdir);
    closedir(tmpdir);
}
