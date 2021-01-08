#ifndef SPINLOCK_H_
#define SPINLOCK_H_

#define SPIN_INIT(q) spinlock_init(&(q)->lock);
#define SPIN_LOCK(q) spinlock_lock(&(q)->lock);
#define SPIN_UNLOCK(q) spinlock_unlock(&(q)->lock);
#define SPIN_DESTROY(q) spinlock_destroy(&(q)->lock);

typedef struct {
    int lock;
} sge_spinlock;

static inline void
spinlock_init(sge_spinlock *lock) {
    lock->lock = 0;
}

static inline void
spinlock_lock(sge_spinlock *lock) {
    while (__sync_lock_test_and_set(&lock->lock,1)) {}
}

static inline int
spinlock_trylock(sge_spinlock *lock) {
    return __sync_lock_test_and_set(&lock->lock,1) == 0;
}

static inline void
spinlock_unlock(sge_spinlock *lock) {
    __sync_lock_release(&lock->lock);
}

static inline void
spinlock_destroy(sge_spinlock *lock) {
    (void) lock;
}


#endif
