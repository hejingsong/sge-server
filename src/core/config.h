#ifndef CONFIG_H_
#define CONFIG_H_

typedef struct {
	const char* workdir;
	const char* logfile;
	const char* entry_file;
	const char* entry_func;
	const char* socket;
	const char* user;
	cb_worker cb;
	int daemon;
} sge_config;

#endif
