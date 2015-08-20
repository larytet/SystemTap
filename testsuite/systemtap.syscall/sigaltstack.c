/* COVERAGE: sigaltstack */

#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

// To test for glibc support for sigaltstack():
#define GLIBC_SUPPORT \
    (_BSD_SOURCE || _XOPEN_SOURCE >= 500 || \
     _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED \
     || /* Since glibc 2.12: */ _POSIX_C_SOURCE >= 200809L)

static void 
sig_act_handler(int signo)
{
}

int main()
{
#ifdef GLIBC_SUPPORT
    stack_t ss;
    struct sigaction act, oact;
    sigset_t mask;

    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = SS_ONSTACK;

    /* Set up an alternate signal stack. */
    sigaltstack(&ss, NULL);
    // Notice we can't test for an exact ss_size, since SIGSTKSZ might
    // vary on different architectures.
    //staptest// sigaltstack ({ss_sp=XXXX, ss_flags=SS_ONSTACK, ss_size=NNNN}, 0x0) = 0
    
    /* Set up the signal handler for 'SIGALRM'. Notice the SA_ONSTACK
     * flag, which tells the system to use the alternate signal
     * stack. Also note that glibc on some platforms likes to add in
     * the SA_RESTORER flag. */
    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_ONSTACK;
    act.sa_handler = (void (*)())sig_act_handler;
    sigaction(SIGALRM, &act, &oact);
#ifdef __ia64__
    //staptest// rt_sigaction (SIGALRM, {XXXX, SA_ONSTACK, \[EMPTY\]}, XXXX, 8) = 0
#else
    //staptest// rt_sigaction (SIGALRM, {XXXX, SA_ONSTACK[[[[|SA_RESTORER]]]]?, XXXX, \[EMPTY\]}, XXXX, 8) = 0
#endif

    alarm(5);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
    //staptest// setitimer (ITIMER_REAL, \[0.000000,5.000000\], XXXX) = NNNN
#else
    //staptest// alarm (5) = NNNN
#endif

    sigemptyset(&mask);
    sigsuspend(&mask);
    //staptest// rt_sigsuspend (\[EMPTY\], 8) = NNNN

    alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
    //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = NNNN
#else
    //staptest// alarm (0) = NNNN
#endif

    /* Limit testing. */
    sigaltstack((stack_t *)-1, NULL);
#ifdef __s390__
    //staptest// sigaltstack (0x[7]?[f]+, 0x0) = NNNN
#else
    //staptest// sigaltstack (0x[f]+, 0x0) = NNNN
#endif

    sigaltstack(NULL, (stack_t *)-1);
#ifdef __s390__
    //staptest// sigaltstack (0x0, 0x[7]?[f]+) = NNNN
#else
    //staptest// sigaltstack (0x0, 0x[f]+) = NNNN
#endif
#endif
    return 0;
}
