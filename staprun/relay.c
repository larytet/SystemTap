/* -*- linux-c -*-
 *
 * relay.c - staprun relayfs functions
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 *
 * Copyright (C) 2007-2013 Red Hat Inc.
 */

#include "staprun.h"

int out_fd[NR_CPUS];
int monitor_pfd[2];
int monitor_set = 0;
int monitor_end = 0;
static pthread_t reader[NR_CPUS];
static int relay_fd[NR_CPUS];
static int avail_cpus[NR_CPUS];
static int switch_file[NR_CPUS];
static pthread_mutex_t mutex[NR_CPUS];
static int bulkmode = 0;
static volatile int stop_threads = 0;
static time_t *time_backlog[NR_CPUS];
static int backlog_order=0;
#define BACKLOG_MASK ((1 << backlog_order) - 1)

#ifdef NEED_PPOLL
int ppoll(struct pollfd *fds, nfds_t nfds,
	  const struct timespec *timeout, const sigset_t *sigmask)
{
	sigset_t origmask;
	int ready;
	int tim;
	if (timeout == NULL)
		tim = -1;
	else
		tim = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;

	sigprocmask(SIG_SETMASK, sigmask, &origmask);
	ready = poll(fds, nfds, tim);
	sigprocmask(SIG_SETMASK, &origmask, NULL);
	return ready;
}
#endif

int init_backlog(int cpu)
{
	int order = 0;
	if (!fnum_max)
		return 0;
	while (fnum_max >> order) order++;
	if (fnum_max == 1<<(order-1)) order--;
	time_backlog[cpu] = (time_t *)calloc(1<<order, sizeof(time_t));
	if (time_backlog[cpu] == NULL) {
		_err("Memory allocation failed\n");
		return -1;
	}
	backlog_order = order;
	return 0;
}

void write_backlog(int cpu, int fnum, time_t t)
{
	time_backlog[cpu][fnum & BACKLOG_MASK] = t;
}

time_t read_backlog(int cpu, int fnum)
{
	return time_backlog[cpu][fnum & BACKLOG_MASK];
}

static int open_outfile(int fnum, int cpu, int remove_file)
{
	char buf[PATH_MAX];
	time_t t;
	if (!outfile_name) {
		_err("-S is set without -o. Please file a bug report.\n");
		return -1;
	}

	time(&t);
	if (fnum_max) {
		if (remove_file) {
			 /* remove oldest file */
			if (make_outfile_name(buf, PATH_MAX, fnum - fnum_max,
				 cpu, read_backlog(cpu, fnum - fnum_max),
				 bulkmode) < 0)
				return -1;
			remove(buf); /* don't care */
		}
		write_backlog(cpu, fnum, t);
	}

	if (make_outfile_name(buf, PATH_MAX, fnum, cpu, t, bulkmode) < 0)
		return -1;
	out_fd[cpu] = open_cloexec (buf, O_CREAT|O_TRUNC|O_WRONLY, 0666);
	if (out_fd[cpu] < 0) {
		perr("Couldn't open output file %s", buf);
		return -1;
	}
	return 0;
}

static int switch_outfile(int cpu, int *fnum)
{
	int remove_file = 0;

	dbug(3, "thread %d switching file\n", cpu);
	close(out_fd[cpu]);
	*fnum += 1;
	if (fnum_max && *fnum >= fnum_max)
		remove_file = 1;
	if (open_outfile(*fnum, cpu, remove_file) < 0) {
		perr("Couldn't open file for cpu %d, exiting.", cpu);
		return -1;
	}
	return 0;
}

/**
 *	reader_thread - per-cpu channel buffer reader
 */
static void *reader_thread(void *data)
{
        char buf[131072];
        int rc, cpu = (int)(long)data;
        struct pollfd pollfd;
	struct timespec tim = {.tv_sec=0, .tv_nsec=200000000}, *timeout = &tim;
	sigset_t sigs;
	off_t wsize = 0;
	int fnum = 0;

	sigemptyset(&sigs);
	sigaddset(&sigs,SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigs, NULL);

	sigfillset(&sigs);
	sigdelset(&sigs,SIGUSR2);
	
	if (bulkmode) {
		cpu_set_t cpu_mask;
		CPU_ZERO(&cpu_mask);
		CPU_SET(cpu, &cpu_mask);
		if( sched_setaffinity( 0, sizeof(cpu_mask), &cpu_mask ) < 0 )
			_perr("sched_setaffinity");
#ifdef NEED_PPOLL
		/* Without a real ppoll, there is a small race condition that could */
		/* block ppoll(). So use a timeout to prevent that. */
		timeout->tv_sec = 10;
		timeout->tv_nsec = 0;
#else
		timeout = NULL;
#endif
	}

        if (reader_timeout_ms && timeout) {
                timeout->tv_sec = reader_timeout_ms / 1000;
                timeout->tv_nsec = (reader_timeout_ms - timeout->tv_sec * 1000) * 1000000;
        }
	
	pollfd.fd = relay_fd[cpu];
	pollfd.events = POLLIN;

        do {
		dbug(3, "thread %d start ppoll\n", cpu);
                rc = ppoll(&pollfd, 1, timeout, &sigs);
		dbug(3, "thread %d end ppoll:%d\n", cpu, rc);
                if (rc < 0) {
			dbug(3, "cpu=%d poll=%d errno=%d\n", cpu, rc, errno);
			if (errno == EINTR) {
				if (stop_threads)
					break;

				pthread_mutex_lock(&mutex[cpu]);
				if (switch_file[cpu]) {
					if (switch_outfile(cpu, &fnum) < 0) {
						switch_file[cpu] = 0;
						pthread_mutex_unlock(&mutex[cpu]);
						goto error_out;
					}
					switch_file[cpu] = 0;
					wsize = 0;
				}
				pthread_mutex_unlock(&mutex[cpu]);
			} else {
				_perr("poll error");
				goto error_out;
			}
                }

		while ((rc = read(relay_fd[cpu], buf, sizeof(buf))) > 0) {
                        int wbytes = rc;
                        char *wbuf = buf;
                        
			/* Switching file */
			pthread_mutex_lock(&mutex[cpu]);
			if ((fsize_max && ((wsize + rc) > fsize_max)) ||
			    switch_file[cpu]) {
				if (switch_outfile(cpu, &fnum) < 0) {
					switch_file[cpu] = 0;
					pthread_mutex_unlock(&mutex[cpu]);
					goto error_out;
				}
				switch_file[cpu] = 0;
				wsize = 0;
			}
			pthread_mutex_unlock(&mutex[cpu]);

                        /* Copy loop.  Must repeat write(2) in case of a pipe overflow
                           or other transient fullness. */
                        while (wbytes > 0) {
                                rc = write(out_fd[cpu], wbuf, wbytes);
                                if (rc <= 0) {
					perr("Couldn't write to output %d for cpu %d, exiting.",
                                             out_fd[cpu], cpu);
                                        goto error_out;
                                }
                                wbytes -= rc;
                                wbuf += rc;
                        }
			wsize += wbytes;
		}
        } while (!stop_threads);
	dbug(3, "exiting thread for cpu %d\n", cpu);
	return(NULL);

error_out:
	/* Signal the main thread that we need to quit */
	kill(getpid(), SIGTERM);
	dbug(2, "exiting thread for cpu %d after error\n", cpu);
	return(NULL);
}

static void switchfile_handler(int sig)
{
	int i;
	if (stop_threads || !outfile_name)
		return;

	for (i = 0; i < ncpus; i++) {
		pthread_mutex_lock(&mutex[avail_cpus[i]]);
		if (reader[avail_cpus[i]] && switch_file[avail_cpus[i]]) {
			pthread_mutex_unlock(&mutex[avail_cpus[i]]);
			dbug(2, "file switching is progressing, signal ignored.\n", sig);
			return;
		}
		pthread_mutex_unlock(&mutex[avail_cpus[i]]);
	}
	for (i = 0; i < ncpus; i++) {
		pthread_mutex_lock(&mutex[avail_cpus[i]]);
		if (reader[avail_cpus[i]]) {
			switch_file[avail_cpus[i]] = 1;
			pthread_mutex_unlock(&mutex[avail_cpus[i]]);

			// Make sure we don't send the USR2 signal to
			// ourselves.
			if (pthread_equal(pthread_self(),
					  reader[avail_cpus[i]]))
				break;
			pthread_kill(reader[avail_cpus[i]], SIGUSR2);
		}
		else {
			pthread_mutex_unlock(&mutex[avail_cpus[i]]);
			break;
		}
	}
}

/**
 *	init_relayfs - create files and threads for relayfs processing
 *
 *	Returns 0 if successful, negative otherwise
 */
int init_relayfs(void)
{
	int i, len;
	int cpui = 0;
	char rqbuf[128];
        char buf[PATH_MAX];
        struct sigaction sa;
        
	dbug(2, "initializing relayfs\n");

	reader[0] = (pthread_t)0;
	relay_fd[0] = 0;
	out_fd[0] = 0;

        /* Find out whether probe module was compiled with STP_BULKMODE. */
	if (send_request(STP_BULK, rqbuf, sizeof(rqbuf)) == 0)
		bulkmode = 1;

	/* Try to open a slew of per-cpu trace%d files.  Per PR19241, we
	   need to go through all potentially present CPUs up to NR_CPUS, that
	   we hope is a reasonable limit.  For !bulknode, "trace0" will be
	   typically used. */

	for (i = 0; i < NR_CPUS; i++) {
                relay_fd[i] = -1;

#ifdef HAVE_OPENAT
                if (relay_basedir_fd >= 0) {
                        if (sprintf_chk(buf, "trace%d", i))
                                return -1;
                        dbug(2, "attempting to openat %s\n", buf);
                        relay_fd[i] = openat_cloexec(relay_basedir_fd, buf, O_RDONLY | O_NONBLOCK, 0);
                }
#endif
                if (relay_fd[i] < 0) {
                        if (sprintf_chk(buf, "/sys/kernel/debug/systemtap/%s/trace%d",
                                        modname, i))
                                return -1;
                        dbug(2, "attempting to open %s\n", buf);
                        relay_fd[i] = open_cloexec(buf, O_RDONLY | O_NONBLOCK, 0);
                }
		if (relay_fd[i] >= 0) {
			avail_cpus[cpui++] = i;
		}
	}
	ncpus = cpui;
	dbug(2, "ncpus=%d, bulkmode = %d\n", ncpus, bulkmode);
	for (i = 0; i < ncpus; i++)
		dbug(2, "cpui=%d, relayfd=%d\n", i, avail_cpus[i]);

	if (ncpus == 0) {
		_err("couldn't open %s.\n", buf);
		return -1;
	}
	if (ncpus > 1 && bulkmode == 0) {
		_err("ncpus=%d, bulkmode = %d\n", ncpus, bulkmode);
		_err("This is inconsistent! Please file a bug report. Exiting now.\n");
		return -1;
	}

        /* PR7097 */
        if (load_only)
                return 0;

	if (fsize_max) {
		/* switch file mode */
		for (i = 0; i < ncpus; i++) {
			if (init_backlog(avail_cpus[i]) < 0)
				return -1;
			if (open_outfile(0, avail_cpus[i], 0) < 0)
  				return -1;
		}
	} else if (bulkmode) {
		for (i = 0; i < ncpus; i++) {
			if (outfile_name) {
				/* special case: for testing we sometimes want to write to /dev/null */
				if (strcmp(outfile_name, "/dev/null") == 0) {
					/* This strcpy() is OK, since
					 * we know buf is PATH_MAX
					 * bytes long. */
					strcpy(buf, "/dev/null");
				} else {
					len = stap_strfloctime(buf, PATH_MAX,
						 outfile_name, time(NULL));
					if (len < 0) {
						err("Invalid FILE name format\n");
						return -1;
					}
					if (snprintf_chk(&buf[len],
						PATH_MAX - len, "_%d", avail_cpus[i]))
						return -1;
				}
			} else {
				if (sprintf_chk(buf, "stpd_cpu%d", avail_cpus[i]))
					return -1;
			}
			
			out_fd[avail_cpus[i]] = open_cloexec (buf, O_CREAT|O_TRUNC|O_WRONLY, 0666);
			if (out_fd[avail_cpus[i]] < 0) {
				perr("Couldn't open output file %s", buf);
				return -1;
			}
		}
	} else {
		/* stream mode */
		if (outfile_name) {
			len = stap_strfloctime(buf, PATH_MAX,
						 outfile_name, time(NULL));
			if (len < 0) {
				err("Invalid FILE name format\n");
				return -1;
			}
			out_fd[avail_cpus[0]] = open_cloexec (buf, O_CREAT|O_TRUNC|O_WRONLY, 0666);
			if (out_fd[avail_cpus[0]] < 0) {
				perr("Couldn't open output file %s", buf);
				return -1;
			}
		} else
			if (monitor) {
				if (pipe_cloexec(monitor_pfd)) {
					perr("Couldn't create pipe");
					return -1;
				}
				fcntl(monitor_pfd[0], F_SETFL, O_NONBLOCK); /* read end */
                                /* NB: leave write end of pipe normal blocking mode, since
                                   that's the same mode as for STDOUT_FILENO. */
				/* fcntl(monitor_pfd[1], F_SETFL, O_NONBLOCK); */ 
#ifdef HAVE_F_SETPIPE_SZ
                                /* Make it less likely for the pipe to be full. */
				/* fcntl(monitor_pfd[1], F_SETPIPE_SZ, 8*65536); */
#endif
				monitor_set = 1;
				out_fd[avail_cpus[0]] = monitor_pfd[1];
			} else {
				out_fd[avail_cpus[0]] = STDOUT_FILENO;
			}
	}

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = switchfile_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, NULL);

        dbug(2, "starting threads\n");
	for (i = 0; i < ncpus; i++) {
		if (pthread_mutex_init(&mutex[avail_cpus[i]], NULL) < 0) {
                        _perr("failed to create mutex");
                        return -1;
		}
	}
        for (i = 0; i < ncpus; i++) {
                if (pthread_create(&reader[avail_cpus[i]], NULL, reader_thread,
                                   (void *)(long)avail_cpus[i]) < 0) {
                        _perr("failed to create thread");
                        return -1;
                }
        }
	
	return 0;
}

void close_relayfs(void)
{
	int i;
	stop_threads = 1;
	dbug(2, "closing\n");
	for (i = 0; i < ncpus; i++) {
		if (reader[avail_cpus[i]])
			pthread_kill(reader[avail_cpus[i]], SIGUSR2);
		else
			break;
	}
	for (i = 0; i < ncpus; i++) {
		if (reader[avail_cpus[i]])
			pthread_join(reader[avail_cpus[i]], NULL);
		else
			break;
	}
	for (i = 0; i < ncpus; i++) {
		if (relay_fd[avail_cpus[i]] >= 0)
			close(relay_fd[avail_cpus[i]]);
		else
			break;
	}
	for (i = 0; i < ncpus; i++) {
		pthread_mutex_destroy(&mutex[avail_cpus[i]]);
	}
	dbug(2, "done\n");
}

