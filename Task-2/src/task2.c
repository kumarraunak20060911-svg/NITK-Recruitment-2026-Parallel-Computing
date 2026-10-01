#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

#define N 8
#define TOTAL 20

typedef struct {
    int arr[N];
    atomic_size_t head;
    atomic_size_t tail;
} RingBuf;

void buf_init(RingBuf *b) {
    b->head = 0;
    b->tail = 0;
}

bool push(RingBuf *b, int x) {
    size_t h = atomic_load(&b->head);
    size_t t = atomic_load(&b->tail);

    if (h - t >= N) {
        return false;
    }

    b->arr[h % N] = x;
    atomic_store(&b->head, h + 1);
    return true;
}

bool pop(RingBuf *b, int *x) {
    size_t t = atomic_load(&b->tail);
    size_t h = atomic_load(&b->head);

    if (t == h) {
        return false;
    }

    *x = b->arr[t % N];
    atomic_store(&b->tail, t + 1);
    return true;
}

int producer(void *arg) {
    RingBuf *b = (RingBuf*)arg;
    for (int i = 1; i <= TOTAL; i++) {
        while (!push(b, i));
        printf("Added: %d\n", i);
    }
    return 0;
}

int consumer(void *arg) {
    RingBuf *b = (RingBuf*)arg;
    int val;
    for (int i = 1; i <= TOTAL; i++) {
        while (!pop(b, &val));
        printf("Removed: %d\n", val);
    }
    return 0;
}

int main() {
    RingBuf b;
    buf_init(&b);

    thrd_t p, c;
    thrd_create(&p, producer, &b);
    thrd_create(&c, consumer, &b);

    thrd_join(p, NULL);
    thrd_join(c, NULL);

    return 0;
}
