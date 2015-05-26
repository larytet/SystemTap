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
static struct pid_namespace *get_pid_namespace (int target_ns) {
  struct pid *target_ns_pid;

  rcu_read_lock();
  // can't use find_get_pid() since it ends up looking for the STP_TARGET_NS_PID
  // in the current process' ns, when the PID is what's seen in the init ns (I think)
  target_ns_pid = find_pid_ns(target_ns, &init_pid_ns);
  rcu_read_unlock();

  if (target_ns_pid){
    // use get_pid_task instead of pid_task since it'll handle locking.
    struct task_struct *target_ns_task = get_pid_task(target_ns_pid, PIDTYPE_PID);
    if(target_ns_task)
      return task_active_pid_ns(target_ns_task);
  }
  return NULL;
}
#endif /* defined(CONFIG_PID_NS) */


#if defined(CONFIG_USER_NS)
static struct user_namespace *get_user_namespace (int target_ns) {
  struct pid *target_ns_pid;

  rcu_read_lock();
  target_ns_pid = find_pid_ns(target_ns, &init_pid_ns);
  rcu_read_unlock();
  if (target_ns_pid){
    struct task_struct *target_ns_task = get_pid_task(target_ns_pid, PIDTYPE_PID);
    if(target_ns_task)
      return task_cred_xxx(target_ns_task, user_ns);
  }
  return NULL;
}
#endif /* defined(CONFIG_USER_NS) */
#endif /* _LINUX_NAMESPACES_H_ */

