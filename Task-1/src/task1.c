#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define NUM_THREADS 4

uint64_t *arr;
uint64_t n;

struct data {
    int id;
    uint64_t total;
};

uint64_t sum_seq() {
    uint64_t s = 0;
    for (uint64_t i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

void* worker1(void* arg) {
    struct data* d = (struct data*) arg;
    uint64_t s = 0;

    for (uint64_t i = d->id; i < n; i += NUM_THREADS) {
        s += arr[i];
    }

    d->total = s;
    return NULL;
}

uint64_t run_strat1() {
    pthread_t t[NUM_THREADS];
    struct data d[NUM_THREADS];
    uint64_t s = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        d[i].id = i;
        d[i].total = 0;
        pthread_create(&t[i], NULL, worker1, &d[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(t[i], NULL);
        s += d[i].total;
    }

    return s;
}

void* worker2(void* arg) {
    struct data* d = (struct data*) arg;
    uint64_t s = 0;

    uint64_t start = d->id * (n / NUM_THREADS);
    uint64_t end = (d->id == NUM_THREADS - 1) ? n : (d->id + 1) * (n / NUM_THREADS);

    for (uint64_t i = start; i < end; i++) {
        s += arr[i];
    }

    d->total = s;
    return NULL;
}

uint64_t run_strat2() {
    pthread_t t[NUM_THREADS];
    struct data d[NUM_THREADS];
    uint64_t s = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        d[i].id = i;
        d[i].total = 0;
        pthread_create(&t[i], NULL, worker2, &d[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(t[i], NULL);
        s += d[i].total;
    }

    return s;
}

int main() {
    printf("Enter array size (N): ");
    if (scanf("%lu", &n) != 1 || n < 1024) {
        printf("Invalid N! Must be an integer >= 1024.\n");
        return 1;
    }

    arr = (uint64_t*) malloc(n * sizeof(uint64_t));
    if (arr == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    srand(42);
    for (uint64_t i = 0; i < n; i++) {
        arr[i] = ((uint64_t)rand() << 32) | rand();
    }

    clock_t start, end;

    start = clock();
    uint64_t s1 = sum_seq();
    end = clock();
    double t1 = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    start = clock();
    uint64_t s2 = run_strat1();
    end = clock();
    double t2 = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    start = clock();
    uint64_t s3 = run_strat2();
    end = clock();
    double t3 = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("\nResults & Validation:\n");
    printf(" - Single-Threaded Sum : %lu\n", s1);
    printf(" - Strategy (i) Sum    : %lu [%s]\n", s2, (s1 == s2) ? "PASSED" : "FAILED");
    printf(" - Strategy (ii) Sum   : %lu [%s]\n\n", s3, (s1 == s3) ? "PASSED" : "FAILED");

    printf("Execution Time Performance:\n");
    printf(" - Single-Threaded    : %.3f ms\n", t1);
    printf(" - Strategy (i) Cyclic: %.3f ms (Speedup: %.2fx)\n", t2, t1 / t2);
    printf(" - Strategy (ii) Block: %.3f ms (Speedup: %.2fx)\n", t3, t1 / t3);

    free(arr);
    return 0;
}
