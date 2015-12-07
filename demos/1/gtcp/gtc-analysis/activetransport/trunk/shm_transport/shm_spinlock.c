/*
 * spin lock implementation on shared memory
 *
 * code borrowed from lithe (http://parlab.eecs.berkeley.edu/lithe)
 */
#include <stdlib.h>
#include "shm_spinlock.h"
#include "shm_atomic.h"


int spinlock_init(int *lock)
{
  if (lock == NULL) {
    return -1;
  }

  *lock = UNLOCKED;

  return 0;
}


int spinlock_trylock(int *lock) 
{
  if (lock == NULL) {
    return -1;
  }

  return cmpxchg(lock, LOCKED, UNLOCKED);
}


int spinlock_lock(int *lock) 
{
  if (lock == NULL) {
    return -1;
  }

  while (spinlock_trylock(lock) != UNLOCKED) {
    atomic_delay();
  }

  return 0;
}


int spinlock_unlock(int *lock) 
{
  if (lock == NULL) {
    return -1;
  }

  *lock = UNLOCKED;

  return 0;
}

