/* -*- linux-c -*- 
 * time-estimation with minimal dependency on xtime
 * Copyright (C) 2006 Intel Corporation.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */
#if defined (__i386__) || defined (__x86_64__)
#include <asm/cpufeature.h>
#endif
#ifdef STAPCONF_TSC_KHZ
#include <asm/tsc.h>
#endif
#ifdef STAPCONF_KTIME_GET_REAL
#include <linux/ktime.h>
#endif

#include <linux/cpufreq.h>

#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC	1000000L
#endif

typedef struct __stp_time_t {
    /* 
     * A write lock is taken by __stp_time_timer_callback() and
     * __stp_time_cpufreq_callback().  Neither writer is in interrupt context,
     * and both disable interrupts before taking the lock, so there should be
     * no opportunity for deadlock.
     *
     * A read lock is taken by _stp_gettimeofday_us().  There is the potential
     * for this to occur at any time, so there is a slim chance that this will
     * happen while the write lock is held, and it will be impossible to get a
     * read lock.  However, we can limit how long we try to get the lock to
     * avoid a deadlock.
     *
     * Note that seqlock is chosen so that readers don't block writers.  It's
     * also important that readers can attempt a lock from _any_ context (e.g.,
     * NMI), and some kernels don't have read_trylock.
     */
    seqlock_t lock;

    /* These provide a reference time to correlate cycles to real time */
    int64_t base_ns;
    cycles_t base_cycles;

    /* The frequency in kHz of this CPU's time stamp counter, for interpolating
     * cycle counts from the base time. */
    unsigned int freq;

    /* Callback used to schedule updates of the base time */
    struct timer_list timer;
} stp_time_t;

void *stp_time = NULL;

/* Flag to tell the timer callback whether to reregister */
int stp_timer_reregister = 0;

/* Try to estimate the number of CPU cycles in a millisecond - i.e. kHz.  This
 * relies heavily on the accuracy of udelay.  By calling udelay twice, we
 * attempt to account for overhead in the call.
 * 
 * NB: interrupts should be disabled when calling this.
 *
 * FIXME: This not very accurate on Xen kernels!
 */
static unsigned int
__stp_get_freq(void)
{
    // If we can get the actual frequency of HW counter, we use it.
#if defined (__i386__) || defined (__x86_64__)
#ifdef STAPCONF_TSC_KHZ
    return tsc_khz;
#else /*STAPCONF_TSC_KHZ*/
    return cpu_khz;
#endif /*STAPCONF_TSC_KHZ*/
#elif defined (__ia64__)
    return local_cpu_data->itc_freq / 1000;
#elif defined (__s390__) || defined (__s390x__) || defined (__arm__)
    // We don't need to find the cpu freq on s390 as the 
    // TOD clock is always a fix freq. (see: POO pg 4-36.)
    return 0;
#else /* __i386__ || __x86_64__ || __ia64__ || __s390__ || __s390x__ || __arm__*/
    // If we don't know the actual frequency, we estimate it.
    cycles_t beg, mid, end;
    beg = get_cycles(); barrier();
    udelay(1); barrier();
    mid = get_cycles(); barrier();
    udelay(1001); barrier();
    end = get_cycles(); barrier();
    return (beg - 2*mid + end);
#endif
}

static void
__stp_ktime_get_real_ts(struct timespec *ts)
{
#ifdef STAPCONF_KTIME_GET_REAL
    ktime_get_real_ts(ts);
#else /* STAPCONF_KTIME_GET_REAL */
    struct timeval tv;
    do_gettimeofday(&tv);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * NSEC_PER_USEC;
#endif
}

/* The timer callback is in a softIRQ -- interrupts enabled. */
static void
__stp_time_timer_callback(unsigned long val)
{
    unsigned long flags;
    stp_time_t *time;
    struct timespec ts;
    int64_t ns;
    cycles_t cycles;

    local_irq_save(flags);

    __stp_ktime_get_real_ts(&ts);
    cycles = get_cycles();

    ns = (NSEC_PER_SEC * (int64_t)ts.tv_sec) + ts.tv_nsec;

    time = per_cpu_ptr(stp_time, smp_processor_id());
    write_seqlock(&time->lock);
    time->base_ns = ns;
    time->base_cycles = cycles;
    write_sequnlock(&time->lock);

    local_irq_restore(flags);
    /* PR6481: reenable IRQs before resetting the timer.
       XXX: The worst that can probably happen is that we get
	    two consecutive timer resets.  */

    if (likely(stp_timer_reregister))
        mod_timer(&time->timer, jiffies + 1);
}

/* This is called as an IPI, with interrupts disabled. */
static void
__stp_init_time(void *info)
{
    struct timespec ts;
    stp_time_t *time = per_cpu_ptr(stp_time, smp_processor_id());

    seqlock_init(&time->lock);
    __stp_ktime_get_real_ts(&ts);
    time->base_cycles = get_cycles();
    time->base_ns = (NSEC_PER_SEC * (int64_t)ts.tv_sec) + ts.tv_nsec;
    time->freq = __stp_get_freq();

    init_timer(&time->timer);
    time->timer.expires = jiffies + 1;
    time->timer.function = __stp_time_timer_callback;
    add_timer(&time->timer);
}

#ifdef CONFIG_CPU_FREQ
/* The cpufreq callback is not in interrupt context -- interrupts enabled */
static int
__stp_time_cpufreq_callback(struct notifier_block *self,
        unsigned long state, void *vfreqs)
{
    unsigned long flags;
    struct cpufreq_freqs *freqs;
    int freq_khz;
    stp_time_t *time;

    switch (state) {
        case CPUFREQ_POSTCHANGE:
        case CPUFREQ_RESUMECHANGE:
            freqs = (struct cpufreq_freqs *)vfreqs;
            freq_khz = freqs->new;
            time = per_cpu_ptr(stp_time, freqs->cpu);
            write_seqlock_irqsave(&time->lock, flags);
            time->freq = freq_khz;
            write_sequnlock_irqrestore(&time->lock, flags);
            break;
    }

    return NOTIFY_OK;
}

struct notifier_block __stp_time_notifier = {
    .notifier_call = __stp_time_cpufreq_callback,
};

static int
__stp_constant_freq(void)
{
#ifdef STAPCONF_CONSTANT_TSC
    // If the CPU has constant tsc, we don't need to use cpufreq.
    return boot_cpu_has(X86_FEATURE_CONSTANT_TSC);
#elif defined (__ia64__) || defined (__s390__) || defined (__s390x__) || defined (__arm__)
    // these architectures have constant time counter.
    return 1;
#else
    return 0;
#endif
}
#endif /* CONFIG_CPU_FREQ */

/* This function is called during module unloading. */
void
_stp_kill_time(void)
{
    if (stp_time) {
        int cpu;
        stp_timer_reregister = 0;
        for_each_online_cpu(cpu) {
            stp_time_t *time = per_cpu_ptr(stp_time, cpu);
            del_timer_sync(&time->timer);
        }
#ifdef CONFIG_CPU_FREQ
        if (!__stp_constant_freq()) {
            cpufreq_unregister_notifier(&__stp_time_notifier,
                                        CPUFREQ_TRANSITION_NOTIFIER);
        }
#endif

        _stp_free_percpu(stp_time);
    }
}

/* This function is called during module loading. */
int
_stp_init_time(void)
{
    int ret = 0;

    stp_time = _stp_alloc_percpu(sizeof(stp_time_t));
    if (unlikely(stp_time == 0))
	    return -1;
    
    stp_timer_reregister = 1;
    ret = on_each_cpu(__stp_init_time, NULL, 0, 1);

#ifdef CONFIG_CPU_FREQ
    if (!ret && !__stp_constant_freq()) {
        ret = cpufreq_register_notifier(&__stp_time_notifier,
                CPUFREQ_TRANSITION_NOTIFIER);

        if (!ret) {
            int cpu;
            for_each_online_cpu(cpu) {
                unsigned long flags;
                int freq_khz = cpufreq_get(cpu);
                if (freq_khz > 0) {
                    stp_time_t *time = per_cpu_ptr(stp_time, cpu);
                    write_seqlock_irqsave(&time->lock, flags);
                    time->freq = freq_khz;
                    write_sequnlock_irqrestore(&time->lock, flags);
                }
            }
        }
    }
#endif
   if (ret)
        _stp_kill_time();
    return ret;
}

int64_t
_stp_gettimeofday_ns(void)
{
    int64_t base;
    cycles_t last, delta;
    unsigned int freq;
    unsigned int seq;
    stp_time_t *time;
    int i = 0;

    preempt_disable();
    time = per_cpu_ptr(stp_time, smp_processor_id());

    seq = read_seqbegin(&time->lock);
    base = time->base_ns;
    last = time->base_cycles;
    freq = time->freq;
    while (unlikely(read_seqretry(&time->lock, seq))) {
        if (unlikely(++i >= MAXTRYLOCK))
            return 0;
        ndelay(TRYLOCKDELAY);
        seq = read_seqbegin(&time->lock);
        base = time->base_ns;
        last = time->base_cycles;
        freq = time->freq;
    }

    delta = get_cycles() - last;

    preempt_enable_no_resched();

#if defined (__s390__) || defined (__s390x__)
    // The TOD clock on the s390 (read by get_cycles() ) 
    // is converted to a nano-second value using the following:
    // (get_cycles() * 125) >> 7;  

    delta = (delta * 125) >> 7;

#elif defined (__arm__)

    /* arm always returns 0 for get_cycles() */
    /* so this is just a fake value until we get a real fix. */
    delta = 1000;

#else /* __s390__ || __s390x__ */

    // Verify units:
    //   (D cycles) * (1E6 ns/ms) / (F cycles/ms [kHz]) = ns
    delta *= NSEC_PER_MSEC;
    do_div(delta, freq);
#endif

    return base + delta;
}
