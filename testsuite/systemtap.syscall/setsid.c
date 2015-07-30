/* COVERAGE: setsid */

#include <unistd.h>

int main()
{
    setsid();
    //staptest// setsid () = NNNN

    return 0;
}
