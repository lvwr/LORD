#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/prctl.h>
#include <sys/sem.h>

#define SHM_SIZE 8192
#define STACK_MAX 8192

typedef struct
{
  int op;
  long unsigned int arg;
} trace_entry;

typedef struct {
  ushort sem_num;
  short sem_op;
  short sem_flg;
} sembuf;

struct Stack {
  long unsigned int data[STACK_MAX];
  int size;
};

typedef struct Stack Stack;

Stack S[STACK_MAX];

int ops[5] = {0, 0, 0, 0, 0};

void Stack_Init()
{
  S->size = 0;
}

void Stack_Push(long unsigned int d)
{
  if (S->size < STACK_MAX)  S->data[S->size++] = d;
  else fprintf(stderr, "Error: stack full\n");
}

long unsigned int Stack_Pop()
{
  if (S->size == 0) fprintf(stderr, "Error: stack empty\n");
  else {
    S->size--;
    return S->data[S->size];
  }
  return -1;
}

void newop(int op, long unsigned int arg){
  long unsigned int last_call;
  ops[op] = ops[op] + 1;
  if(op == 1 || op == 2){
    Stack_Push(arg);
    return;
  }
  if(op == 3 || op == 4){
    last_call = Stack_Pop();
    if(last_call != arg){
      ops[0]++;
      printf("#WARNING: Unmatching addresses %lx - %lx\n", last_call, arg);
    }
    return;
  }
  printf("#ERROR: Unrecognized operation %d\n", op);
  return;
}

void print_stats(){
  printf("\n******** \nStatus:\n1: %d\n2: %d\n3: %d\n4: %d\n\n\
         Calls: %d\nRets: %d\nWarnings: %d\n\n",
         ops[1], ops[2], ops[3], ops[4], ops[1]+ops[2], ops[3]+ops[4], ops[0]);
}

Stack s;
trace_entry volatile *sheph_sh;
int volatile *tb_counter;

#define MAX_RETRIES 10

int main(){

  int k = 0;
  int i = 0;
  int j = 0;

  Stack_Init();

  printf("reader started\n");
  key_t key1, key2, key3;
  int shmid, shmid2;
  int semid;

  struct sembuf lock;
  lock.sem_num = 0;
  lock.sem_flg = SEM_UNDO;

  // Create buffer
  if ((key1 = ftok("/proc/cpuinfo", 'R')) == -1) {
    perror("ftok");
    exit(1);
  }

  if ((shmid = shmget(key1, SHM_SIZE*sizeof(trace_entry), 0644 | IPC_CREAT)) == -1) {
    perror("shmget");
    exit(1);
  }

  sheph_sh = shmat(shmid, (void *)0, 0);
  if (sheph_sh == (trace_entry *)(-1)) {
    perror("shmat");
    exit(1);
  }

  // Create buffer pointer
  if ((key2 = ftok("/tmp/", 'R')) == -1) {
    perror("ftok");
    exit(1);
  }

  if ((shmid2 = shmget(key2, sizeof(int), 0644 | IPC_CREAT)) == -1) {
    perror("shmget");
    exit(1);
  }

  tb_counter = shmat(shmid2, (void *)0, 0);

  if (tb_counter == (int *)(-1)) {
    perror("shmat");
    exit(1);
  }

  // Create semaphores
  key3 = ftok("/etc/passwd", 'E');
  if ((semid = semget(key3, 1, 0666 | IPC_CREAT)) == -1){
    perror("semget");
    exit(3);
  }

  while(1){
    i++;
    lock.sem_op = -1;
    semop(semid, &lock, 1);

    for(j = 0; j < *tb_counter; j++){
      newop(sheph_sh[j].op, sheph_sh[j].arg);
    }
    *tb_counter = 0;
    lock.sem_op = 1;
    semop(semid, &lock, 1);

    if(i == 20000){
      i = 0;
      printf("Partial Status:");
      print_stats();
    }
    for(k=0; k<40000; k++);
  }
  printf("Final status:\n");
  print_stats();
}
