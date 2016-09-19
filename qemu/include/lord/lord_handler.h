union semun {
	int val;               /* used for SETVAL only */
	struct semid_ds *buf;  /* used for IPC_STAT and IPC_SET */
	ushort *array;         /* used for GETALL and SETALL */
};

int strh_to_i(const char *arg);
void handle_arg_shdump(const char* arg);
void handle_arg_sh(const char* arg);
