#ifndef ENV_H_
#define ENV_H_

int init_env();
int load_config(const char* file, sge_config* config);
int destroy_env();

#endif
