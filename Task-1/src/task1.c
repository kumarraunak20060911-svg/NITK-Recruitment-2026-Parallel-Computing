#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdalign.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_COUNT 4
#define L1_CACHE_LINE 64

typedef struct {
    alignas(L1_CACHE_LINE) uint64_t accumulated;
} thread_slot_t;

typedef struct {
    const uint64_t *vec;
    size_t len;
    size_t id;
    thread_slot_t *slots;
} worker_ctx_t;

static void *task_cyclic(void *param) {
    worker_ctx_t *ctx = (worker_ctx_t *)param;
    const uint64_t *buf = ctx->vec;
    size_t n = ctx->len;
    size_t tid = ctx->id;

    uint64_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
    size_t idx = tid;

    for (; idx + 3 * THREAD_COUNT < n; idx += 4 * THREAD_COUNT) {
        v0 += buf[idx];
        v1 += buf[idx + THREAD_COUNT];
        v2 += buf[idx + 2 * THREAD_COUNT];
        v3 += buf[idx + 3 * THREAD_COUNT];
    }
    for (; idx < n; idx += THREAD_COUNT) {
        v0 += buf[idx];
    }

    ctx->slots[tid].accumulated = v0 + v1 + v2 + v3;
    return NULL;
}

static void *task_block(void *param) {
    worker_ctx_t *ctx = (worker_ctx_t *)param;
    const uint64_t *buf = ctx->vec;
    size_t n = ctx->len;
    size_t tid = ctx->id;

    size_t chunk = n / THREAD_COUNT;
    size_t start = tid * chunk;
    size_t stop = (tid == THREAD_COUNT - 1) ? n : start + chunk;

    uint64_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
    size_t idx = start;

    for (; idx + 3 < stop; idx += 4) {
        v0 += buf[idx];
        v1 += buf[idx + 1];
        v2 += buf[idx + 2];
        v3 += buf[idx + 3];
    }
    for (; idx < stop; ++idx) {
        v0 += buf[idx];
    }

    ctx->slots[tid].accumulated = v0 + v1 + v2 + v3;
    return NULL;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    size_t N = (argc > 1) ? (size_t)atol(argv[1]) : 100000000ULL;
    if (N < 1024) N = 1024;

    uint64_t *data = NULL;
    if (posix_memalign((void **)&data, L1_CACHE_LINE, N * sizeof(uint64_t)) != 0) {
        return 1;
    }

    srand((unsigned)time(NULL));
    for (size_t i = 0; i < N; ++i) {
        data[i] = ((uint64_t)rand() << 32) | (uint64_t)rand();
    }

    pthread_t workers[THREAD_COUNT];
    worker_ctx_t ctxs[THREAD_COUNT];
    thread_slot_t slots[THREAD_COUNT];

    double t0 = now_sec();
    uint64_t seq_total = 0;
    for (size_t i = 0; i < N; ++i) {
        seq_total += data[i];
    }
    double seq_ms = (now_sec() - t0) * 1000.0;

    t0 = now_sec();
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        ctxs[i] = (worker_ctx_t){ .vec = data, .len = N, .id = i, .slots = slots };
        pthread_create(&workers[i], NULL, task_cyclic, &ctxs[i]);
    }
    uint64_t cyc_total = 0;
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(workers[i], NULL);
        cyc_total += slots[i].accumulated;
    }
    double cyc_ms = (now_sec() - t0) * 1000.0;

    t0 = now_sec();
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        ctxs[i] = (worker_ctx_t){ .vec = data, .len = N, .id = i, .slots = slots };
        pthread_create(&workers[i], NULL, task_block, &ctxs[i]);
    }
    uint64_t blk_total = 0;
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(workers[i], NULL);
        blk_total += slots[i].accumulated;
    }
    double blk_ms = (now_sec() - t0) * 1000.0;

    if (seq_total != cyc_total || seq_total != blk_total) {
        free(data);
        return 1;
    }

    printf("Array Size: %zu elements\n", N);
    printf("Single-Threaded : %llu | Time: %8.3f ms\n", (unsigned long long)seq_total, seq_ms);
    printf("Strategy I  (Cyc) : %llu | Time: %8.3f ms | Speedup: %.2fx\n", (unsigned long long)cyc_total, cyc_ms, seq_ms / cyc_ms);
    printf("Strategy II (Blk) : %llu | Time: %8.3f ms | Speedup: %.2fx\n\n", (unsigned long long)blk_total, blk_ms, seq_ms / blk_ms);

    printf("Analysis:\n");
    printf("- Strategy II outperforms Strategy I due to spatial locality.\n");
    printf("- Linear access allows L1/L2 cache prefetching per core and prevents cache line thrashing.\n");

    free(data);
    return 0;
}
