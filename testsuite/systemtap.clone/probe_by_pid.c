#include <stdio.h>
#include <unistd.h>

void test_function(void)
{
    (void) getpid();
    sleep(1);
}

int main(void)
{
    sleep(1); // so that post-prologue doesn't fall inside loop
    while (1) {
	test_function();
    }
}
