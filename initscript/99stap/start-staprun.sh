#!/bin/bash

# Inserts the SystemTap modules using staprun.

. /etc/systemtap-params.conf

# From here, we can access /var/run (or rather what it will link to),
# but because $STAT_PATH is user-configurable, we're not guaranteed that
# it will be /var/run.  Regardless, we can't have access to the final
# root so we make do and write to /var/run/systemtap anyway. The init
# script will take care of moving the PID files to the real directory if
# necessary.
PIDDIR=/run/systemtap
mkdir -p $PIDDIR

for script in $ONBOOT_SCRIPTS; do
   eval opts=\$${script}_OPT
   if [ $LOG_BOOT_ERR -eq 1 ]; then
      $STAPRUN $opts $CACHE_PATH/$script.ko 2> $PIDDIR/$script.log
   else
      $STAPRUN $opts $CACHE_PATH/$script.ko
   fi
done

