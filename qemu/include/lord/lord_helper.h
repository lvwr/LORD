#include "cpu.h"

typedef struct {
	ushort sem_num;
	short sem_op;
	short sem_flg;
} sembuf;

typedef struct
{
	int op;
	target_ulong arg;
} trace_entry;
