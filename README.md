# Homework 4: Concurrency

## Overview

This assignment explores concurrent programming by modifying a non-thread-safe hash table to support multi-threaded insertions and retrievals. We implemented and analyzed three different synchronization approaches: global mutexes, spinlocks, and fine-grained per-bucket mutexes.

## Part 1: Understanding the Problem and Mutex Implementation

### What causes entries to get lost?

When running the hash table with multiple threads, entries get "lost" - they're inserted but can't be found during retrieval. This happens because of a race condition in the insert function.

The core issue is in the insert operation which does three things:

1. Reads what's currently at `table[i]`
2. Creates a new entry that points to what was just read
3. Updates `table[i]` to point to the new entry

Here's what goes wrong when two threads insert simultaneously:

```
Time  Thread A                        Thread B
----  ------------------------------  ------------------------------
1     Reads table[i] = NULL
2                                     Reads table[i] = NULL
3     Creates entry A, points to NULL
4                                     Creates entry B, points to NULL
5     Writes table[i] = A
6                                     Writes table[i] = B (overwrites A!)
```

Thread B's write overwrites Thread A's entry. Now entry A is lost - there's no way to find it anymore even though it was created and allocated memory.

The fundamental problem is that the read-modify-write sequence happens without any protection. Multiple threads can interleave these operations and corrupt the data structure.

### Solution: parallel_mutex.c

I added a single global mutex to protect the entire hash table:

1. Added global mutex variable: `pthread_mutex_t table_mutex`
2. Initialized it in main with `pthread_mutex_init(&table_mutex, NULL)`
3. In `insert()`: lock before accessing table, unlock after done
4. In `retrieve()`: same pattern - lock before searching, unlock when done
5. Clean up at end with `pthread_mutex_destroy(&table_mutex)`

Now only one thread can access the hash table at a time, preventing race conditions entirely.

### Performance Analysis

Testing with various thread counts showed significant overhead:

| Threads | Original Insert (s) | Mutex Insert (s) | Original Retrieve (s) | Mutex Retrieve (s) |
|---------|---------------------|------------------|----------------------|-------------------|
| 1       | 0.006               | 0.007            | 4.03                 | 4.05              |
| 2       | 0.002               | 0.005            | 0.68                 | 1.80              |
| 4       | 0.002               | 0.008            | 0.45                 | 2.50              |
| 8       | 0.002               | 0.010            | 0.25                 | 2.98              |

The mutex adds huge overhead during retrieval as we add more threads. With 8 threads, the mutex version takes 2.98 seconds compared to 0.25 seconds for the original - approximately 1092% overhead (11.9x slower).

This dramatic increase happens because the global mutex completely serializes all operations. The overhead comes from:

1. Lock contention: Only one thread holds the mutex at a time, serializing everything
2. Context switching: Threads sleep when they can't get the lock and must be woken up later
3. Cache coherence: The mutex must stay consistent across CPU cores, causing cache invalidations

With 1 thread, overhead is minimal (~0.5%) since there's no contention. But with 8 threads competing for the same lock, serialization dominates performance. The retrieval phase suffers more because walking through linked lists takes longer than insertions - longer critical sections mean other threads wait longer.

---

## Part 2: Spinlock Implementation

### Initial Hypothesis

I expected spinlocks would perform about the same or worse than mutexes, especially with many threads.

My reasoning: Mutexes put threads to sleep when they can't acquire the lock, freeing up CPU for other work. Spinlocks continuously loop checking if the lock is free, burning CPU cycles while waiting.

For our hash table, I figured spinlocks would be worse because:

1. Our critical sections are relatively long (memory allocation, list walking)
2. We have high contention - many threads competing over just 5 buckets
3. All that spinning wastes CPU time that could be used by the thread holding the lock

Spinlocks work well when critical sections are very short and there's low contention. But with our longer operations, I expected busy-waiting to waste more than it saves.

### Implementation: parallel_spin.c

Since macOS doesn't have built-in pthread spinlocks, I implemented my own using C11 atomic operations:

```c
typedef atomic_flag spinlock_t;
spinlock_t table_spinlock = ATOMIC_FLAG_INIT;

void spin_lock(spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        // Spin (busy-wait)
    }
}

void spin_unlock(spinlock_t *lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}
```

### Results

| Threads | Original (s) | Mutex (s) | Spinlock (s) |
|---------|--------------|-----------|--------------|
| 1       | 4.03         | 4.05      | 4.10         |
| 2       | 0.68         | 1.80      | 2.35         |
| 4       | 0.45         | 2.50      | 3.80         |
| 8       | 0.25         | 2.98      | 4.82         |

Spinlocks add approximately 1828% overhead compared to the original at 8 threads (19.3x slower), and perform about 62% worse than mutexes (1.62x slower than mutex version).

My hypothesis was correct - spinlocks perform poorly for this workload:

1. With 8 threads and only 5 buckets, we have multiple threads spinning and burning CPU
2. Spinlocks never yield the CPU, so spinning threads compete with the thread doing actual work
3. Our operations (malloc, list walking) are long enough that busy-waiting is wasteful
4. More threads means more spinners wasting more cycles - it scales poorly

The data clearly shows spinlocks only make sense for very short critical sections (just a few instructions) with low contention. Our workload is the opposite.

---

## Part 3: Retrieve Parallelization

### Do we need a lock for retrieval?

Yes, we need some form of synchronization, but we can optimize it significantly.

Even though retrieve only reads data, we still need protection. While one thread walks through a linked list in a bucket, another thread might be modifying that same list by inserting. If the `table[i]` pointer or a `next` pointer changes while we're traversing, we could miss entries, follow dangling pointers, or read corrupted data.

However, this is a classic readers-writers problem. Multiple retrievals can safely happen simultaneously if they're working on different buckets (completely independent), or even the same bucket (both just reading). The only real conflict is when a retrieval and insertion target the same bucket simultaneously.

### Changes Made: parallel_mutex_opt.c

The key optimization is switching from one global mutex to per-bucket mutexes.

Instead of one lock for the entire table, I created an array:

```c
pthread_mutex_t bucket_mutexes[NUM_BUCKETS];
```

Then in both insert and retrieve, I only lock the specific bucket being accessed:

```c
// In insert()
int i = key % NUM_BUCKETS;
pthread_mutex_lock(&bucket_mutexes[i]);
// ... do the insert ...
pthread_mutex_unlock(&bucket_mutexes[i]);

// In retrieve()
int i = key % NUM_BUCKETS;
pthread_mutex_lock(&bucket_mutexes[i]);
// ... search for the key ...
pthread_mutex_unlock(&bucket_mutexes[i]);
```

I initialize all 5 mutexes at startup:

```c
for (i = 0; i < NUM_BUCKETS; i++) {
    pthread_mutex_init(&bucket_mutexes[i], NULL);
}
```

This optimization works because threads working on different buckets no longer block each other. With 5 buckets and random key distribution, we get approximately 5x more parallelism. Retrievals can now run simultaneously as long as they're accessing different buckets.

I considered using reader-writer locks to allow multiple reads of the same bucket simultaneously, but that adds complexity and rwlocks have their own overhead. The per-bucket mutex approach provides a good balance.

---

## Part 4: Insert Parallelization

### When can insertions be safely parallelized?

Multiple insertions can happen safely when they target different buckets.

Our hash table has 5 buckets (NUM_BUCKETS = 5). Each bucket is an independent linked list. When inserting a key, we determine the bucket using: `bucket_index = key % NUM_BUCKETS`

Safe scenario:

- Thread A inserts key 10 (goes to bucket 0)
- Thread B inserts key 11 (goes to bucket 1)
- These are completely independent - they modify different lists, no conflict

Unsafe scenario:

- Thread A inserts key 10 (goes to bucket 0)
- Thread B inserts key 15 (15 % 5 = 0, also bucket 0)
- These conflict - both trying to modify the same list head simultaneously

### How this is handled

The per-bucket mutex approach from Part 3 already handles insert parallelization. The same code works for both operations. Each bucket gets its own mutex, so threads inserting into different buckets don't block each other. Threads inserting into the same bucket serialize on that bucket's mutex.

With random keys spread across 5 buckets, we get approximately 5x parallelism.

### Performance Improvement

With 8 threads:

- Global mutex: insert 0.010s, retrieve 2.98s
- Per-bucket mutex: insert 0.010s, retrieve 0.87s

The retrieve speedup is dramatic - approximately 3.4x faster! While insert times are similar (both very fast), the retrieval phase benefits enormously from reduced lock contention.

---

## Summary

All three implementations work correctly with no lost keys:

1. **parallel_mutex.c** - Uses one global mutex. Simple to implement but forces complete serialization, eliminating parallelism benefits.

2. **parallel_spin.c** - Uses a custom spinlock (since macOS doesn't have pthread spinlocks). Performs worse than mutexes because threads waste CPU cycles spinning instead of sleeping, especially problematic with longer critical sections.

3. **parallel_mutex_opt.c** - Uses one mutex per bucket. This is the winner - threads can work on different buckets simultaneously, providing approximately 3.4x speedup over the global mutex approach.

### Performance Summary

| Implementation       | 8-Thread Retrieve Time (s) | Overhead vs Original | Correctness    |
|---------------------|---------------------------|---------------------|----------------|
| Original (Unsafe)   | 0.25                      | Baseline            | ❌ Loses keys  |
| Global Mutex        | 2.98                      | 1092% (11.9x)       | ✓ Correct      |
| Spinlock            | 4.82                      | 1828% (19.3x)       | ✓ Correct      |
| Per-Bucket Mutex    | 0.87                      | 248% (3.5x)         | ✓ Correct      |

### Key Lesson

The granularity of locking matters significantly for performance. Fine-grained locks (per-bucket) allow substantially more parallelism than coarse-grained locks (global). While slightly more complex to implement, the performance benefits are dramatic - we achieved 3.4x speedup on retrieval operations. The tradeoff between implementation complexity and performance must be evaluated based on specific workload characteristics.

## Files Included

- `parallel_mutex.c` - Global mutex implementation
- `parallel_spin.c` - Spinlock implementation
- `parallel_mutex_opt.c` - Optimized per-bucket mutex implementation

## Testing

All implementations were tested with 1, 2, 4, and 8 threads. The correct implementations (mutex, spinlock, and optimized) all show "0 keys lost" across all threads, confirming correctness.
