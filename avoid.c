#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void* thread1(void* arg) {
    int retry = 0;

    while (1) {
        printf("[Thread 1] (attempt %d) Trying to lock mutex_a...\n", ++retry);
        pthread_mutex_lock(&mutex_a);
        printf("[Thread 1] Acquired mutex_a! Trying mutex_b...\n");

        if (pthread_mutex_trylock(&mutex_b) == 0) {
            // Success: acquired both locks
            printf("[Thread 1] Acquired mutex_b! Working...\n");
            sleep(1);
            pthread_mutex_unlock(&mutex_b);
            pthread_mutex_unlock(&mutex_a);
            printf("[Thread 1] Done\n");
            break;
        } else {
            // Failed: release mutex_a immediately and retry
            printf("[Thread 1] Failed to acquire mutex_b -> releasing mutex_a and retrying\n");
            pthread_mutex_unlock(&mutex_a);
            usleep(100000);  // Wait 0.1s before retrying (yield to other thread)
        }
    }
    return NULL;
}

void* thread2(void* arg) {
    int retry = 0;

    while (1) {
        printf("[Thread 2] (attempt %d) Trying to lock mutex_b...\n", ++retry);
        pthread_mutex_lock(&mutex_b);
        printf("[Thread 2] Acquired mutex_b! Trying mutex_a...\n");

        if (pthread_mutex_trylock(&mutex_a) == 0) {
            // Success: acquired both locks
            printf("[Thread 2] Acquired mutex_a! Working...\n");
            sleep(1);
            pthread_mutex_unlock(&mutex_a);
            pthread_mutex_unlock(&mutex_b);
            printf("[Thread 2] Done\n");
            break;
        } else {
            // Failed: release mutex_b immediately and retry
            printf("[Thread 2] Failed to acquire mutex_a -> releasing mutex_b and retrying\n");
            pthread_mutex_unlock(&mutex_b);
            usleep(150000);  // Slightly longer wait than Thread 1 to prevent livelock
        }
    }
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
