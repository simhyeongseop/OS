#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// ================= SYSTEM CONFIGURATION (DO NOT MODIFY) =================

#define M 3             // waiting lane capacity
#define N 5             // order rail capacity
#define K 2             // number of baristas
#define TOTAL_CARS 15   // total number of cars to simulate

// ================= SYNCHRONIZATION VARIABLES & SHARED RESOURCES =================

// order-taking area variables
pthread_mutex_t lane_mutex;
sem_t cars_in_lane;   // number of cars waiting in lane
sem_t taker_ready;    // signal that order taker is ready
int waiting_cars = 0; // current number of cars in the waiting lane

int lane_queue[M];    // waiting lane queue
int lane_in = 0, lane_out = 0;

// kitchen area variables
pthread_mutex_t rail_mutex;
sem_t empty_slots;    // number of empty slots on rail (initial: N)
sem_t filled_slots;   // number of orders on rail (initial: 0)
int rail_count = 0;   // current number of orders on rail

int rail_queue[N];    // order rail queue
int rail_in = 0, rail_out = 0;

// 현재 시간을 출력하기 위한 헬퍼 함수
void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// ================= THREAD FUNCTIONS =================

// 1. car customer thread
void* car_thread(void* arg) {
    int car_id = *(int*)arg;
    // protected by lane_mutex
    pthread_mutex_lock(&lane_mutex);

    // (1) check if lane is full inside mutex (prevents race condition)
    if (waiting_cars == M) {
        pthread_mutex_unlock(&lane_mutex);
        print_time();
        printf("Car [%d]: Line too long, driving away.\n", car_id);
        return NULL;
    }

    // (2) add car ID to waiting lane queue
    lane_queue[lane_in % M] = car_id;
    lane_in++; waiting_cars++;
    print_time();
    printf("Car [%d] entered -> waiting in lane. (lane %d/%d)\n", car_id, waiting_cars, M);
    pthread_mutex_unlock(&lane_mutex);

    // (3) signal order taker that a car has arrived
    sem_post(&cars_in_lane);

    return NULL;
}

// 2. order taker thread
void* taker_thread(void* arg) {
    while (1) {

        // (1) wait for a car to arrive
        sem_wait(&cars_in_lane);

        // (2) dequeue car ID from waiting lane (protected by lane_mutex)
        pthread_mutex_lock(&lane_mutex);
        int car_id = lane_queue[lane_out % M];
        lane_out++; waiting_cars--;
        print_time();
        printf("Order taker: taking order from Car [%d]. (lane %d/%d)\n", car_id, waiting_cars, M);
        pthread_mutex_unlock(&lane_mutex);

        // (3) take order (random 1-2 seconds)
        sleep(rand() % 2 + 1);

        // (4) wait for an empty slot on the rail if full
        sem_wait(&empty_slots);

        // (5) place order slip on rail (protected by rail_mutex)
        pthread_mutex_lock(&rail_mutex);
        rail_queue[rail_in % N] = car_id;
        rail_in++; rail_count++;
        print_time();
        printf("Order taker: Car [%d] order slip placed on rail. (rail %d/%d)\n", car_id, rail_count, N);
        pthread_mutex_unlock(&rail_mutex);

        sem_post(&filled_slots); // notify barista that an order is ready
    }
}

// 3. barista thread
void* barista_thread(void* arg) {
    while(1) {
        // (1) wait until an order slip is on the rail
        sem_wait(&filled_slots);

        // (2) pick up order slip from rail (protected by rail_mutex)
        pthread_mutex_lock(&rail_mutex);
        int car_id = rail_queue[rail_out % N];
        rail_out++; rail_count--;

        print_time();
        printf("Barista %d: picked up Car %d order, starting preparation. (rail %d/%d)\n", *(int*)arg, car_id, rail_count, N);

        pthread_mutex_unlock(&rail_mutex);
        sem_post(&empty_slots); // notify that a rail slot is now free

        // (3) prepare drink (3~5 seconds, must be longer than order-taking time)
        usleep((rand() % 2000 + 3000) * 1000);

        // (4) drink complete
        print_time();
        printf("Barista [%d]: Car [%d]'s [Iced Americano] is ready!\n", *(int*)arg, car_id);
    }
}

// ================= MAIN FUNCTION (DO NOT MODIFY) =================
int main() {
    time_t seed = time(NULL);
    srand(seed);

    printf("[SEED] Random seed for verification: %lu\n", seed);

    // initialize mutexes and semaphores
    pthread_mutex_init(&lane_mutex, NULL);
    pthread_mutex_init(&rail_mutex, NULL);

    sem_init(&cars_in_lane, 0, 0);   // 초기 차량 0
    sem_init(&taker_ready, 0, 0);    // 초기 직원 비활성
    sem_init(&empty_slots, 0, N);    // 초기 레일 빈자리 N
    sem_init(&filled_slots, 0, 0);   // 초기 레일 주문 0

    pthread_t taker;
    pthread_t baristas[K];
    pthread_t cars[TOTAL_CARS];
    int barista_ids[K];
    int car_ids[TOTAL_CARS];

    // create order taker thread
    pthread_create(&taker, NULL, taker_thread, NULL);

    // create barista threads
    for (int i = 0; i < K; i++) {
        barista_ids[i] = i + 1;
        pthread_create(&baristas[i], NULL, barista_thread, &barista_ids[i]);
    }

    // simulate random car arrivals (0.5s ~ 2s intervals)
    for (int i = 0; i < TOTAL_CARS; i++) {
        car_ids[i] = i + 1;
        pthread_create(&cars[i], NULL, car_thread, &car_ids[i]);
        usleep((rand() % 1500 + 500) * 1000);
    }

    // wait until all cars have been processed
    for (int i = 0; i < TOTAL_CARS; i++) {
        pthread_join(cars[i], NULL);
    }

    // wait for baristas to finish remaining orders
    sleep(10);
    print_time();
    printf("Closing for the day.\n");

    // release resources
    pthread_mutex_destroy(&lane_mutex);
    pthread_mutex_destroy(&rail_mutex);
    sem_destroy(&cars_in_lane);
    sem_destroy(&taker_ready);
    sem_destroy(&empty_slots);
    sem_destroy(&filled_slots);

    return 0;
}