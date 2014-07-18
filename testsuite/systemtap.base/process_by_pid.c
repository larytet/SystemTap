#include "sys/sdt.h"

void sleeper () {}

int main () {
    while (1) {  
        sleeper();
        marker_here:
            STAP_PROBE(tmp_test_file, while_start);
    }
    return 0;
}
