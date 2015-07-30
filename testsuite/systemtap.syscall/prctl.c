/* COVERAGE: prctl */
#include <string.h>
#include <sys/prctl.h>
#include <linux/prctl.h>
#include <linux/capability.h>
#include <linux/version.h>

#if defined(PR_SET_SECUREBITS) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
// PR_SET_SECUREBITS has existed since Linux 2.6.26. However, RHEL6
// doesn't have the associated /usr/include/linux/securebits.h include
// file. So, let's require at least a 3.0 kernel and hope that's
// reasonable to assume a /usr/include/linux/securebits.h file exists there.
#include <linux/securebits.h>
#endif

int main()
{
    char buffer[1024];
    int int_buffer;
    int *int_ptr;

    // PR_CAPBSET_READ (since Linux 2.6.25)
#ifdef PR_CAPBSET_READ
    prctl(PR_CAPBSET_READ, CAP_CHOWN, 0L, 0L, 0L);
    //staptest// prctl (PR_CAPBSET_READ, CAP_CHOWN) = NNNN
#endif

    // PR_CAPBSET_DROP (since Linux 2.6.25)
#ifdef PR_CAPBSET_DROP
    prctl(PR_CAPBSET_DROP, CAP_SETGID, 0L, 0L, 0L);
    //staptest// prctl (PR_CAPBSET_DROP, CAP_SETGID) = NNNN
#endif

    // PR_SET_CHILD_SUBREAPER (since Linux 3.4)
#ifdef PR_SET_CHILD_SUBREAPER
    prctl(PR_SET_CHILD_SUBREAPER, 1L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_CHILD_SUBREAPER, 1) = NNNN
#endif

    // PR_GET_CHILD_SUBREAPER (since Linux 3.4)
#ifdef PR_GET_CHILD_SUBREAPER
    prctl(PR_GET_CHILD_SUBREAPER, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_CHILD_SUBREAPER, XXXX) = NNNN
#endif

    // PR_SET_DUMPABLE (since Linux 2.3.20)
    prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_DUMPABLE, 0) = NNNN

    // PR_GET_DUMPABLE (since Linux 2.3.20)
    prctl(PR_GET_DUMPABLE, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_DUMPABLE) = NNNN

    // PR_SET_ENDIAN (since Linux 2.6.18, PowerPC only)
    // Since PR_SET_ENDIAN actually does something on PowerPC,
    // don't run it there.
#if defined(PR_SET_ENDIAN) && !(defined(__powerpc64__) || defined(__powerpc__))
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    prctl(PR_SET_ENDIAN, PR_ENDIAN_LITTLE, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_ENDIAN, PR_ENDIAN_LITTLE) = NNNN
#else
    prctl(PR_SET_ENDIAN, PR_ENDIAN_BIG, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_ENDIAN, PR_ENDIAN_BIG) = NNNN
#endif
#endif

    // PR_GET_ENDIAN (since Linux 2.6.18, PowerPC only)
#ifdef PR_GET_ENDIAN
    prctl(PR_GET_ENDIAN, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_ENDIAN, XXXX) = NNNN
#endif

    // PR_SET_FPEMU (since Linux 2.4.18, 2.5.9, only on ia64)
#ifdef PR_SET_FPEMU
    prctl(PR_SET_FPEMU, PR_FPEMU_NOPRINT, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_FPEMU, PR_FPEMU_NOPRINT) = NNNN
#endif

    // PR_GET_FPEMU (since Linux 2.4.18, 2.5.9, only on ia64)
#ifdef PR_GET_FPEMU
    prctl(PR_GET_FPEMU, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_FPEMU, XXXX) = NNNN
#endif

    // PR_SET_FPEXC (since Linux 2.4.21, 2.5.32, only on PowerPC)
    prctl(PR_SET_FPEXC, PR_FP_EXC_DISABLED, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_FPEXC, PR_FP_EXC_DISABLED) = NNNN

    // PR_GET_FPEXC (since Linux 2.4.21, 2.5.32, only on PowerPC)
    prctl(PR_GET_FPEXC, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_FPEXC, XXXX) = NNNN

    // PR_SET_KEEPCAPS (since Linux 2.2.18)
    prctl(PR_SET_KEEPCAPS, 1L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_KEEPCAPS, 1) = NNNN

    // PR_GET_KEEPCAPS (since Linux 2.2.18)
    prctl(PR_GET_KEEPCAPS, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_KEEPCAPS) = NNNN

    // PR_GET_NAME (since Linux 2.6.11)
    prctl(PR_GET_NAME, &buffer, 0L, 0L, 0L); 
    //staptest// prctl (PR_GET_NAME, XXXX) = 0

    // PR_SET_NAME (since Linux 2.6.9)
    // Notice we're setting the name to the same name we got from
    // PR_GET_NAME.
    prctl(PR_SET_NAME, &buffer, 0L, 0L, 0L); 
    //staptest// prctl (PR_SET_NAME, XXXX) = NNNN

    // PR_SET_NO_NEW_PRIVS (since Linux 3.5)
#ifdef PR_SET_NO_NEW_PRIVS
    prctl(PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_NO_NEW_PRIVS, 1) = NNNN
#endif

    // PR_GET_NO_NEW_PRIVS (since Linux 3.5)
#ifdef PR_GET_NO_NEW_PRIVS
    prctl(PR_GET_NO_NEW_PRIVS, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_NO_NEW_PRIVS) = NNNN
#endif

    // PR_SET_PDEATHSIG (since Linux 2.1.57)
    prctl(PR_GET_PDEATHSIG, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_PDEATHSIG, XXXX) = 0

    // PR_SET_PTRACER (since Linux 3.4)
#ifdef PR_SET_PTRACER
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_PTRACER, PR_SET_PTRACER_ANY) = NNNN

    prctl(PR_SET_PTRACER, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_PTRACER, 0) = NNNN
#endif

    // PR_SET_SECCOMP (since Linux 2.6.23)
    // Note that PR_GET_SECCOMP causes a SIGKILL signal to be sent if
    // we're in strict secure computing mode.
#ifdef PR_GET_SECCOMP
    prctl(PR_GET_SECCOMP, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_SECCOMP) = NNNN
#endif

    // PR_SET_SECUREBITS (since Linux 2.6.26). However, RHEL6 doesn't
    // have the SECBIT stuff available to userspace (no
    // linux/securebits.h header exists). So, we require a 3.0 kernel.
#if defined(PR_SET_SECUREBITS) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
    prctl(PR_SET_SECUREBITS, SECBIT_KEEP_CAPS | SECBIT_NO_SETUID_FIXUP,
	  0L, 0L, 0L);
    //staptest// prctl (PR_SET_SECUREBITS, SECBIT_NO_SETUID_FIXUP|SECBIT_KEEP_CAPS) = NNNN
#endif

    // PR_GET_SECUREBITS (since Linux 2.6.26)
#ifdef PR_GET_SECUREBITS
    prctl(PR_GET_SECUREBITS, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_SECUREBITS) = NNNN
#endif

    // PR_GET_TID_ADDRESS (since Linux 3.5)
#ifdef PR_GET_TID_ADDRESS
    prctl(PR_GET_TID_ADDRESS, &int_ptr, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_TID_ADDRESS, XXXX) = NNNN
#endif

    // PR_SET_TIMERSLACK (since Linux 2.6.28)
#ifdef PR_SET_TIMERSLACK
    prctl(PR_SET_TIMERSLACK, 10L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_TIMERSLACK, 10) = NNNN

    prctl(PR_SET_TIMERSLACK, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_TIMERSLACK, 0) = NNNN
#endif

    // PR_GET_TIMERSLACK (since Linux 2.6.28)
#ifdef PR_GET_TIMERSLACK
    prctl(PR_GET_TIMERSLACK, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_TIMERSLACK) = NNNN
#endif

    // PR_SET_TIMING (since Linux 2.6.0-test4)
#ifdef PR_SET_TIMING
    prctl(PR_SET_TIMING, PR_TIMING_TIMESTAMP, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_TIMING, PR_TIMING_TIMESTAMP) = NNNN
#endif
	  
    // PR_GET_TIMING (since Linux 2.6.0-test4)
#ifdef PR_GET_TIMING
    prctl(PR_GET_TIMING, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_TIMING) = NNNN
#endif

    // PR_TASK_PERF_EVENTS_ENABLE (since Linux 2.6.31)
#ifdef PR_TASK_PERF_EVENTS_ENABLE
    prctl(PR_TASK_PERF_EVENTS_ENABLE, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_TASK_PERF_EVENTS_ENABLE) = NNNN
#endif

    // PR_TASK_PERF_EVENTS_DISABLE (since Linux 2.6.31)
#ifdef PR_TASK_PERF_EVENTS_DISABLE
    prctl(PR_TASK_PERF_EVENTS_DISABLE, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_TASK_PERF_EVENTS_DISABLE) = NNNN
#endif

    // PR_SET_TSC (since Linux 2.6.26, x86 only)
#ifdef PR_SET_TSC
    prctl(PR_SET_TSC, PR_TSC_ENABLE, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_TSC, PR_TSC_ENABLE) = NNNN
#endif
    
    // PR_GET_TSC (since Linux 2.6.26, x86 only)
#ifdef PR_GET_TSC
    prctl(PR_GET_TSC, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_TSC, XXXX) = NNNN
#endif

    // PR_SET_UNALIGN
    prctl(PR_SET_UNALIGN, PR_UNALIGN_NOPRINT, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_UNALIGN, PR_UNALIGN_NOPRINT) = NNNN

    // PR_GET_UNALIGN
    prctl(PR_GET_UNALIGN, &int_buffer, 0L, 0L, 0L);
    //staptest// prctl (PR_GET_UNALIGN, XXXX) = NNNN

    // PR_MCE_KILL (since Linux 2.6.32)
#ifdef PR_MCE_KILL
    prctl(PR_MCE_KILL, PR_MCE_KILL_SET, PR_MCE_KILL_EARLY, 0L, 0L);
    //staptest// prctl (PR_MCE_KILL, PR_MCE_KILL_SET, PR_MCE_KILL_EARLY) = NNNN

    prctl(PR_MCE_KILL, PR_MCE_KILL_CLEAR, 0L, 0L, 0L);
    //staptest// prctl (PR_MCE_KILL, PR_MCE_KILL_CLEAR) = NNNN
#endif

    // PR_MCE_KILL_GET (since Linux 2.6.32)
#ifdef PR_MCE_KILL
    prctl(PR_MCE_KILL_GET, 0L, 0L, 0L, 0L);
    //staptest// prctl (PR_MCE_KILL_GET) = NNNN
#endif

    // PR_SET_MM (since Linux 3.3)
#ifdef PR_SET_MM
    // PR_SET_MM_START_BRK
    prctl(PR_SET_MM, PR_SET_MM_START_BRK, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_MM, PR_SET_MM_START_BRK, 0x0) = NNNN

    // PR_SET_MM_BRK
    prctl(PR_SET_MM, PR_SET_MM_BRK, 0L, 0L, 0L);
    //staptest// prctl (PR_SET_MM, PR_SET_MM_BRK, 0x0) = NNNN
#endif

    /* Limit testing. */
    prctl(-1, 0L, 0L, 0L, 0L);
    //staptest// prctl (0xffffffff, 0x0, 0x0, 0x0, 0x0) = -NNNN

    prctl(-1, -1L, 0L, 0L, 0L);
    //staptest// prctl (0xffffffff, 0x[f]+, 0x0, 0x0, 0x0) = -NNNN

    prctl(-1, 0L, -1L, 0L, 0L);
    //staptest// prctl (0xffffffff, 0x0, 0x[f]+, 0x0, 0x0) = -NNNN

    prctl(-1, 0L, 0L, -1L, 0L);
    //staptest// prctl (0xffffffff, 0x0, 0x0, 0x[f]+, 0x0) = -NNNN

    prctl(-1, 0L, 0L, 0L, -1L);
    //staptest// prctl (0xffffffff, 0x0, 0x0, 0x0, 0x[f]+) = -NNNN

    return 0;
}
