/* COVERAGE: getsid */
#include <unistd.h>


int main() {


    // --- successful call ---

    getsid(1);
    //staptest// getsid (1) = 1


    // --- nasty call ---

    getsid(-1);
    //staptest// getsid (-1) = -3 (ESRCH)


    return 0;
}
