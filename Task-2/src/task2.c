#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <pthread.h>
#include <time.h>

#define RING_CAPACITY 65536
#define RING_MASK (RING_CAPACITY - 1)
#define TOTAL_OPS 10000000ULL
#define L1_CACHE_LINE 64

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #define cpu_relax() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define cpu_relax() __asm__ __volatile__("isb sy" ::: "memory")
#else
    #define cpu_relax() ((void)0)
#endif

typedef struct {
    alignas(L1_CACHE_LINE) _Atomic uint64_t head;
    alignas(L1_CACHE_LINE) _Atomic uint64_t tail;
    alignas(L1_CACHE_LINE) uint64_t ring[RING_CAPACITY];
} spsc_queue_t;

typedef struct {
    spsc_queue_t *q;
    uint64_t ops;
    uint64_t checksum;
} worker_ctx_t;

static inline bool spsc_push(spsc_queue_t *q, uint64_t val) {
    uint64_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint64_t t = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (h - t >= RING_CAPACITY) return false;

    q->ring[h & RING_MASK] = val;
    atomic_store_explicit(&q->head, h + 1, memory_order_release);
    return true;
}

static inline bool spsc_pop(spsc_queue_t *q, uint64_t *val) {
    uint64_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint64_t h = atomic_load_explicit(&q->head, memory_order_acquire);

    if (t == h) return false;

    *val = q->ring[t & RING_MASK];
    atomic_store_explicit(&q->tail, t + 1, memory_order_release);
    return true;
}

static void *producer_task(void *arg) {
    worker_ctx_t *ctx = (worker_ctx_t *)arg;
    spsc_queue_t *q = ctx->q;
    uint64_t acc = 0;

    for (uint64_t i = 1; i <= ctx->ops; ++i) {
        acc += i;
        while (!spsc_push(q, i)) cpu_relax();
    }

    ctx->checksum = acc;
    return NULL;
}

static void *consumer_task(void *arg) {
    worker_ctx_t *ctx = (worker_ctx_t *)arg;
    spsc_queue_t *q = ctx->q;
    uint64_t acc = 0;
    uint64_t val = 0;

    for (uint64_t i = 0; i < ctx->ops; ++i) {
        while (!spsc_pop(q, &val)) cpu_relax();
        acc += val;
    }

    ctx->checksum = acc;
    return NULL;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    spsc_queue_t *q = NULL;
    if (posix_memalign((void **)&q, L1_CACHE_LINE, sizeof(spsc_queue_t)) != 0) return 1;

    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);

    pthread_t prod, cons;
    worker_ctx_t prod_ctx = { .q = q, .ops = TOTAL_OPS, .checksum = 0 };
    worker_ctx_t cons_ctx = { .q = q, .ops = TOTAL_OPS, .checksum = 0 };

    double t0 = now_sec();

    pthread_create(&prod, NULL, producer_task, &prod_ctx);
    pthread_create(&cons, NULL, consumer_task, &cons_ctx);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    double dt = now_sec() - t0;
    double mops = ((double)TOTAL_OPS / dt) / 1e6;

    printf("Items Processed : %llu\n", (unsigned long long)TOTAL_OPS);
    printf("Producer Sum    : %llu\n", (unsigned long long)prod_ctx.checksum);
    printf("Consumer Sum    : %llu\n", (unsigned long long)cons_ctx.checksum);
    printf("Verification    : %s\n", (prod_ctx.checksum == cons_ctx.checksum) ? "PASSED" : "FAILED");
    printf("Execution Time  : %.4f s\n", dt);
    printf("Throughput      : %.2f Mops/s\n", mops);

    free(q);
    return (prod_ctx.checksum == cons_ctx.checksum) ? 0 : 1;
}
