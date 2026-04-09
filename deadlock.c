#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* thread1(void* arg) {
    printf("[Thread 1] Trying to lock mutex_a...\n");
    pthread_mutex_lock(&mutex_a);
    printf("[Thread 1] Acquired mutex_a! Waiting before trying mutex_b...\n");
    sleep(1);  // Give Thread 2 time to acquire mutex_b first

    printf("[Thread 1] Trying to lock mutex_b... (will block here)\n");
    pthread_mutex_lock(&mutex_b);
    printf("[Thread 1] Acquired mutex_b! (never reaches here)\n");

    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    return NULL;
}

void* thread2(void* arg) {
    printf("[Thread 2] Trying to lock mutex_b...\n");
    pthread_mutex_lock(&mutex_b);
    printf("[Thread 2] Acquired mutex_b! Waiting before trying mutex_a...\n");
    sleep(1);  // Give Thread 1 time to acquire mutex_a first

    printf("[Thread 2] Trying to lock mutex_a... (will block here)\n");
    pthread_mutex_lock(&mutex_a);
    printf("[Thread 2] Acquired mutex_a! (never reaches here)\n");

    pthread_mutex_unlock(&mutex_a);
    pthread_mutex_unlock(&mutex_b);
    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_mutex_init(&mutex_a, NULL);
    pthread_mutex_init(&mutex_b, NULL);

    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);  // Program hangs here forever

    pthread_mutex_destroy(&mutex_a);
    pthread_mutex_destroy(&mutex_b);
    return 0;
}
