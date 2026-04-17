#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// ================= SYSTEM CONFIGURATION (DO NOT MODIFY) =================

#define M 3             // waiting lane capacity
#define N 5             // order rail capacity
#define K 2             // number of baristas
#define TOTAL_CARS 15   // total number of cars to simulate

// ================= MONITOR: Lane (차량 대기 차선) =================
// 공유자원: lane_queue (대기 차선 큐), count (현재 대기 차량 수)
// 조건변수: car_waiting (접수직원이 차량 도착을 대기)
// 프로시저: lane_enter(), lane_dequeue()

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  car_waiting;
    int queue[M];
    int in, out, count;
} LaneMonitor;

// ================= MONITOR: Rail (주문 레일) =================
// 공유자원: rail_queue (주문 레일 큐), count (현재 레일 위 주문 수)
// 조건변수: has_order (바리스타가 주문 대기), has_space (접수직원이 빈 슬롯 대기)
// 프로시저: rail_place(), rail_pickup()

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  has_order;
    pthread_cond_t  has_space;
    int queue[N];
    int in, out, count;
} RailMonitor;

LaneMonitor lane;
RailMonitor rail;

// ================= TIME HELPER =================

void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// ================= LANE MONITOR PROCEDURES =================

// 차량이 대기 차선에 진입 (가득 찼으면 0 반환 후 이탈, 아니면 1 반환)
int lane_enter(int car_id) {
    // lane 작업 전 mutex lock
    pthread_mutex_lock(&lane.lock);
    // 차선이 가득 찼는지 확인 (race condition 방지)
    if (lane.count == M) {
        pthread_mutex_unlock(&lane.lock);
        print_time();
        printf("Car [%d]: Line too long, driving away.\n", car_id);
        return 0;
    }
    // 대기 차선 큐에 차량 ID 추가
    lane.queue[lane.in % M] = car_id;
    lane.in++;
    lane.count++;
    print_time();
    printf("Car [%d] entered -> waiting in lane. (lane %d/%d)\n", car_id, lane.count, M);

    pthread_cond_signal(&lane.car_waiting);  // csignal: 접수직원에게 차량 도착 알림
    // 작업 끝나고 mutex unlock
    pthread_mutex_unlock(&lane.lock);
    return 1;
}

// 접수직원이 대기 차선에서 다음 차량을 가져옴 (차량 없으면 대기)
int lane_dequeue() {
    // lane 작업 전 mutex lock
    pthread_mutex_lock(&lane.lock);
    // 대기 차선에 차량이 없으면 대기
    while (lane.count == 0)
        pthread_cond_wait(&lane.car_waiting, &lane.lock);  // cwait
    // 대기 차선 큐에서 차량 ID 꺼내기
    int car_id = lane.queue[lane.out % M];
    lane.out++;
    lane.count--;
    print_time();
    printf("Order taker: taking order from Car [%d]. (lane %d/%d)\n", car_id, lane.count, M);
    
    // 작업 끝나고 mutex unlock
    pthread_mutex_unlock(&lane.lock);
    return car_id;
}

// ================= RAIL MONITOR PROCEDURES =================

// 접수직원이 주문서를 레일에 올림 (레일 가득 찼으면 대기)
void rail_place(int car_id) {
    // rail 작업 전 mutex lock
    pthread_mutex_lock(&rail.lock);
    // 레일이 가득 찬 경우 빈 슬롯 대기
    while (rail.count == N)
        pthread_cond_wait(&rail.has_space, &rail.lock);  // cwait
    // 주문서를 레일에 올리기
    rail.queue[rail.in % N] = car_id;
    rail.in++;
    rail.count++;
    print_time();
    printf("Order taker: Car [%d] order slip placed on rail. (rail %d/%d)\n", car_id, rail.count, N);

    pthread_cond_signal(&rail.has_order);  // csignal: 바리스타에게 주문 알림
    // 작업 끝나고 mutex unlock
    pthread_mutex_unlock(&rail.lock);
}

// 바리스타가 레일에서 주문서를 가져옴 (주문 없으면 대기)
int rail_pickup(int barista_id) {
    // rail 작업 전 mutex lock
    pthread_mutex_lock(&rail.lock);
    // 레일에 주문이 없으면 대기
    while (rail.count == 0)
        pthread_cond_wait(&rail.has_order, &rail.lock);  // cwait
    // 레일에서 주문서 가져오기
    int car_id = rail.queue[rail.out % N];
    rail.out++;
    rail.count--;
    print_time();
    printf("Barista [%d]: picked up Car [%d] order, starting preparation. (rail %d/%d)\n",
           barista_id, car_id, rail.count, N);

    pthread_cond_signal(&rail.has_space);  // csignal: 접수직원에게 빈 슬롯 알림
    // 작업 끝나고 mutex unlock
    pthread_mutex_unlock(&rail.lock);
    return car_id;
}

// ================= THREAD FUNCTIONS =================

// 1. 차량 고객 스레드
void* car_thread(void* arg) {
    int car_id = *(int*)arg;
    // 차량 대기 차선 진입
    lane_enter(car_id);
    return NULL;
}

// 2. 주문 접수 직원 스레드
void* taker_thread(void* arg) {
    // 직원은 계속해서 차량을 처리
    while (1) {
        int car_id = lane_dequeue();
        sleep(rand() % 2 + 1);
        // 레일에 주문서 올리기
        rail_place(car_id);
    }
    return NULL;
}

// 3. 바리스타 스레드
void* barista_thread(void* arg) {
    int id = *(int*)arg;
    // 바리스타는 계속해서 주문을 처리
    while (1) {
        int car_id = rail_pickup(id);
        usleep((rand() % 2000 + 3000) * 1000);
        print_time();
        printf("Barista [%d]: Car [%d]'s [Iced Americano] is ready!\n", id, car_id);
    }
    return NULL;
}

// ================= MAIN FUNCTION =================
int main() {
    time_t seed = time(NULL);
    srand(seed);

    printf("[SEED] Random seed for verification: %lu\n", seed);

    // initialize lane monitor
    pthread_mutex_init(&lane.lock, NULL);
    pthread_cond_init(&lane.car_waiting, NULL);
    lane.in = lane.out = lane.count = 0;

    // initialize rail monitor
    pthread_mutex_init(&rail.lock, NULL);
    pthread_cond_init(&rail.has_order, NULL);
    pthread_cond_init(&rail.has_space, NULL);
    rail.in = rail.out = rail.count = 0;

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
    pthread_mutex_destroy(&lane.lock);
    pthread_cond_destroy(&lane.car_waiting);
    pthread_mutex_destroy(&rail.lock);
    pthread_cond_destroy(&rail.has_order);
    pthread_cond_destroy(&rail.has_space);

    return 0;
}
