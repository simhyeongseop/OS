#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

// Returns a timespec set to 'sec' seconds from now
struct timespec get_timeout(int sec) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += sec;
    return ts;
}

void* thread1(void* arg) {
    int retry = 0;

    while (1) {
        printf("[Thread 1] (attempt %d) Trying to lock mutex_a...\n", ++retry);
        pthread_mutex_lock(&mutex_a);
        printf("[Thread 1] Acquired mutex_a! Trying mutex_b...\n");
        sleep(1);  // Give Thread 2 time to acquire mutex_b

        // Timeout after 2 seconds: treat as deadlock detected
        struct timespec ts = get_timeout(2);
        int ret = pthread_mutex_timedlock(&mutex_b, &ts);

        if (ret == 0) {
            // Success: proceed with work
            printf("[Thread 1] Acquired mutex_b! Working...\n");
            sleep(1);
            pthread_mutex_unlock(&mutex_b);
            pthread_mutex_unlock(&mutex_a);
            printf("[Thread 1] Done\n");
            break;
        } else if (ret == ETIMEDOUT) {
            // Detected: timeout means deadlock -> release all locks and recover
            printf("[Thread 1] ★ Deadlock detected! Releasing mutex_a and recovering...\n");
            pthread_mutex_unlock(&mutex_a);  // Recovery: release held lock
            sleep(1);                        // Wait before retrying
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
        sleep(1);  // Give Thread 1 time to acquire mutex_a

        // Timeout after 2 seconds: treat as deadlock detected
        struct timespec ts = get_timeout(2);
        int ret = pthread_mutex_timedlock(&mutex_a, &ts);

        if (ret == 0) {
            // Success: proceed with work
            printf("[Thread 2] Acquired mutex_a! Working...\n");
            sleep(1);
            pthread_mutex_unlock(&mutex_a);
            pthread_mutex_unlock(&mutex_b);
            printf("[Thread 2] Done\n");
            break;
        } else if (ret == ETIMEDOUT) {
            // Detected: timeout means deadlock -> release all locks and recover
            printf("[Thread 2] ★ Deadlock detected! Releasing mutex_b and recovering...\n");
            pthread_mutex_unlock(&mutex_b);  // Recovery: release held lock
            sleep(2);                        // Longer wait than Thread 1 to prevent livelock
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
