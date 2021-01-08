#ifndef SOCKET_H_
#define SOCKET_H_

#include <stdint.h>
#include "core/buffer.h"

typedef enum EVENT_TYPE {
	EVT_READ = 0X01,
	EVT_WRITE = 0X02,
	EVT_ERROR = 0X04
} EVENT_TYPE;

typedef enum {
	SOCKET_AVAILABLE,
	SOCKET_HALFCLOSE,
	SOCKET_CLOSED
} SOCKET_STATUS;

typedef struct sge_socket sge_socket;

typedef int (*cb_on_read)(sge_socket* sock);
typedef int (*cb_on_write)(sge_socket* sock);

struct sge_socket {
	int fd;
	uint32_t events;
	uint32_t options;
	cb_on_read on_read;
	cb_on_write on_write;
	int status;
	sge_buffer* w_buf;
};

sge_socket* create_socket(int fd);
void destroy_socket(sge_socket* sock);

#endif
