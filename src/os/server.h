#ifndef SERVER_H_
#define SERVER_H_

#include "core/config.h"

int start_server(sge_config* config);
int destroy_server();

int sendto_server(COMMAND_TYPE type, int id, void (*cb_free)(void*), void* data);

#endif
