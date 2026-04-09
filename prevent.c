#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* thread1(void* arg) {
    printf("[Thread 1] Trying to lock mutex_a...\n");
    pthread_mutex_lock(&mutex_a);       // Always acquire in order: a -> b
    printf("[Thread 1] Acquired mutex_a!\n");
    sleep(1);

    printf("[Thread 1] Trying to lock mutex_b...\n");
    pthread_mutex_lock(&mutex_b);
    printf("[Thread 1] Acquired both locks! Working...\n");
    sleep(1);

    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    printf("[Thread 1] Done\n");
    return NULL;
}

void* thread2(void* arg) {
    printf("[Thread 2] Trying to lock mutex_a...\n");
    pthread_mutex_lock(&mutex_a);       // Same order as Thread 1: a -> b
    printf("[Thread 2] Acquired mutex_a!\n");
    sleep(1);

    printf("[Thread 2] Trying to lock mutex_b...\n");
    pthread_mutex_lock(&mutex_b);
    printf("[Thread 2] Acquired both locks! Working...\n");
    sleep(1);

    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    printf("[Thread 2] Done\n");
    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);

    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&mutex_a);
    pthread_mutex_destroy(&mutex_b);
    printf("[Main] All threads finished successfully\n");
    return 0;
}
