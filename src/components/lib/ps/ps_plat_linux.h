#ifndef PS_PLAT_LINUX_H
#define PS_PLAT_LINUX_H

#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>

static inline void *
ps_plat_alloc(size_t sz) 
{ return mmap(0, sz, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (size_t)0); }

static inline void
ps_plat_free(void *x, size_t sz) 
{ munmap(x, sz); }

#ifndef PS_SLAB_ALLOC
#define PS_SLAB_ALLOC(sz)   ps_plat_alloc(sz)
#endif
#ifndef PS_SLAB_FREE
#define PS_SLAB_FREE(x, sz) ps_plat_free(x, sz)
#endif

#define u16_t unsigned short int
#define u32_t unsigned int
#define u64_t unsigned long long
typedef u64_t ps_tsc_t; 	/* our time-stamp counter representation */
typedef u16_t coreid_t;

#define PS_CACHE_LINE  64
#define PS_CACHE_PAD   (PS_CACHE_LINE*2)
#define PS_WORD        sizeof(long)
#define PS_PACKED      __attribute__((packed))
#define PS_ALIGNED     __attribute__((aligned(PS_CACHE_LINE)))
#define PS_WORDALIGNED __attribute__((aligned(PS_WORD)))
#define PS_NUMCORES    1
#define PS_PAGE_SIZE   4096
#define PS_RNDUP(v, a) (-(-(v) & -(a))) /* from blogs.oracle.com/jwadams/entry/macros_and_powers_of_two */

#define EQUIESCENCE (200)

#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

static inline ps_tsc_t
ps_tsc(void)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d), "=c" (c) : : );

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline ps_tsc_t
ps_tsc_locality(coreid_t *coreid, coreid_t *numaid)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtscp" : "=a" (a), "=d" (d), "=c" (c) : : );
	*coreid = 0; /*c & 0xFFF;*/ 	/* lower 12 bits in Linux = coreid */
	*numaid = c >> 12; 	/* next 8 = socket/numa id */

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline unsigned int
ps_coreid(void)
{
	return 0; 		/*  testing... */
	/* unsigned int coreid, numaid; */

	/* ps_rdtsc_locality(&coreid, &numaid); */

	/* return coreid; */
}


#ifndef ps_cc_barrier
#define ps_cc_barrier() __asm__ __volatile__ ("" : : : "memory")
#endif

#define PS_CAS_INSTRUCTION "cmpxchgq" /* "cmpxchgl" for x86-32 */
#define PS_CAS_STR PS_CAS_INSTRUCTION " %2, %0; setz %1"

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
ps_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
        char z;
        __asm__ __volatile__("lock " PS_CAS_STR
                             : "+m" (*target), "=a" (z)
                             : "q"  (updated), "a"  (old)
                             : "memory", "cc");
        return (int)z;
}

/* 
 * Only atomic on a uni-processor, so not for cross-core coordination.
 * Faster on a multiprocessor when used to synchronize between threads
 * on a single core by avoiding locking.
 */
static inline int
ps_upcas(unsigned long *target, unsigned long old, unsigned long updated)
{
        char z;
        __asm__ __volatile__(PS_CAS_STR
                             : "+m" (*target), "=a" (z)
                             : "q"  (updated), "a"  (old)
                             : "memory", "cc");
        return (int)z;
}

static inline void 
ps_mem_fence(void)
{ __asm__ __volatile__("mfence" ::: "memory"); }


/* FIXME: this is truly horrible for now, but a simple lock for testing */
struct ps_lock {
	unsigned long o;
};

static inline void
ps_lock_take(struct ps_lock *l)
{ while (!ps_cas(&l->o, 0, 1)); }

static inline void
ps_lock_release(struct ps_lock *l)
{ l->o = 0; }


#endif	/* PS_PLAT_LINUX_H */
