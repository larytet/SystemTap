#include "sys/sdt.h"

void sleeper () {
    sleep (5);
}

int main () {
    while (1) {  
        sleeper();
        marker_here:
            STAP_PROBE(tmp_test_file, while_start);
    }
    return 0;
}
