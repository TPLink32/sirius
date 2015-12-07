#ifndef _SHM_SPINLOCK_H_
#define _SHM_SPINLOCK_H_
/*
 * spin lock implementation on shared memory
 *
 * code borrowed from lithe (http://parlab.eecs.berkeley.edu/lithe)
 */

#ifdef __cplusplus
extern "C" {
#endif

#define LOCKED (1)
#define UNLOCKED (0)

int spinlock_init(int *lock);
int spinlock_trylock(int *lock);
int spinlock_lock(int *lock);
int spinlock_unlock(int *lock);

#ifdef __cplusplus
}
#endif

#endif 

