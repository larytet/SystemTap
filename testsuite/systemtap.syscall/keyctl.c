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

// On arm (3.11.10-301.fc20.armv7hl), the KEYCTL_GET_PERSISTENT code
// causes a SIGSEGV.
#if defined(__arm__) && defined(KEYCTL_GET_PERSISTENT)
#undef KEYCTL_GET_PERSISTENT
#endif

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
    key_serial_t ring_id, ring_id_2, ring_id_3, key_id, key_id_2;

    // --- test normal operation for keyctl manpage scenarios

    // (*) Get a keyring to work with
    ring_id = syscall(__NR_keyctl, KEYCTL_GET_KEYRING_ID, KEY_SPEC_SESSION_KEYRING, 0);
    //staptest// [[[[keyctl (KEYCTL_GET_KEYRING_ID, KEY_SPEC_SESSION_KEYRING, 0)!!!!ni_syscall ()]]]] = NNNN

    // (*) Add a key to a keyring
    key_id = __add_key("user", "testkey", "somedata", strlen("somedata"), ring_id);
    //staptest// add_key ("user", "testkey", "somedata", 8, NNNN) = NNNN

    // (*) Create a keyring
    ring_id_2 = __add_key("keyring", "newring2", NULL, 0, ring_id);
    //staptest// add_key ("keyring", "newring2",\ +0x0, 0, NNNN) = NNNN

    // (*) Get persistent keyring
#ifdef KEYCTL_GET_PERSISTENT
    ring_id_3 = syscall(__NR_keyctl, KEYCTL_GET_PERSISTENT, (unsigned long)-1, ring_id_2);
    //staptest// [[[[keyctl (KEYCTL_GET_PERSISTENT, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    syscall(__NR_keyctl, KEYCTL_UNLINK, ring_id_3, ring_id_2);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN
#endif

    // (*) Request a key
    __request_key("user", "testkey", 0, ring_id);
    //staptest// request_key ("user", "testkey", 0x0, NNNN) = NNNN

    // (*) Update a key
    syscall(__NR_keyctl, KEYCTL_UPDATE, key_id, "blah", 4);
    //staptest// [[[[keyctl (KEYCTL_UPDATE, NNNN, "blah", 4)!!!!ni_syscall ()]]]] = NNNN

    // (*) Revoke a key
    key_id_2 = __add_key("user", "testkey2", "somedata2", strlen("somedata2"), ring_id);
    //staptest// add_key ("user", "testkey2", "somedata2", 9, NNNN) = NNNN
    syscall(__NR_keyctl, KEYCTL_REVOKE, key_id_2);
    //staptest// [[[[keyctl (KEYCTL_REVOKE, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Clear a keyring
    syscall(__NR_keyctl, KEYCTL_CLEAR, ring_id_2);
    //staptest// [[[[keyctl (KEYCTL_CLEAR, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // Remove unneeded keyring
    syscall(__NR_keyctl, KEYCTL_UNLINK, ring_id_2, ring_id);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Link a key to a keyring
    syscall(__NR_keyctl, KEYCTL_LINK, key_id, ring_id);
    //staptest// [[[[keyctl (KEYCTL_LINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Unlink a key from a keyring or the session keyring tree
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Search a keyring
    syscall(__NR_keyctl, KEYCTL_SEARCH, ring_id, "user", "testkey", 0);
    //staptest// [[[[keyctl (KEYCTL_SEARCH, NNNN, "user", "testkey", 0)!!!!ni_syscall ()]]]] = NNNN

    // (*) Read a key
    // see "Get a keyring to work with" above

    // (*) List a keyring
    // see "Get a keyring to work with" above plus "Describe a key" below

    // (*) Describe a key
    syscall(__NR_keyctl, KEYCTL_DESCRIBE, key_id, NULL, 0);
    //staptest// [[[[keyctl (KEYCTL_DESCRIBE, NNNN, 0x0, 0)!!!!ni_syscall ()]]]] = NNNN

    // (*) Change the access controls on a key
    syscall(__NR_keyctl, KEYCTL_CHOWN, key_id, getuid(), (unsigned long)-1);
    //staptest// [[[[keyctl (KEYCTL_CHOWN, NNNN, NNNN, -1)!!!!ni_syscall ()]]]] = NNNN
    syscall(__NR_keyctl, KEYCTL_CHOWN, key_id, (unsigned long)-1, getgid());
    //staptest// [[[[keyctl (KEYCTL_CHOWN, NNNN, -1, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Set the permissions mask on a key
    syscall(__NR_keyctl, KEYCTL_SETPERM, key_id, 0x3f3f3f3f);
    // on rhel[56] i686 ths max string length appears to be too short
    // to accomodate whole the perm string:
    //staptest// [[[[keyctl (KEYCTL_SETPERM, NNNN, KEY_[^ ]+)!!!!ni_syscall ()]]]] = NNNN

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
    //staptest// [[[[keyctl (KEYCTL_ASSUME_AUTHORITY, NNNN)!!!!ni_syscall ()]]]] = NNNN
    syscall(__NR_keyctl, KEYCTL_INSTANTIATE, key_id_2, "somedata2", strlen("somedata2"), ring_id);
    //staptest// [[[[keyctl (KEYCTL_INSTANTIATE, NNNN, "somedata2", 9, NNNN)!!!!ni_syscall ()]]]] = NNNN

#ifdef KEYCTL_INSTANTIATE_IOV
#include <sys/uio.h>
    struct iovec iovecs[] = {
        {"data1", 5},
        {"data2", 5}
    };

    syscall(__NR_keyctl, KEYCTL_INSTANTIATE_IOV, key_id_2, &iovecs, 2, ring_id);
    //staptest// [[[[keyctl (KEYCTL_INSTANTIATE_IOV, NNNN, XXXX, 2, NNNN)!!!!ni_syscall ()]]]] = NNNN
#endif
    syscall(__NR_keyctl, KEYCTL_NEGATE, key_id_2, 30, ring_id);
    //staptest// [[[[keyctl (KEYCTL_NEGATE, NNNN, 30, NNNN)!!!!ni_syscall ()]]]] = NNNN
#ifdef KEYCTL_REJECT
    syscall(__NR_keyctl, KEYCTL_REJECT, key_id_2, 30, 64, ring_id);
    //staptest// [[[[keyctl (KEYCTL_REJECT, NNNN, 30, 64, NNNN)!!!!ni_syscall ()]]]] = NNNN
#endif

    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Set the expiry time on a key
    key_id_2 = __add_key("user", "testkey2", "somedata2", strlen("somedata2"), ring_id);
    //staptest// add_key ("user", "testkey2", "somedata2", 9, NNNN) = NNNN
    syscall(__NR_keyctl, KEYCTL_SET_TIMEOUT, key_id_2, 100);
    //staptest// [[[[keyctl (KEYCTL_SET_TIMEOUT, NNNN, 100)!!!!ni_syscall ()]]]] = NNNN
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id_2, ring_id);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    // (*) Retrieve a key's security context
#ifdef KEYCTL_GET_SECURITY
    syscall(__NR_keyctl, KEYCTL_GET_SECURITY, key_id, NULL, 0);
    //staptest// [[[[keyctl (KEYCTL_GET_SECURITY, NNNN, 0x0, 0)!!!!ni_syscall ()]]]] = NNNN
#endif

    // (*) Give the parent process a new session keyring
    syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, NULL);
    //staptest// [[[[keyctl (KEYCTL_JOIN_SESSION_KEYRING, 0x0)!!!!ni_syscall ()]]]] = NNNN

    // following would cause irreversible change, so we'll skip testing it:
    // syscall(__NR_keyctl, KEYCTL_SESSION_TO_PARENT);

    // (*) Remove dead keys from the session keyring tree
    // see "Unlink a key from a keyring or the session keyring tree"

    // (*) Remove matching keys from the session keyring tree
    // see "Unlink a key from a keyring or the session keyring tree"

    // --- test normal operation on commands that weren't covered by named `man keyctl` scenarios


    // KEYCTL_INVALIDATE
#ifdef KEYCTL_INVALIDATE
    syscall(__NR_keyctl, KEYCTL_INVALIDATE, key_id);
    //staptest// [[[[keyctl (KEYCTL_INVALIDATE, NNNN)!!!!ni_syscall ()]]]] = NNNN
#else
    syscall(__NR_keyctl, KEYCTL_UNLINK, key_id, ring_id);
    //staptest// [[[[keyctl (KEYCTL_UNLINK, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN
#endif

    // KEYCTL_GET_KEYRING_ID
    syscall(__NR_keyctl, KEYCTL_GET_KEYRING_ID, KEY_SPEC_PROCESS_KEYRING, 1);
    //staptest// [[[[keyctl (KEYCTL_GET_KEYRING_ID, KEY_SPEC_PROCESS_KEYRING, 1)!!!!ni_syscall ()]]]] = NNNN

    // KEYCTL_SET_REQKEY_KEYRING
    syscall(__NR_keyctl, KEYCTL_SET_REQKEY_KEYRING, KEY_REQKEY_DEFL_NO_CHANGE);
    //staptest// [[[[keyctl (KEYCTL_SET_REQKEY_KEYRING, KEY_REQKEY_DEFL_NO_CHANGE)!!!!ni_syscall ()]]]] = NNNN

    // --- test ugly calls

    syscall(__NR_keyctl, (int)-1, (unsigned long)-1, (unsigned long)-1, (unsigned long)-1, (unsigned long)-1);
    //staptest// [[[[keyctl (-1, -1, -1, -1, -1)!!!!ni_syscall ()]]]] = NNNN

    __add_key((const char *)-1, (const char *)-1, (const char *)-1, -1, -1);
#ifdef __s390__
#if __WORDSIZE == 64
    //staptest// add_key (0x[7]?[f]+, 0x[7]?[f]+, 0x[7]?[f]+, 18446744073709551615, -1) = NNNN (EINVAL)
#else
    //staptest// add_key (0x[7]?[f]+, 0x[7]?[f]+, 0x[7]?[f]+, 4294967295, -1) = NNNN (EINVAL)
#endif
#else
#if __WORDSIZE == 64
    //staptest// add_key (0x[f]+, 0x[f]+, 0x[f]+, 18446744073709551615, -1) = NNNN (EINVAL)
#else
    //staptest// add_key (0x[f]+, 0x[f]+, 0x[f]+, 4294967295, -1) = NNNN (EINVAL)
#endif
#endif

    __request_key((const char *)-1, (const char *)-1, (const char *)-1, -1);
#ifdef __s390__
    //staptest// request_key (0x[7]?[f]+, 0x[7]?[f]+, 0x[7]?[f]+, -1) = -14 (EFAULT)
#else
    //staptest// request_key (0x[7]?[f]+, 0x[7]?[f]+, 0x[f]+, -1) = -14 (EFAULT)
#endif

    return 0;
}
