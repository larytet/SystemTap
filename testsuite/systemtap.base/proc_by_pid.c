#include "stdio.h"
#include "sys/sdt.h"
#include <unistd.h>

void first_function() {}
void second_function() {first_function();}
void exit_probe_function() {}

int main()
{
    sleep(30);
    STAP_PROBE(proc_by_pid, main_start);
    marker_here:
      first_function();
    STAP_PROBE(proc_by_pid, main_end);
    second_function();
    exit_probe_function();
    return 0;
}
