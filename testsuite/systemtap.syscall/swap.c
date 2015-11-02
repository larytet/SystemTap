/* COVERAGE: swapon swapoff */
#include <unistd.h>
#include <sys/swap.h>


int main()
{
  /* Actually creating a real swap file here is problematic. It won't
   * work if we're not root or we're on a filesystem that doesn't
   * support swap files (like tmpfs or nfs). So, we'll specify a file
   * that doesn't exist. So, all of the following calls will fail (for
   * various reasons). */

  swapon("foobar_swap", 0);
  //staptest// swapon ("foobar_swap", 0x0) = -NNNN

  swapon("foobar_swap", ((1 << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK) | SWAP_FLAG_PREFER);
  //staptest// swapon ("foobar_swap", SWAP_FLAG_PREFER|1) = -NNNN

  swapon("foobar_swap", ((7 << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK) | SWAP_FLAG_PREFER);
  //staptest// swapon ("foobar_swap", SWAP_FLAG_PREFER|7) = -NNNN

  swapon(0, 0);
  //staptest// swapon ( *0x0, 0x0) =

#ifdef SWAP_FLAG_DISCARD
  swapon("foobar_swap", SWAP_FLAG_DISCARD);
  //staptest// swapon ("foobar_swap", SWAP_FLAG_DISCARD) = -NNNN
#endif

  swapon("foobar_swap", -1);
  //staptest// swapon ("foobar_swap", SWAP_FLAG_[^ ]+|XXXX|32767) = -NNNN

  swapon((char *)-1, SWAP_FLAG_PREFER);
#ifdef __s390__
  //staptest// swapon (0x[7]?[f]+, SWAP_FLAG_PREFER|0) = -NNNN
#else
  //staptest// swapon (0x[f]+, SWAP_FLAG_PREFER|0) = -NNNN
#endif

  swapoff("foobar_swap");
  //staptest// swapoff ("foobar_swap") = -NNNN

  swapoff(0);
  //staptest// swapoff ( *0x0) = -NNNN

  swapoff((char *)-1);
#ifdef __s390__
  //staptest// swapoff (0x[7]?[f]+) = -NNNN
#else
  //staptest// swapoff (0x[f]+) = -NNNN
#endif

  return 0;
}
 
