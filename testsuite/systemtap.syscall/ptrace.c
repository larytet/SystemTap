/* COVERAGE: ptrace */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <elf.h>
#include <sys/ptrace.h>
#include <stddef.h>

// Sigh. /usr/include/sys/ptrace.h and /usr/include/linux/ptrace.h
// don't get along well. Work around this.
#define ia64_fpreg XXX_ia64_fpreg
#define pt_all_user_regs XXX_pt_all_user_regs
#include <linux/ptrace.h>
#undef pt_all_user_regs
#undef ia64_fpreg

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/uio.h>
#ifdef PTRACE_ARCH_PRCTL
#include <asm/prctl.h>
#endif

/* Sigh. ptrace request values are really enum values. Usually, but
 * not always, there is a corresponding #define. Sometimes the #define
 * is named 'PT_*' instead of 'PTRACE_*'. Fix the ones that we test.
 */
#if defined(PT_GETREGS) && !defined(PTRACE_GETREGS)
#define PTRACE_GETREGS PT_GETREGS
#endif
#if defined(PT_GETFPREGS) && !defined(PTRACE_GETFPREGS)
#define PTRACE_GETFPREGS PT_GETFPREGS
#endif
#if defined(PT_SETREGS) && !defined(PTRACE_SETREGS)
#define PTRACE_SETREGS PT_SETREGS
#endif
#if defined(PT_SETFPREGS) && !defined(PTRACE_SETFPREGS)
#define PTRACE_SETFPREGS PT_SETFPREGS
#endif

/* Sigh. On s390x, the user headers have the PTRACE_GETREGS and
 * PTRACE_SINGLEBLOCK constants with the same value. The kernel only
 * supports PTRACE_SINGLEBLOCK. Fix this. */
#if ((defined(__s390__) || defined(__s390x__)) && defined(PTRACE_GETREGS) \
     && defined(PTRACE_SINGLEBLOCK))
#undef PTRACE_GETREGS
#endif

static void do_child(void)
{
    struct sigaction child_act;

    child_act.sa_handler = SIG_IGN;
    child_act.sa_flags = SA_RESTART;
    sigemptyset(&child_act.sa_mask);
    sigaction(SIGUSR2, &child_act, NULL);
    ptrace(PTRACE_TRACEME, 0, 0, 0);

    /* ensure that child bypasses signal handler */
    kill(getpid(), SIGUSR2);
    exit(1);
}

int main()
{
    pid_t child_pid;
    int status;
    struct sigaction act;
    siginfo_t siginfo;
#ifdef PTRACE_PEEKSIGINFO
    struct ptrace_peeksiginfo_args peeksiginfo_args;
#endif
    unsigned long message;
    sigset_t set;
#if defined(PTRACE_PEEKUSR_AREA) || defined(PTRACE_POKEUSR_AREA) \
    || defined(PTRACE_PEEKTEXT_AREA) || defined(PTRACE_PEEKDATA_AREA) \
    || defined(PTRACE_POKETEXT_AREA) || defined(PTRACE_POKEDATA_AREA)
    ptrace_area area;
#endif

    /* For PTRACE_{GETREGS,SETREGS,GETFPREGS,SETFPREGS}, etc., we
     * really should pass each architecture's [floating point]
     * register definition structure. So, we should have an
     * architecture-dependent structure definition section
     * below. However, these calls aren't going to succeed anyway. To
     * be safe though, define a big buffer to use instead. */
    unsigned char regbuf[8192];
    struct iovec iov;

    child_pid = fork();
    if (child_pid == 0) {
	/* Child */
	do_child();
	exit(0);
    }

    /* Parent */
    waitpid(child_pid, &status, 0);

    /* Restart child */
    ptrace(PTRACE_CONT, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_CONT, NNNN, 0x0, SIG_0) = NNNN

    /* Give the child time to exit. */
    sleep(1);

    /* Argunment handling testing. Notice we're using the child's pid,
     * which is has exited, but hasn't been reaped. So, hopefully, all
     * the following calls will fail. */  

    /* Note that for PEEKTEXT/PEEKDATA/PEEKUSER, glibc puts an
     * address in for the 'data' parameter, even though we passed in a
     * 0. */
    ptrace(PTRACE_PEEKTEXT, child_pid, 0x12345678, 0);
    //staptest// ptrace (PTRACE_PEEKTEXT, NNNN, 0x12345678, \[XXXX\]) = NNNN

    ptrace(PTRACE_PEEKDATA, child_pid, 0x12345678, 0);
    //staptest// ptrace (PTRACE_PEEKDATA, NNNN, 0x12345678, \[XXXX\]) = NNNN

    ptrace(PTRACE_PEEKUSER, child_pid, 0x12345678, 0);
    //staptest// ptrace (PTRACE_PEEKUSR, NNNN, 0x12345678, \[XXXX\]) = NNNN

#if __WORDSIZE == 64
    ptrace(PTRACE_POKETEXT, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PTRACE_POKETEXT, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PTRACE_POKETEXT, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PTRACE_POKETEXT, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif

#if __WORDSIZE == 64
    ptrace(PTRACE_POKEDATA, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PTRACE_POKEDATA, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PTRACE_POKEDATA, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PTRACE_POKEDATA, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif

#if __WORDSIZE == 64
    ptrace(PTRACE_POKEUSER, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PTRACE_POKEUSR, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PTRACE_POKEUSER, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PTRACE_POKEUSR, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif

    // PTRACE_GETREGS isn't supported on all architectures.
#ifdef PTRACE_GETREGS
    ptrace(PTRACE_GETREGS, child_pid, 0, regbuf);
    //staptest// ptrace ([[[[PTRACE_GETREGS!!!!XXXX]]]], NNNN, 0x0, XXXX) = NNNN
#endif

    // PTRACE_SETREGS isn't supported on all architectures.
#ifdef PTRACE_SETREGS
    ptrace(PTRACE_SETREGS, child_pid, 0, regbuf);
    //staptest// ptrace ([[[[PTRACE_SETREGS!!!!XXXX]]]], NNNN, 0x0, XXXX) = NNNN
#endif

    // PTRACE_GETFPREGS isn't supported on all architectures.
#ifdef PTRACE_GETFPREGS
    ptrace(PTRACE_GETFPREGS, child_pid, 0, regbuf);
    //staptest// ptrace ([[[[PTRACE_GETFPREGS!!!!XXXX]]]], NNNN, 0x0, XXXX) = NNNN
#endif

    // PTRACE_SETFPREGS isn't supported on all architectures.
#ifdef PTRACE_SETFPREGS
    ptrace(PTRACE_SETFPREGS, child_pid, 0, regbuf);
    //staptest// ptrace ([[[[PTRACE_SETFPREGS!!!!XXXX]]]], NNNN, 0x0, XXXX) = NNNN
#endif

    // PTRACE_GETREGSET (since Linux 2.6.34)
#ifdef PTRACE_GETREGSET
    iov.iov_len = sizeof(regbuf);
    iov.iov_base = regbuf;
    ptrace(PTRACE_GETREGSET, child_pid, NT_PRSTATUS, &iov);
    //staptest// ptrace (PTRACE_GETREGSET, NNNN, NT_PRSTATUS, \[{XXXX, 8192}\]) = NNNN
#endif

    // PTRACE_SETREGSET (since Linux 2.6.34)
#ifdef PTRACE_SETREGSET
    iov.iov_len = sizeof(regbuf);
    iov.iov_base = regbuf;
    ptrace(PTRACE_SETREGSET, child_pid, NT_FPREGSET, &iov);
    //staptest// ptrace (PTRACE_SETREGSET, NNNN, NT_PRFPREG, \[{XXXX, 8192}\]) = NNNN
#endif

    ptrace(PTRACE_GETSIGINFO, child_pid, 0, &act);
    //staptest// ptrace (PTRACE_GETSIGINFO, NNNN, 0x0, XXXX) = NNNN
	
    ptrace(PTRACE_SETSIGINFO, child_pid, 0, &act);
    //staptest// ptrace (PTRACE_SETSIGINFO, NNNN, 0x0, XXXX) = NNNN

    // PTRACE_PEEKSIGINFO (since Linux 3.10).
#ifdef PTRACE_PEEKSIGINFO
    peeksiginfo_args.off = 0;
    peeksiginfo_args.flags = PTRACE_PEEKSIGINFO_SHARED;
    peeksiginfo_args.nr = 1;
    ptrace(PTRACE_PEEKSIGINFO, child_pid, &peeksiginfo_args, &siginfo);
    //staptest// ptrace (PTRACE_PEEKSIGINFO, NNNN, XXXX, XXXX) = NNNN
#endif

    // PTRACE_GETSIGMASK (since Linux 3.11)
#ifdef PTRACE_GETSIGMASK
    ptrace(PTRACE_GETSIGMASK, child_pid, sizeof(set), &set);
    //staptest// ptrace (PTRACE_GETSIGMASK, NNNN, XXXX, XXXX) = NNNN
#endif

    // PTRACE_SETSIGMASK (since Linux 3.11)
#ifdef PTRACE_SETSIGMASK
    ptrace(PTRACE_SETSIGMASK, child_pid, sizeof(set), &set);
    //staptest// ptrace (PTRACE_SETSIGMASK, NNNN, XXXX, XXXX) = NNNN
#endif

    // PTRACE_SETOPTIONS
    ptrace(PTRACE_SETOPTIONS, child_pid, 0,
	   PTRACE_O_TRACECLONE|PTRACE_O_TRACEEXEC|PTRACE_O_TRACEEXIT);
    //staptest// ptrace (PTRACE_SETOPTIONS, NNNN, 0x0, PTRACE_O_TRACECLONE|PTRACE_O_TRACEEXEC|PTRACE_O_TRACEEXIT) = NNNN

    ptrace(PTRACE_GETEVENTMSG, child_pid, 0, &message);
    //staptest// ptrace (PTRACE_GETEVENTMSG, NNNN, 0x0, XXXX) = NNNN

    ptrace(PTRACE_SYSCALL, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SYSCALL, NNNN, 0x0, SIG_0) = NNNN

    ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SINGLESTEP, NNNN, 0x0, SIG_0) = NNNN

#ifdef PTRACE_SYSEMU
    ptrace(PTRACE_SYSEMU, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SYSEMU, NNNN, 0x0, SIG_0) = NNNN
#endif

#ifdef PTRACE_SYSEMU_SINGLESTEP
    ptrace(PTRACE_SYSEMU_SINGLESTEP, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SYSEMU_SINGLESTEP, NNNN, 0x0, SIG_0) = NNNN
#endif

#ifdef PTRACE_LISTEN
    // PTRACE_LISTEN (since Linux 3.4)
    ptrace(PTRACE_LISTEN, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_LISTEN, NNNN, 0x0, 0x0) = NNNN
#endif

    ptrace(PTRACE_KILL, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_KILL, NNNN, 0x0, 0x0) = NNNN
    
#ifdef PTRACE_INTERRUPT
    // PTRACE_INTERRUPT (since Linux 3.4)
    ptrace(PTRACE_INTERRUPT, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_INTERRUPT, NNNN, 0x0, 0x0) = NNNN
#endif

    ptrace(PTRACE_ATTACH, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_ATTACH, NNNN, 0x0, 0x0) = NNNN

#ifdef PTRACE_SEIZE
    // PTRACE_SEIZE (since Linux 3.4)
    ptrace(PTRACE_SEIZE, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SEIZE, NNNN, 0x0, 0x0) = NNNN
#endif

    ptrace(PTRACE_DETACH, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_DETACH, NNNN, 0x0, SIG_0) = NNNN

    /* Arch-specific ptrace requests. */

#ifdef PTRACE_GETFPXREGS
    ptrace(PTRACE_GETFPXREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETFPXREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_SETFPXREGS
    ptrace(PTRACE_SETFPXREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_SETFPXREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_OLDSETOPTIONS
    ptrace(PTRACE_OLDSETOPTIONS, child_pid, 0, PTRACE_O_TRACECLONE);
    //staptest// ptrace (PTRACE_OLDSETOPTIONS, NNNN, 0x0, PTRACE_O_TRACECLONE) = NNNN
#endif

#ifdef PTRACE_GET_THREAD_AREA
    // The following call is equivalent to the get_thread_area() syscall.
    ptrace(PTRACE_GET_THREAD_AREA, child_pid, -1, regbuf);
    //staptest// ptrace (PTRACE_GET_THREAD_AREA, NNNN, -1, XXXX) = NNNN
#endif

#ifdef COMPAT_PTRACE_GET_THREAD_AREA
    // The following call is equivalent to the get_thread_area() syscall.
    ptrace(COMPAT_PTRACE_GET_THREAD_AREA, child_pid, -1, regbuf);
    //staptest// ptrace (COMPAT_PTRACE_GET_THREAD_AREA, NNNN, -1, XXXX) = NNNN
#endif

#ifdef PTRACE_SET_THREAD_AREA
    // The following call is equivalent to the set_thread_area() syscall.
    ptrace(PTRACE_SET_THREAD_AREA, child_pid, -1, regbuf);
    //staptest// ptrace (PTRACE_SET_THREAD_AREA, NNNN, -1, XXXX) = NNNN
#endif
    
#ifdef PTRACE_SINGLEBLOCK
    ptrace(PTRACE_SINGLEBLOCK, child_pid, 0, 0);
    //staptest// ptrace (PTRACE_SINGLEBLOCK, NNNN, 0x0, SIG_0) = NNNN
#endif

#ifdef PTRACE_ARCH_PRCTL
    // The following call is equivalent to the arch_prctl() syscall,
    // except the args are backwards.
    ptrace(PTRACE_ARCH_PRCTL, child_pid, &message, ARCH_GET_FS);
    //staptest// ptrace (PTRACE_ARCH_PRCTL, NNNN, XXXX, ARCH_GET_FS) = NNNN
#endif

#ifdef PTRACE_OLD_GETSIGINFO
    ptrace(PTRACE_OLD_GETSIGINFO, child_pid, 0, &act);
    //staptest// ptrace (PTRACE_OLD_GETSIGINFO, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_OLD_SETSIGINFO
    ptrace(PTRACE_OLD_SETSIGINFO, child_pid, 0, &act);
    //staptest// ptrace (PTRACE_OLD_SETSIGINFO, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_GETVRREGS
    ptrace(PTRACE_GETVRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETVRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_SETVRREGS
    ptrace(PTRACE_SETVRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_SETVRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_GETEVRREGS
    ptrace(PTRACE_GETEVRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETEVRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_SETEVRREGS
    ptrace(PTRACE_SETEVRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_SETEVRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_GETVSRREGS
    ptrace(PTRACE_GETVSRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETVSRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_SETVSRREGS
    ptrace(PTRACE_SETVSRREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_SETVSRREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_GET_DEBUGREG
    // PTRACE_GET_DEBUGREG: addr is the register number to get
    ptrace(PTRACE_GET_DEBUGREG, child_pid, 2, regbuf);
    //staptest// ptrace (PTRACE_GET_DEBUGREG, NNNN, 0x2, XXXX) = NNNN
#endif

#ifdef PTRACE_SET_DEBUGREG
    // PTRACE_GET_DEBUGREG: addr is the register number to set, data
    // is the value to set.
    ptrace(PTRACE_SET_DEBUGREG, child_pid, 3, 0x1234);
    //staptest// ptrace (PTRACE_SET_DEBUGREG, NNNN, 0x3, 0x1234) = NNNN
#endif

#ifdef PTRACE_GETREGS64
    ptrace(PTRACE_GETREGS64, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETREGS64, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_SETREGS64
    ptrace(PTRACE_SETREGS64, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_SETREGS64, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PPC_PTRACE_GETREGS
    ptrace(PPC_PTRACE_GETREGS, child_pid, regbuf, 0);
    //staptest// ptrace (PPC_PTRACE_GETREGS, NNNN, XXXX, 0x0) = NNNN
#endif

#ifdef PPC_PTRACE_SETREGS
    ptrace(PPC_PTRACE_SETREGS, child_pid, regbuf, 0);
    //staptest// ptrace (PPC_PTRACE_SETREGS, NNNN, XXXX, 0x0) = NNNN
#endif

#ifdef PPC_PTRACE_GETFPREGS
    ptrace(PPC_PTRACE_GETFPREGS, child_pid, regbuf, 0);
    //staptest// ptrace (PPC_PTRACE_GETFPREGS, NNNN, XXXX, 0x0) = NNNN
#endif

#ifdef PPC_PTRACE_SETFPREGS
    ptrace(PPC_PTRACE_SETFPREGS, child_pid, regbuf, 0);
    //staptest// ptrace (PPC_PTRACE_SETFPREGS, NNNN, XXXX, 0x0) = NNNN
#endif

#ifdef PPC_PTRACE_PEEKTEXT_3264
    ptrace(PPC_PTRACE_PEEKTEXT_3264, child_pid, 0x12345678, 0);
    //staptest// ptrace (PPC_PTRACE_PEEKTEXT_3264, NNNN, 0x12345678, \[XXXX\]) = NNNN
#endif

#ifdef PPC_PTRACE_PEEKDATA_3264
    ptrace(PPC_PTRACE_PEEKDATA_3264, child_pid, 0x12345678, 0);
    //staptest// ptrace (PPC_PTRACE_PEEKDATA_3264, NNNN, 0x12345678, \[XXXX\]) = NNNN
#endif

#ifdef PPC_PTRACE_PEEKUSER_3264
    ptrace(PPC_PTRACE_PEEKUSER_3264, child_pid, 0x12345678, 0);
    //staptest// ptrace (PPC_PTRACE_PEEKUSR_3264, NNNN, 0x12345678, \[XXXX\]) = NNNN
#endif

#ifdef PPC_PTRACE_POKETEXT_3264
#if __WORDSIZE == 64
    ptrace(PPC_PTRACE_POKETEXT_3264, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PPC_PTRACE_POKETEXT_3264, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PPC_PTRACE_POKETEXT_3264, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PPC_PTRACE_POKETEXT_3264, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif
#endif

#ifdef PPC_PTRACE_POKEDATA_3264
#if __WORDSIZE == 64
    ptrace(PPC_PTRACE_POKEDATA_3264, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PPC_PTRACE_POKEDATA_3264, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PPC_PTRACE_POKEDATA_3264, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PPC_PTRACE_POKEDATA_3264, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif
#endif

#ifdef PPC_PTRACE_POKEUSER_3264
#if __WORDSIZE == 64
    ptrace(PPC_PTRACE_POKEUSER_3264, child_pid, 0x12345678, 0x12345678abcdefab);
    //staptest// ptrace (PPC_PTRACE_POKEUSR_3264, NNNN, 0x12345678, 0x12345678abcdefab) = NNNN
#else
    ptrace(PPC_PTRACE_POKEUSER_3264, child_pid, 0x12345678, 0xabcdefab);
    //staptest// ptrace (PPC_PTRACE_POKEUSR_3264, NNNN, 0x12345678, 0xabcdefab) = NNNN
#endif
#endif

#ifdef PPC_PTRACE_DELHWDEBUG
    ptrace(PPC_PTRACE_DELHWDEBUG, child_pid, 0, 10);
    //staptest// ptrace (PPC_PTRACE_DELHWDEBUG, NNNN, 0x0, 10) = NNNN
#endif

#ifdef PTRACE_PEEKUSR_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_PEEKUSR_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_PEEKUSR_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_POKEUSR_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_POKEUSR_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_POKEUSR_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_PEEKTEXT_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_PEEKTEXT_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_PEEKTEXT_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_POKETEXT_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_POKETEXT_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_POKETEXT_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_PEEKDATA_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_PEEKDATA_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_PEEKDATA_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_POKEDATA_AREA
    area.len = 128;
    area.kernel_addr = (addr_t)regbuf;
    area.process_addr = 0;
    ptrace(PTRACE_POKEDATA_AREA, child_pid, &area, 0x0);
    //staptest// ptrace (PTRACE_POKEDATA_AREA, NNNN, [{128, XXXX, 0x0}], 0x0) = NNNN
#endif

#ifdef PTRACE_GET_LAST_BREAK
    ptrace(PTRACE_GET_LAST_BREAK, child_pid, 0x0, regbuf);
    //staptest// ptrace (PTRACE_GET_LAST_BREAK, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_ENABLE_TE
    ptrace(PTRACE_ENABLE_TE, child_pid, 0x0, 0x0);
    //staptest// ptrace (PTRACE_ENABLE_TE, NNNN, 0x0, 0x0) = NNNN
#endif

#ifdef COMPAT_PTRACE_GETREGS
    ptrace(COMPAT_PTRACE_GETREGS, child_pid, 0, regbuf);
    //staptest// ptrace (COMPAT_PTRACE_GETREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef COMPAT_PTRACE_SETREGS
    ptrace(COMPAT_PTRACE_SETREGS, child_pid, 0, regbuf);
    //staptest// ptrace (COMPAT_PTRACE_SETREGS, NNNN, 0x0, XXXX) = NNNN
#endif

#ifdef PTRACE_GETCRUNCHREGS
    ptrace(PTRACE_GETCRUNCHREGS, child_pid, 0, regbuf);
    //staptest// ptrace (PTRACE_GETCRUNCHREGS, NNNN, 0x0, XXXX) = NNNN
#endif

    /* Limit testing. */

    ptrace(-1, child_pid, 0, 0);
    //staptest// ptrace (0x[f]+, NNNN, 0x0, 0x0) = NNNN

    ptrace(-1, (pid_t)-1, 0, 0);
    //staptest// ptrace (0x[f]+, -1, 0x0, 0x0) = NNNN

    ptrace(-1, child_pid, (void *)-1, 0);
#ifdef __s390__
    //staptest// ptrace (0x[f]+, NNNN, 0x[7]?[f]+, 0x0) = NNNN
#else
    //staptest// ptrace (0x[f]+, NNNN, 0x[f]+, 0x0) = NNNN
#endif

    ptrace(-1, child_pid, 0, (void *)-1);
#ifdef __s390__
    //staptest// ptrace (0x[f]+, NNNN, 0x0, 0x[7]?[f]+) = NNNN
#else
    //staptest// ptrace (0x[f]+, NNNN, 0x0, 0x[f]+) = NNNN
#endif

    /* Reap the (dead) child. */
    waitpid(child_pid, &status, 0);

    return 0;
}
