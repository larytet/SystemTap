/* COVERAGE: keyctl add_key request_key */

#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/keyctl.h>

typedef int32_t key_serial_t;

#define __KEYCTL_ARG_TYPE unsigned long

key_serial_t __add_key(const char *type, const char *description,
                       const void *payload, size_t plen, key_serial_t ringid)
{
    return syscall(__NR_add_key, type, description, payload, plen, ringid);
}

key_serial_t __request_key(const char *type, const char *description,
                           const char * callout_info, key_serial_t destringid)
{
    return syscall(__NR_request_key, type, description, callout_info, destringid);
}

int main() {
    key_serial_t ring_id, ring_id_2, key_id, key_id_2;

    // --- test normal operation for keyctl manpage scenarios

    // (*) Get a keyring to work with
    syscall(__NR_keyctl, KEYCTL_READ, KEY_SPEC_USER_SESSION_KEYRING, &ring_id, sizeof(key_serial_t));
    //staptest// keyctl (KEYCTL_READ, KEY_SPEC_USER_SESSION_KEYRING, XXXX, NNNN) = NNNN

    // (*) Add a key to a keyring
    key_id = __add_key("user", "testkey", "somedata", strlen("somedata"), ring_id);
    //staptest// add_key ("user", "testkey", "somedata", 8, NNNN) = NNNN

    // (*) Create a keyring
    ring_id_2 = __add_key("keyring", "newring2", NULL, 0, ring_id);
    //staptest// add_key ("keyring", "newring2",\ +(null), 0, NNNN) = NNNN

    // (*) Request a key
    __request_key("user", "testkey", 0, ring_id);
    //staptest// request_key ("user", "testkey", 0x0, NNNN) = NNNN

    // (*) Update a key
    syscall(__NR_keyctl, KEYCTL_UPDATE, key_id, "blah", 4);
    //staptest// keyctl (KEYCTL_UPDATE, NNNN, "blah", 4) = NNNN

    // (*) Revoke a key
    key_id_2 = __add_key("user", "testkey2", "somedata2", strlen("somedata2"), ring_id);
    //staptest// add_key ("user", "testkey2", "somedata2", 9, NNNN) = NNNN
    syscall(__NR_keyctl, KEYCTL_REVOKE, key_id_2);
    //staptest// keyctl (KEYCTL_REVOKE, NNNN) = 0

    // (*) Clear a keyring
    syscall(__NR_keyctl, KEYCTL_CLEAR, ring_id_2);
    //staptest// keyctl (KEYCTL_CLEAR, NNNN) = 0

    // Remove unneeded keyring
    syscall(__NR_keyctl, KEYCTL_UNLINK, ring_id_2, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = 0

    // (*) Link a key to a keyring
    syscall(__NR_keyctl, KEYCTL_LINK, key_id, ring_id);
    //staptest// keyctl (KEYCTL_LINK, NNNN, NNNN) = 0

    // (*) Unlink a key from a keyring or the session keyring tree
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = 0

    // (*) Search a keyring
    syscall(__NR_keyctl, KEYCTL_SEARCH, ring_id, "user", "testkey", 0);
    //staptest// keyctl (KEYCTL_SEARCH, NNNN, "user", "testkey", 0) = NNNN

    // (*) Read a key
    // see "Get a keyring to work with" above

    // (*) List a keyring
    // see "Get a keyring to work with" above plus "Describe a key" below

    // (*) Describe a key
    syscall(__NR_keyctl, KEYCTL_DESCRIBE, key_id, NULL, 0);
    //staptest// keyctl (KEYCTL_DESCRIBE, NNNN, 0x0, 0) = NNNN

    // (*) Change the access controls on a key
    syscall(__NR_keyctl, KEYCTL_CHOWN, key_id, getuid(), (unsigned long)-1);
    //staptest// keyctl (KEYCTL_CHOWN, NNNN, NNNN, -1) = 0
    syscall(__NR_keyctl, KEYCTL_CHOWN, key_id, (unsigned long)-1, getgid());
    //staptest// keyctl (KEYCTL_CHOWN, NNNN, -1, NNNN) = 0

    // (*) Set the permissions mask on a key
    syscall(__NR_keyctl, KEYCTL_SETPERM, key_id, 0x3f3f3f3f);
    // on rhel[56] i686 ths max string length appears to be too short
    // to accomodate whole the perm string:
    //staptest// keyctl (KEYCTL_SETPERM, NNNN, KEY_POS_VIEW|KEY_POS_READ|KEY_POS_WRITE|KEY_POS_SEARCH|KEY_POS_LINK|KEY_POS_SETATTR|KEY_USR_VIEW|KEY_USR_READ|KEY_USR_WRITE|KEY_USR_SEARCH|KEY_USR_LINK|KEY_USR_SETATTR|KEY_GRP_VIEW|KEY_GRP_READ|KEY_GRP_WRITE|KEY_GRP_SEARCH|KEY_G?R?[[[[!!!!P_LINK|KEY_GRP_SETATTR|KEY_OTH_VIEW|KEY_OTH_READ|KEY_OTH_WRITE|KEY_OTH_SEARCH|KEY_OTH_LINK|KEY_OTH_SETATTR]]]]) = 0

    // (*) Start a new session with fresh keyrings
    // irrelevant here

    // (*) Instantiate a key

    /*
     *  keyctl_assume_authority, keyctl_instantiate, keyctl_instantiate_iov,
     *  keyctl_reject and keyctl_negate can only be successfully run if the
     *  program is run by /sbin/request-key. See `man 3 keyctl_assume_authority`
     *  and `man 3 keyctl` for more info. These (below) will not succeed here:
     */

    key_id_2 = __add_key("user", "testkey2", "somedata2", strlen("somedata2"), ring_id);
    //staptest// add_key ("user", "testkey2", "somedata2", 9, NNNN) = NNNN

    syscall(__NR_keyctl, KEYCTL_ASSUME_AUTHORITY, key_id_2);
    //staptest// keyctl (KEYCTL_ASSUME_AUTHORITY, NNNN) = NNNN ([[[[ENOKEY!!!!EAGAIN]]]])
    syscall(__NR_keyctl, KEYCTL_INSTANTIATE, key_id_2, "somedata2", strlen("somedata2"), ring_id);
    //staptest// keyctl (KEYCTL_INSTANTIATE, NNNN, "somedata2", 9, NNNN) = NNNN (EPERM)

#ifdef KEYCTL_INSTANTIATE_IOV
#include <sys/uio.h>
    struct iovec iovecs[] = {
        {"data1", 5},
        {"data2", 5}
    };

    syscall(__NR_keyctl, KEYCTL_INSTANTIATE_IOV, key_id_2, &iovecs, 2, ring_id);
    //staptest// keyctl (KEYCTL_INSTANTIATE_IOV, NNNN, XXXX, 2, NNNN) = NNNN ([[[[EFAULT!!!!EPERM]]]])
#endif
    syscall(__NR_keyctl, KEYCTL_NEGATE, key_id_2, 30, ring_id);
    //staptest// keyctl (KEYCTL_NEGATE, NNNN, 30, NNNN) = NNNN (EPERM)
#ifdef KEYCTL_REJECT
    syscall(__NR_keyctl, KEYCTL_REJECT, key_id_2, 30, 64, ring_id);
    //staptest// keyctl (KEYCTL_REJECT, NNNN, 30, 64, NNNN) = NNNN (EPERM)
#endif

    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = NNNN

    // (*) Set the expiry time on a key
    key_id_2 = __add_key("user", "testkey2", "somedata2", strlen("somedata2"), ring_id);
    //staptest// add_key ("user", "testkey2", "somedata2", 9, NNNN) = NNNN
    syscall(__NR_keyctl, KEYCTL_SET_TIMEOUT, key_id_2, 100);
    //staptest// keyctl (KEYCTL_SET_TIMEOUT, NNNN, 100) = 0
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = 0

    // (*) Retrieve a key's security context
#ifdef KEYCTL_GET_SECURITY
    syscall(__NR_keyctl, KEYCTL_GET_SECURITY, key_id, NULL, 0);
    //staptest// keyctl (KEYCTL_GET_SECURITY, NNNN, 0x0, 0) = NNNN
#endif

    // (*) Give the parent process a new session keyring
    syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, NULL);
    //staptest// keyctl (KEYCTL_JOIN_SESSION_KEYRING, 0x0) = NNNN

    // following would cause irreversible change, so we'll skip testing it:
    // syscall(__NR_keyctl, KEYCTL_SESSION_TO_PARENT);

    // (*) Remove dead keys from the session keyring tree
    // see "Unlink a key from a keyring or the session keyring tree"

    // (*) Remove matching keys from the session keyring tree
    // see "Unlink a key from a keyring or the session keyring tree"

    // (*) Get persistent keyring
#ifdef KEYCTL_GET_PERSISTENT
    ring_id_2 = syscall(__NR_keyctl, KEYCTL_GET_PERSISTENT, (unsigned long)-1, ring_id);
    //staptest// keyctl (KEYCTL_GET_PERSISTENT, -1, NNNN) = NNNN

    syscall(__NR_keyctl, KEYCTL_UNLINK, ring_id_2, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = 0
#endif

    // --- test normal operation on commands that weren't covered by named `man keyctl` scenarios


    // KEYCTL_INVALIDATE
#ifdef KEYCTL_INVALIDATE
    syscall(__NR_keyctl, KEYCTL_INVALIDATE, key_id);
    //staptest// keyctl (KEYCTL_INVALIDATE, NNNN) = 0
#else
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id, ring_id);
    //staptest// keyctl (KEYCTL_UNLINK, NNNN, NNNN) = 0
#endif

    // KEYCTL_GET_KEYRING_ID
    syscall(__NR_keyctl, KEYCTL_GET_KEYRING_ID, KEY_SPEC_PROCESS_KEYRING, 1);
    //staptest// keyctl (KEYCTL_GET_KEYRING_ID, KEY_SPEC_PROCESS_KEYRING, 1) = NNNN

    // KEYCTL_SET_REQKEY_KEYRING
    syscall(__NR_keyctl, KEYCTL_SET_REQKEY_KEYRING, KEY_REQKEY_DEFL_NO_CHANGE);
    //staptest// keyctl (KEYCTL_SET_REQKEY_KEYRING, KEY_REQKEY_DEFL_NO_CHANGE) = 0

    // --- test ugly calls

    syscall(__NR_keyctl, (int)-1, (unsigned long)-1, (unsigned long)-1, (unsigned long)-1, (unsigned long)-1);
    //staptest// keyctl (-1, -1, -1, -1, -1) = NNNN (EOPNOTSUPP)

    __add_key((const char *)-1, (const char *)-1, (const char *)-1, -1, -1);
#ifdef __s390__
#if __WORDSIZE == 64
    //staptest// add_key ([7]?[f]+, [7]?[f]+, [7]?[f]+, 18446744073709551615, -1) = NNNN (EINVAL)
#else
    //staptest// add_key ([7]?[f]+, [7]?[f]+, [7]?[f]+, 4294967295, -1) = NNNN (EINVAL)
#endif
#else
#if __WORDSIZE == 64
    //staptest// add_key ([f]+, [f]+, [f]+, 18446744073709551615, -1) = NNNN (EINVAL)
#else
    //staptest// add_key ([f]+, [f]+, [f]+, 4294967295, -1) = NNNN (EINVAL)
#endif
#endif

    __request_key((const char *)-1, (const char *)-1, (const char *)-1, -1);
#ifdef __s390__
    //staptest// request_key ([7]?[f]+, [7]?[f]+, 0x[7]?[f]+, -1) = -14 (EFAULT)
#else
    //staptest// request_key ([7]?[f]+, [7]?[f]+, 0x[f]+, -1) = -14 (EFAULT)
#endif

    return 0;
}
