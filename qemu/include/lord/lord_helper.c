#include <stdio.h>
#include <sys/sem.h>
#include "cpu.h"
#include "helper.h"
#include "lord_helper.h"

bool onFlag = false, ret_dynlib = false;
int call_counter = 0, i = 0;

// Compilation flags for different shadow stack/program shepherding versions:
//
// SHEPH_NOPT_WRITE:
// Uses pipe for ipc, has no local buffer - 1 call/ret generates 1 ipc msg
//
// SHEPH_BUFFER:
// Uses pipe for ipc, optimized with a local buffer to store consecutive calls
// Genrates ipc msgs with full buffer content on rets
//
// SHEPH_STACK_WRITE:
// Same as SHEPH_BUFFER + WRITE debugging messages
//
// SHEPH_STACK:
// Same as SHEPH_BUFFER
// Optimized with a local stack to assert consecutive calls/rets
//
// SHEPH_SHARED:
// Uses shared memory for ipc
//
// SHEPH_LEAF:
// No program shepherding / shadow stack here.
// Just an experiment for counting leaf functions.
//
// For more information, see the paper:
// http://ieeexplore.ieee.org/document/6970646/?reload=true&arnumber=6970646

#ifdef SHEPH_SHARED
int volatile *tb_counter = NULL;
trace_entry volatile *sheph_sh = NULL;
struct sembuf lock;
int semid;

#define MAX_SHARED 16384

#else
int tb_counter = 0;
#endif

unsigned long long int ncalls = 0, nrets = 0;
FILE* sheph_f = NULL;

trace_entry trace_buffer[10000];
trace_entry trace_op;

#ifdef SHEPH_STACK_WRITE
// Uses a local stack to solve consecutive calls/rets - decreases output
// Also prints trace info on screen for debugging

bool leaf_flag = true;

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 1;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
	leaf_flag = true;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 2;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
	leaf_flag = true;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
		tb_counter--;
		if(trace_buffer[tb_counter].arg != t0){
			printf("#WARNING: Corrupted return address\n\
                                #WARNING: -Expected: %lx\n\
                                #WARNING: -Destination: %lx\n",
                               trace_buffer[tb_counter].arg, t0);
		}
		leaf_flag = false;
		return;
	}

	trace_buffer[tb_counter].op = 3;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);
	printf("#WRITE%d\n", tb_counter);
	fflush(stdout);

	tb_counter = 0;
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
		tb_counter--;
		if(trace_buffer[tb_counter].arg != t0){
			printf("#WARNING: Corrupted return address\n\
                                #WARNING: -Expected: %lx\n\
                                #WARNING: -Destination: %lx\n",
                               trace_buffer[tb_counter].arg, t0);
		}
		leaf_flag = false;
		return;
	}

	trace_buffer[tb_counter].op = 4;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);
	printf("#WRITE%d\n", tb_counter);
	fflush(stdout);

	tb_counter = 0;
}

#elif defined(SHEPH_STACK)

bool leaf_flag = true;

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 1;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
	leaf_flag = true;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 2;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
	leaf_flag = true;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
		tb_counter--;
		if(trace_buffer[tb_counter].arg != t0){
			printf("#WARNING: Corrupted return address\n\
                                #WARNING: -Expected: %lx\n\
                                #WARNING: -Destination: %lx\n",
                               trace_buffer[tb_counter].arg, t0);
		}
		leaf_flag = false;
		return;
	}

	trace_buffer[tb_counter].op = 3;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
		tb_counter--;
		if(trace_buffer[tb_counter].arg != t0){
			printf("#WARNING: Corrupted return address\n\
                                #WARNING: -Expected: %lx\n\
                                #WARNING: -Destination: %lx\n",
                              trace_buffer[tb_counter].arg, t0);
		}
		leaf_flag = false;
		return;
	}

	trace_buffer[tb_counter].op = 4;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;
}

#elif defined(SHEPH_SHARED)

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	lock.sem_op = -1;
	semop(semid, &lock, 1);
	sheph_sh[*tb_counter].op = 1;
	sheph_sh[*tb_counter].arg = t1;
	*tb_counter = *tb_counter + 1;
	if(*tb_counter > MAX_SHARED){
		perror("buffer pointer reached maximum number of operations\n");
		exit(1);
	}
	lock.sem_op = 1;
	semop(semid, &lock, 1);
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	lock.sem_op = -1;
	semop(semid, &lock, 1);
	sheph_sh[*tb_counter].op = 2;
	sheph_sh[*tb_counter].arg = t1;
	*tb_counter = *tb_counter + 1;
	if(*tb_counter > MAX_SHARED){
		perror("buffer pointer reached maximum number of operations\n");
		exit(1);
	}
	lock.sem_op = 1;
	semop(semid, &lock, 1);
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	lock.sem_op = -1;
	semop(semid, &lock, 1);
	sheph_sh[*tb_counter].op = 3;
	sheph_sh[*tb_counter].arg = t0;
	*tb_counter = *tb_counter + 1;
	if(*tb_counter > MAX_SHARED){
		perror("buffer pointer reached maximum number of operations\n");
		exit(1);
	}
	lock.sem_op = 1;
	semop(semid, &lock, 1);
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	lock.sem_op = -1;
	semop(semid, &lock, 1);
	sheph_sh[*tb_counter].op = 4;
	sheph_sh[*tb_counter].arg = t0;
	*tb_counter = *tb_counter + 1;
	if(*tb_counter > MAX_SHARED){
		perror("buffer pointer reached maximum number of operations\n");
                exit(1);
	}
	lock.sem_op = 1;
	semop(semid, &lock, 1);
}

#elif defined(SHEPH_LEAF)

bool leaf_flag = true;

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	leaf_flag = true;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	leaf_flag = true;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
			printf("#LEAF\n");
			fflush(stdout);
			leaf_flag = false;
			return;
	}
	printf("#NONLEAF\n");
	fflush(stdout);
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	if(leaf_flag){
			printf("#LEAF\n");
			fflush(stdout);
			leaf_flag = false;
			return;
	}
	printf("#NONLEAF\n");
	fflush(stdout);
}

#elif defined(SHEPH_BUFFER_WRITE)

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 1;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 2;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 3;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);
	printf("#WRITE%d\n", tb_counter);
	fflush(stdout);

	tb_counter = 0;
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 4;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);
	printf("#WRITE%d\n", tb_counter);
	fflush(stdout);

	tb_counter = 0;
}

#elif defined(SHEPH_BUFFER)

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 1;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	trace_buffer[tb_counter].op = 2;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 3;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 4;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;
}



#elif defined(SHEPH_BUFFER_LEAF)

bool leaf_flag = false;

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	leaf_flag = true;
	trace_buffer[tb_counter].op = 1;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
	leaf_flag = true;
	trace_buffer[tb_counter].op = 2;
	trace_buffer[tb_counter].arg = t1;
	tb_counter++;
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 3;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;

	if(leaf_flag){
	      	printf("#LEAF\n");
      		fflush(stdout);
                leaf_flag = false;
                return;
	}
	printf("#NONLEAF\n");
	fflush(stdout);
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
	trace_buffer[tb_counter].op = 4;
	trace_buffer[tb_counter].arg = t0;
	tb_counter++;

	fwrite(trace_buffer, sizeof(trace_entry), tb_counter, sheph_f);
	fflush(sheph_f);

	tb_counter = 0;

	if(leaf_flag){
	      	printf("#LEAF\n");
      		fflush(stdout);
                leaf_flag = false;
                return;
	}
	printf("#NONLEAF\n");
	fflush(stdout);
}

#elif defined(SHEPH_NOPT_WRITE)

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
		trace_op.op = 1;
		trace_op.arg = t1;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
		printf("#WRITE1\n");
		fflush(stdout);
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
		trace_op.op = 2;
		trace_op.arg = t1;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
		printf("#WRITE1\n");
		fflush(stdout);
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
		trace_op.op = 3;
		trace_op.arg = t0;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
		printf("#WRITE1\n");
		fflush(stdout);
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
		trace_op.op = 4;
		trace_op.arg = t0;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
		printf("#WRITE1\n");
		fflush(stdout);
}



#else

void HELPER(trace_call_ev)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
		trace_op.op = 1;
		trace_op.arg = t1;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
}

void HELPER(trace_call_im)(target_ulong _eip, target_ulong t0, target_ulong t1)
{
		trace_op.op = 2;
		trace_op.arg = t1;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
}

void HELPER(trace_ret_im)(target_ulong _eip, target_ulong t0)
{
		trace_op.op = 3;
		trace_op.arg = t0;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
}

void HELPER(trace_ret)(target_ulong _eip, target_ulong t0)
{
		trace_op.op = 4;
		trace_op.arg = t0;
		fwrite(&trace_op, sizeof(trace_entry), 1, sheph_f);
		fflush(sheph_f);
}

#endif
