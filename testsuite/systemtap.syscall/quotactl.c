/* COVERAGE: quotactl */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/quota.h>
//#include <xfs/xqm.h>


#ifndef QFMT_VFS_V0
#define   QFMT_VFS_V0 2
#endif


int main()
{
    struct dqblk dqblk;
    struct dqinfo dqinfo;
    uid_t uid;
    int qfmt;

    // Note that these calls will fail for a couple of reasons:
    //   1) You have to be root to run quotactl()
    //   2) A filesystem has to have quotas turned on
    // The latter isn't likely to be true, but we'll be careful
    // anyway.
    uid = getuid();

    quotactl(QCMD(Q_QUOTAON, USRQUOTA), "somedevice", QFMT_VFS_V0, "staptestmnt/aquota.user");
    //staptest// quotactl (Q_QUOTAON|USRQUOTA, "somedevice", QFMT_VFS_V0, "staptestmnt/aquota.user") = -NNNN

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), "somedevice", getuid(), (caddr_t)&dqblk);
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, "somedevice", NNNN, {dqb_bhardlimit=NNNN, dqb_bsoftlimit=NNNN, dqb_curspace=NNNN, dqb_ihardlimit=NNNN, dqb_isoftlimit=NNNN, ...}) = -NNNN

    quotactl(QCMD(Q_SETQUOTA, USRQUOTA), "somedevice", getuid(), (caddr_t)&dqblk);
    //staptest// quotactl (Q_SETQUOTA|USRQUOTA, "somedevice", NNNN, {dqb_bhardlimit=NNNN, dqb_bsoftlimit=NNNN, dqb_curspace=NNNN, dqb_ihardlimit=NNNN, dqb_isoftlimit=NNNN, ...}) = -NNNN

    quotactl(QCMD(Q_GETINFO, USRQUOTA), "somedevice", getuid(), (caddr_t)&dqinfo);
    //staptest// quotactl (Q_GETINFO|USRQUOTA, "somedevice", NNNN, {dqi_bgrace=NNNN, dqi_igrace=NNNN, dqi_flags=NNNN, dqi_valid=NNNN}) = -NNNN

    quotactl(QCMD(Q_SETINFO, USRQUOTA), "somedevice", getuid(), (caddr_t)&dqinfo);
    //staptest// quotactl (Q_SETINFO|USRQUOTA, "somedevice", NNNN, {dqi_bgrace=NNNN, dqi_igrace=NNNN, dqi_flags=NNNN, dqi_valid=NNNN}) = -NNNN

    quotactl(QCMD(Q_GETFMT, USRQUOTA), "somedevice", getuid(), (caddr_t)&qfmt);
    //staptest// quotactl (Q_GETFMT|USRQUOTA, "somedevice", NNNN, XXXX) = -NNNN

    quotactl(QCMD(Q_SYNC, USRQUOTA), "somedevice", 0, NULL);
    //staptest// quotactl (Q_SYNC|USRQUOTA, "somedevice", 0, 0x0) = -NNNN

    quotactl(QCMD(Q_QUOTAOFF, USRQUOTA), "somedevice", 0, NULL);
    //staptest// quotactl (Q_QUOTAOFF|USRQUOTA, "somedevice", 0, 0x0) = -NNNN

    /* Limit testing. */
    quotactl(-1, NULL, uid, (caddr_t)&dqblk);
    //staptest// quotactl (XXXX|XXXX, 0x0, NNNN, XXXX) = -NNNN

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), (char *)-1, uid, (caddr_t)&dqblk);
#ifdef __s390__
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, 0x[7]?[f]+, NNNN, {dqb_bhardlimit=NNNN, dqb_bsoftlimit=NNNN, dqb_curspace=NNNN, dqb_ihardlimit=NNNN, dqb_isoftlimit=NNNN, ...}) = -NNNN
#else
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, 0x[f]+, NNNN, {dqb_bhardlimit=NNNN, dqb_bsoftlimit=NNNN, dqb_curspace=NNNN, dqb_ihardlimit=NNNN, dqb_isoftlimit=NNNN, ...}) = -NNNN
#endif

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), NULL, -1, (caddr_t)&dqblk);
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *0x0, -1, {dqb_bhardlimit=NNNN, dqb_bsoftlimit=NNNN, dqb_curspace=NNNN, dqb_ihardlimit=NNNN, dqb_isoftlimit=NNNN, ...}) = -NNNN

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), NULL, uid, (caddr_t)-1);
#ifdef __s390__
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *0x0, NNNN, 0x[7]?[f]+) = -NNNN
#else
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *0x0, NNNN, 0x[f]+) = -NNNN
#endif

    return 0;
}
