#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "lord/lord_handler.h"
#include "lord/lord_helper.h"

#define SHM_SIZE 16384

#ifdef SHEPH_SHARED
extern trace_entry *sheph_sh;
extern int *tb_counter;
extern sembuf lock;
extern int semid;

#else
extern FILE* sheph_f;
#endif

extern int addr_tbl[1000];
extern int number_of_addrs;

int strh_to_i(const char *arg){
    int i, a = strlen(arg);
    int value = 0;
    for(i=a; i>0; i--){
        if(arg[a-i]>=48 && arg[a-i]<58){
            value = value + (arg[a-i] - 48) * pow(16, i-1);
        } else if(arg[a-i]>=97 && arg[a-i]<103) {
            value = value + (arg[a-i] - 87) * pow(16, i-1);
        } else {
            printf("An error ocurred while converting inputs - lowert\n");
            exit(1);
        }
    }
    return value;
}

void handle_arg_sh(const char* arg)
{
#ifdef SHEPH_SHARED
	key_t key;
        union semun sem_arg;
        static ushort sem_array[1] = {1};
        int shmid;
        pid_t pID;

        lock.sem_num = 0;
        lock.sem_flg = SEM_UNDO;

        // Create buffer
        if ((key = ftok("/proc/cpuinfo", 'R')) == -1) {
          perror("ftok");
          exit(1);
        }

        if ((shmid = shmget(key, SHM_SIZE*sizeof(trace_entry),
                            0644 | IPC_CREAT)) == -1) {
          perror("shmget");
          exit(1);
        }

        sheph_sh = shmat(shmid, (void *)0, 0);
        if (sheph_sh == (trace_entry *)(-1)) {
          perror("shmat");
          exit(1);
        }

        // Create buffer pointer
        if ((key = ftok("/tmp/", 'R')) == -1) {
          perror("ftok");
          exit(1);
        }

        if ((shmid = shmget(key, sizeof(int), 0644 | IPC_CREAT)) == -1) {
          perror("shmget");
          exit(1);
        }

        tb_counter = shmat(shmid, (void *)0, 0);
        *tb_counter = 0;

        if (tb_counter == (int *)(-1)) {
          perror("shmat");
          exit(1);
        }

        // Create sempahores
        key = ftok("/etc/passwd", 'E');
        if ((semid = semget(key, 1, 0666 | IPC_CREAT)) == -1){
          perror("semget");
          exit(1);
        }

        sem_arg.array=sem_array;
        if (semctl(semid, 1, SETALL, sem_arg) == -1){
          perror("semctl: SETALL");
          exit (3);
        }

        // Fork external analysis
        pID = fork();
        if(pID < 0){
          perror("failed to fork\n");
          exit(1);
        }
        if(pID == 0){
          execv(arg, (char *) 0);
        }

#else
        sheph_f = popen(arg, "w");
        fflush(sheph_f);
#endif
}
