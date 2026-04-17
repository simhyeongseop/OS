#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// ================= 시스템 설정 (변경금지) =================

#define M 3             // 대기 차로 크기
#define N 5             // 주문서 레일 크기 
#define K 2             // 바리스타 수
#define TOTAL_CARS 15   // 시뮬레이션할 총 차량 수

// ================= 동기화 변수 및 공유 자원 =================

// 주문구역 변수들
pthread_mutex_t lane_mutex;
sem_t cars_in_lane;   // 대기 중인 차량 수
sem_t taker_ready;    // 접수 직원의 주문 받을 준비 완료 신호
int waiting_cars = 0; // 현재 대기 차로에 있는 차량 수

int lane_queue[M];    // 대기 차로 큐 (필요 시 별도 구현 가능)
int lane_in = 0, lane_out = 0;

// 주방구역 변수들
pthread_mutex_t rail_mutex;
sem_t empty_slots;    // 레일의 빈 공간 수 (초기값 N)
sem_t filled_slots;   // 레일에 걸린 주문서 수 (초기값 0)
int rail_count = 0;   // 현재 레일에 걸린 주문서 수

int rail_queue[N];    // 주문서 레일 큐 (필요 시 별도 구현 가능)
int rail_in = 0, rail_out = 0;

// 현재 시간을 출력하기 위한 헬퍼 함수
void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// ================= 스레드 함수 =================

// 1. 차량 고객 스레드
void* car_thread(void* arg) {
    int car_id = *(int*)arg;
    // lane_mutex로 보호
    pthread_mutex_lock(&lane_mutex);

    // (1) mutex 안에서 대기 차로가 full인지 확인 (race condition 방지)
    if (waiting_cars == M) { // 차로에 가득 찬 경우
        pthread_mutex_unlock(&lane_mutex);
        print_time();
        printf("차량 [%d]: 대기 줄이 너무 길어 그냥 지나갑니다(Drive away).\n", car_id);
        return NULL;   // 차량이 대기 줄이 꽉 찼을 때 그냥 지나감.
    }

    // (2) 대기 차로에 차량 ID 저장 후 추가
    lane_queue[lane_in % M] = car_id;
    lane_in++; waiting_cars++;
    print_time();
    printf("차량 [%d] 진입 -> 차로에서 대기합니다. (차로 %d/%d)\n", car_id, waiting_cars, M);
    pthread_mutex_unlock(&lane_mutex);

    // (3) 접수 직원에게 차량 도착 신호
    sem_post(&cars_in_lane);

    return NULL;
}

// 2. 주문 접수 직원 스레드
void* taker_thread(void* arg) {
    while (1) {
        
        // (1) 차량이 올 때까지 세마포어로 대기 (쉬기)
        sem_wait(&cars_in_lane);

        // (2) 대기 차로에서 차량 ID 꺼내기 (lane_mutex로 보호)
        pthread_mutex_lock(&lane_mutex);
        int car_id = lane_queue[lane_out % M];
        lane_out++; waiting_cars--;
        print_time();
        printf("주문 접수 직원: 차량 [%d] 주문 접수 시작. (차로 %d/%d)\n", car_id, waiting_cars, M);
        pthread_mutex_unlock(&lane_mutex);

        // (3) 주문 접수 (랜덤 1-2초)
        sleep(rand() % 2 + 1);

        // (4) 레일이 꽉 찼으면 메시지 출력 후 빈 자리가 날 때까지 대기
        sem_wait(&empty_slots);

        // (5) 주문서를 레일에 올리기 (rail_mutex로 보호)
        pthread_mutex_lock(&rail_mutex);
        rail_queue[rail_in % N] = car_id;
        rail_in++; rail_count++;
        print_time();
        printf("주문 접수 직원: 차량 [%d] 주문서 레일에 등록. (레일 %d/%d)\n", car_id, rail_count, N);
        pthread_mutex_unlock(&rail_mutex);

        sem_post(&filled_slots); // 바리스타에게 주문서 있다고 알려줌
    }
}

// 3. 바리스타 스레드
void* barista_thread(void* arg) {
    while(1) {
        // (1) 주문서 레일에 주문이 올라올 때까지 대기
        sem_wait(&filled_slots);

        // (2) 레일에서 주문서 꺼내기 (rail_mutex로 보호)
        pthread_mutex_lock(&rail_mutex);
        int car_id = rail_queue[rail_out % N]; // 레일에서 주문서 가져오기
        rail_out++; rail_count--;

        print_time();
        printf("바리스타 %d: 레일에서 차량 %d 주문 픽업 및 제조 시작. (레일 %d/%d)\n", *(int*)arg, car_id, rail_count, N);
        
        pthread_mutex_unlock(&rail_mutex);
        sem_post(&empty_slots); // 레일에 빈 공간이 생겼음을 알림

        // (2) 음료 제조 (3~5초, 조건: 주문 시간보다 길어야 함)
        usleep((rand() % 2000 + 3000) * 1000);

        // (3) 제조 완료
        print_time();
        printf("바리스타 [%d]: 차량 [%d]의 [아이스 아메리카노] 제조 완료!\n", *(int*)arg, car_id);
        
    }
}

// ================= 메인 함수 (변경 금지) =================
int main() {
    time_t seed = time(NULL);
    srand(seed);
    
    printf("[SEED] 랜덤 검증용 시드값: %lu\n", seed);

    // Mutex 및 세마포어 초기화
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

    // 직원 스레드 생성
    pthread_create(&taker, NULL, taker_thread, NULL);
    
    // 바리스타 스레드 생성
    for (int i = 0; i < K; i++) {
        barista_ids[i] = i + 1;
        pthread_create(&baristas[i], NULL, barista_thread, &barista_ids[i]);
    }

    // 차량 고객 무작위 도착 시뮬레이션 (0.5초 ~ 2초 간격으로 차량 진입)
    for (int i = 0; i < TOTAL_CARS; i++) {
        car_ids[i] = i + 1;
        pthread_create(&cars[i], NULL, car_thread, &car_ids[i]);
        usleep((rand() % 1500 + 500) * 1000);
    }

    // 모든 차량이 진입하고 처리될 때까지 대기
    for (int i = 0; i < TOTAL_CARS; i++) {
        pthread_join(cars[i], NULL);
    }

    // 바리스타가 남은 주문을 모두 처리할 때 까지 대기
    sleep(10);
    print_time();
    printf("오늘의 영업을 종료합니다.\n");

    // 자원 해제
    pthread_mutex_destroy(&lane_mutex);
    pthread_mutex_destroy(&rail_mutex);
    sem_destroy(&cars_in_lane);
    sem_destroy(&taker_ready);
    sem_destroy(&empty_slots);
    sem_destroy(&filled_slots);

    return 0;
}