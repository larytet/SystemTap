#include "sys/sdt.h"
#include <unistd.h>

void sleeper () {
  sleep(1);
}

int main () {
    while (1) {  
        sleeper();
        marker_here:
            STAP_PROBE(tmp_test_file, while_start);
    }
    if (0) // suppress gcc warning about unused label
      goto marker_here;
    return 0;
}
