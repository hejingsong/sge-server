#ifndef EVENT_H_
#define EVENT_H_

#include "os/socket.h"

typedef struct sge_event sge_event;

typedef int (*cb_init)(sge_event*);
typedef int (*cb_add)(sge_event*, sge_socket*, EVENT_TYPE);
typedef int (*cb_remove)(sge_event*, sge_socket*, EVENT_TYPE);
typedef int (*cb_poll)(sge_event*, sge_socket**);
typedef int (*cb_destroy)(sge_event*);

typedef struct sge_event {
	int efd;
	cb_init init;
	cb_add add;
	cb_remove remove;
	cb_poll poll;
	cb_destroy destroy;
} sge_event;

sge_event* create_event();

#endif