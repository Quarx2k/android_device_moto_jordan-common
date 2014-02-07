#include <linux/spinlock.h>
#include <linux/module.h>

#if !((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)) && (defined(CONFIG_UML) || defined(CONFIG_X86))) && !((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)) && defined(CONFIG_ARM) && !defined(CONFIG_GENERIC_ATOMIC64))

static DEFINE_SPINLOCK(lock);
#define NR_LOCKS        16
static union {
         spinlock_t lock;
         char pad[L1_CACHE_BYTES];
} atomic64_lock[NR_LOCKS] __cacheline_aligned_in_smp = {
         [0 ... (NR_LOCKS - 1)] = {
                 .lock =  __SPIN_LOCK_UNLOCKED(atomic64_lock.lock),
         },
};
static inline spinlock_t *lock_addr(const atomic64_t *v)
{
         unsigned long addr = (unsigned long) v;
 
         addr >>= L1_CACHE_SHIFT;
         addr ^= (addr >> 8) ^ (addr >> 16);
         return &atomic64_lock[addr & (NR_LOCKS - 1)].lock;
}

long long atomic64_read(const atomic64_t *v)
{
    unsigned long flags;
    long long val;

    spin_lock_irqsave(&lock, flags);
    val = v->counter;
    spin_unlock_irqrestore(&lock, flags);
    return val;
}
EXPORT_SYMBOL_GPL(atomic64_read);

long long atomic64_add_return(long long a, atomic64_t *v)
{
    unsigned long flags;
    long long val;

    spin_lock_irqsave(&lock, flags);
    val = v->counter += a;
    spin_unlock_irqrestore(&lock, flags);
    return val;
}
EXPORT_SYMBOL_GPL(atomic64_add_return);

void atomic64_set(atomic64_t *v, long long i)
{
         unsigned long flags;
         spinlock_t *lock = lock_addr(v);
 
         spin_lock_irqsave(lock, flags);
         v->counter = i;
         spin_unlock_irqrestore(lock, flags);
}
EXPORT_SYMBOL(atomic64_set);

#endif

