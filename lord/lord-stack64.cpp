#include <iostream>
#include <sstream>
#include <stack>
#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
using namespace std;

#define OPC_VALIDO (opcode == 0 || opcode == 1 || opcode == 2 || opcode == 3 || opcode == 4 || opcode == 5 || opcode == 6)
#define MAX_OPCODE 7

/// Defs
typedef unsigned long int uInt64;


/// Prototypes
bool handleMessage();
bool handleCallImm();
bool handleCallEv();
bool handleRetImm();
bool handleRet();

uInt64 opcode;

/// Global vars
//map<uInt64, bool> monitorTable;
stack<uInt64> monitorTable;
uInt64 histogram[MAX_OPCODE];

typedef struct{
	uInt64 op;
	uInt64 exp_ret;
} trace_entry;

typedef struct{
	uInt64 op;
	uInt64 target;
} ret_entry;

ret_entry ret_buf;
trace_entry call_buf;
int warnings = 0;

/// Main entry point
int main(int argc, char *argv[]) {
  int lidos;

	printf("Lord initializing...\n");

  while ( true ) {

	lidos = fread(&opcode, sizeof(opcode), 1, stdin);

	if (lidos == 0) {
		break;
	}
  switch (opcode) {
  /// Message
	case 0:
  	//printf("message\n");
		histogram[0]++;
	  handleMessage();
  	break;

  /// Call immediate
	case 1:
		//printf("opcode 1\n");
		histogram[1]++;
	  handleCallImm();
  	break;

	case 5:
  	//printf("opcode 5\n");
		histogram[5]++;
    handleCallImm();
    break;

  /// Call Ev
  case 2:
		//printf("opcode 2\n");
		histogram[2]++;
    handleCallEv();
    break;

	case 6:
  	//printf("opcode 6\n");
		histogram[6]++;
    handleCallEv();
    break;

  /// Ret immediate
  case 3:
    //printf("ret3\n");
		histogram[3]++;
    handleRetImm();
    break;

  /// Ret
  case 4:
    //printf("ret4\n");
		histogram[4]++;
    handleRet();
    break;

	default:
  	printf("Warning: Unknown opcode %c\n", opcode);
    break;
  	}
	}

	printf("HISTOGRAM\n-\n");

	for (int i=0; i<MAX_OPCODE; i++) {
		printf("%d, %lu\n", i, histogram[i]);
	}
  printf("-\nTotals:\n\n");
	printf("Rets: %ld\n", histogram[3] + histogram[4]);
	printf("Calls: %ld\n", histogram[1] + histogram[2]);
	printf("Warnings: %d\n", warnings);
    return 0;
}


bool handleMessage() {
  char message[100];

	/// Read the ','
  fgets(message, 100, stdin);

	if (message[strlen(message)-1] != '\n') {
		printf("didn't read the entire message\n");
		//scanf("%*[^\n]\n");
	}
	else {
		message[strlen(message)-1] = '\0';
	}

  printf("Lord: %s\n", message);

  return true;
}

bool handleCallImm() {
  fread(&call_buf.exp_ret,sizeof(uInt64),1,stdin);
	monitorTable.push(call_buf.exp_ret);
	//printf("call %x\n", call_buf.exp_ret);
  return true;
}

bool handleCallEv() {
	fread(&call_buf.exp_ret,sizeof(uInt64),1,stdin);
  monitorTable.push(call_buf.exp_ret);
	//printf("call %x\n", call_buf.exp_ret);
  return true;
}

bool handleRetImm() {
	fread(&ret_buf.target,sizeof(uInt64),1,stdin);
	//printf("ret %x\n", ret_buf.target);

	if(monitorTable.empty()){
		printf("Warning: RET returned to unexpected address 0x%lx\n", ret_buf.target);
		warnings++;
		return false;
	}

	uInt64 top = monitorTable.top();
	if(top != ret_buf.target) {
		printf("Warning: RET returned to unexpected address 0x%lx\n", ret_buf.target);
		warnings++;
		return false;
	} else {
		monitorTable.pop();
		return true;
	}
}

bool handleRet() {
	fread(&ret_buf.target,sizeof(uInt64),1,stdin);
	//printf("ret %x\n", ret_buf.target);
	if(monitorTable.empty()){
		printf("Warning: RET returned to unexpected address 0x%lx\n", ret_buf.target);
		warnings++;
		return false;
	}
	
	uInt64 top = monitorTable.top();
	if(top != ret_buf.target) {
		printf("Warning: RET returned to unexpected address 0x%lx\n", ret_buf.target);
		warnings++;
		return false;
	} else {
		monitorTable.pop();
		return true;
	}
}
