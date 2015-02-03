#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/quota.h>
//#include <xfs/xqm.h>

int main()
{
    struct dqblk dqblk;
    uid_t uid;

    // Note that these calls will fail for a couple of reasons:
    //   1) You have to be root to run quotactl()
    //   2) A filesystem has to have quotas turned on
    // The latter isn't likely to be true, but we'll be careful
    // anyway.
    uid = getuid();

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), NULL, uid, (caddr_t)&dqblk);
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *(null), NNNN, XXXX) = -NNNN

    quotactl(QCMD(Q_SYNC, GRPQUOTA), NULL, 0, NULL);
    //staptest// quotactl (Q_SYNC|GRPQUOTA, *(null), 0, 0x0) = NNNN

    /* Limit testing. */
    quotactl(-1, NULL, uid, (caddr_t)&dqblk);
    //staptest// quotactl (XXXX|XXXX, *(null), NNNN, XXXX) = -NNNN

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), (char *)-1, uid, (caddr_t)&dqblk);
#ifdef __s390__
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, [7]?[f]+, NNNN, XXXX) = -NNNN
#else
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, [f]+, NNNN, XXXX) = -NNNN
#endif

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), NULL, -1, (caddr_t)&dqblk);
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *(null), -1, XXXX) = -NNNN

    quotactl(QCMD(Q_GETQUOTA, USRQUOTA), NULL, uid, (caddr_t)-1);
#ifdef __s390__
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *(null), NNNN, 0x[7]?[f]+) = -NNNN
#else
    //staptest// quotactl (Q_GETQUOTA|USRQUOTA, *(null), NNNN, 0x[f]+) = -NNNN
#endif
    return 0;
}
