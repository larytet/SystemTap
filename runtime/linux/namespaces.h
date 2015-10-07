/* -*- linux-c -*-
 * Namespace Functions
 * Copyright (C) 2015 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_NAMESPACES_H_
#define _LINUX_NAMESPACES_H_

#if defined(CONFIG_PID_NS)
#include <linux/pid_namespace.h>
#endif

#if defined(CONFIG_USER_NS)
#include <linux/user_namespace.h>
#endif

typedef enum {
  PID,
  TID,
  PGRP,
  SID
} PIDINFOTYPE;

typedef enum {
  UID,
  EUID,
  GID,
  EGID
} USERINFOTYPE;


// The get_task_from_pid() function assumes the rcu_read_lock is
// held. The returned task_struct, if not NULL, has its reference
// count increased. Callers must call put_task_struct() when finished
// to decrease the reference count.
//
// Also note that the returned task_struct pointer, if not NULL,
// should be a valid task_struct pointer that doesn't need to be
// dereferenced before using.
static struct task_struct *get_task_from_pid(int target_pid)
{
  struct task_struct *target_ns_task = NULL;
#if defined(CONFIG_PID_NS) || defined(CONFIG_USER_NS)
  struct pid *target_ns_pid;

  // can't use find_get_pid() since it ends up looking for the
  // STP_TARGET_NS_PID in the current process' ns, when the PID is
  // what's seen in the init ns (I think)
  target_ns_pid = find_pid_ns(target_pid, &init_pid_ns);

  if (!target_ns_pid)
    return NULL;

  // use pid_task instead of since we want to handle our own locking
  target_ns_task = pid_task(target_ns_pid, PIDTYPE_PID);
  if (!target_ns_task)
    return NULL;

  get_task_struct(target_ns_task);
#endif
  return target_ns_task;
}


// The get_pid_namespace() function assumes the rcu_read_lock is
// held. The returned pid_namespace, if not NULL, has its reference
// count increased. Callers must call put_pid_ns() when finished to
// decrease the reference count.
static struct pid_namespace *get_pid_namespace(int target_ns)
{
#if defined(CONFIG_PID_NS)
  struct task_struct *target_ns_task;
  struct pid_namespace *target_pid_ns;

  target_ns_task = get_task_from_pid(target_ns);
  if (!target_ns_task)
    return NULL;

  target_pid_ns = task_active_pid_ns(target_ns_task);
  if (target_pid_ns)
    get_pid_ns(target_pid_ns);

  // there is no put_pid_task(), so do the next best thing
  put_task_struct(target_ns_task);
  return target_pid_ns;

#else
  return NULL;
#endif
}


static int from_target_pid_ns(struct task_struct *ts, PIDINFOTYPE type)
{
#if defined(CONFIG_PID_NS) &&  LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
  struct pid_namespace *target_pid_ns;
  int ret = -1;

  rcu_read_lock();
  target_pid_ns = get_pid_namespace(_stp_namespaces_pid);

  if (!target_pid_ns) {
    rcu_read_unlock();
    return -1;
  }

  switch (type) {
    case PID:
      ret = task_tgid_nr_ns(ts, target_pid_ns);
      break;
    case TID:
      ret = task_pid_nr_ns(ts, target_pid_ns);
      break;
    case PGRP:
      ret = task_pgrp_nr_ns(ts, target_pid_ns);
      break;
    case SID:
      ret = task_session_nr_ns(ts, target_pid_ns);
      break;
  }

  // done with the pid namespace and the read lock
  put_pid_ns(target_pid_ns);
  rcu_read_unlock();
  return ret;
#else
  return -1;
#endif
}


// The get_user_namespace() function assumes the rcu_read_lock is
// held. The returned user_namespace, if not NULL, has its reference
// count increased by get_user_ns(). Callers must call put_user_ns()
// when finished with the returned user_namespace to decrease the
// reference count.
static struct user_namespace *get_user_namespace(int target_ns)
{
#if defined(CONFIG_USER_NS)
  struct task_struct *target_ns_task;
  struct user_namespace *target_user_ns;

  target_ns_task = get_task_from_pid(target_ns);
  if (!target_ns_task)
    return NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
  target_user_ns = target_ns_task->nsproxy->user_ns;
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
  target_user_ns = (task_cred_xxx(target_ns_task, user))->user_ns;
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39) */
  target_user_ns = task_cred_xxx(target_ns_task, user_ns);
#endif
#endif

  if (target_user_ns)
    get_user_ns(target_user_ns);
  put_task_struct(target_ns_task);
  return target_user_ns;
#endif /* defined(CONFIG_USER_NS) */
  return NULL;
}

static int from_target_user_ns(struct task_struct *ts, USERINFOTYPE type)
{
#if defined(CONFIG_USER_NS)
  struct user_namespace *target_user_ns;
  int ret = -1;

  rcu_read_lock();
  target_user_ns = get_user_namespace(_stp_namespaces_pid);
  if (!target_user_ns) {
    rcu_read_unlock();
    return -1;
  }

  switch (type) {
    case UID:
      ret = from_kuid_munged(target_user_ns, task_uid(ts));
      break;
    case EUID:
      ret = from_kuid_munged(target_user_ns, task_euid(ts));
      break;
    case GID:
      /* If task_gid() isn't defined, make our own. */
#if !defined(task_gid) && defined(task_cred_xxx)
#define task_gid(task)		(task_cred_xxx((task), gid))
#endif
      ret = from_kgid_munged(target_user_ns, task_gid(ts));
      break;
    case EGID:
      /* If task_egid() isn't defined, make our own. */
#if !defined(task_egid) && defined(task_cred_xxx)
#define task_egid(task)		(task_cred_xxx((task), egid))
#endif
      ret = from_kgid_munged(target_user_ns, task_egid(ts));
      break;
  }

  // done with the user namespace and the read lock
  put_user_ns(target_user_ns);
  rcu_read_unlock();
  return ret;
#else
  return -1;
#endif
}

#endif /* _LINUX_NAMESPACES_H_ */

