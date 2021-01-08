#include <assert.h>
#include <sys/epoll.h>

#include "core/sge.h"
#include "core/log.h"
#include "os/event.h"

#define	MAX_EPOLL_SIZE 1024

static uint32_t
calc_event(EVENT_TYPE types) {
	uint32_t ev = 0;

	if (types & EVT_READ) {
		ev |= (EPOLLIN | EPOLLHUP);
	}
	if (types & EVT_WRITE) {
		ev |= EPOLLOUT;
	}
	if (types & EVT_ERROR) {
		ev |= EPOLLERR;
	}
	return ev;
}

static int
init_event(sge_event* evt) {
	assert(evt->efd == 0);
	int efd = epoll_create(MAX_EPOLL_SIZE);
	if (efd < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}
	evt->efd = efd;
	return SGE_OK;
}

static int
add_event(sge_event* evt, sge_socket* sock, EVENT_TYPE types) {
	assert(evt->efd);
	int op = 0, ret;
	struct epoll_event event;

	if (sock->events == 0) {
		op = EPOLL_CTL_ADD;
	} else {
		op = EPOLL_CTL_MOD;
	}
	types |= sock->events;
	event.events = calc_event(types);
	event.data.ptr = (void*)sock;

	ret = epoll_ctl(evt->efd, op, sock->fd, &event);
	if (ret < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}
	sock->events |= types;
	return SGE_OK;
}

static int
remove_event(sge_event* evt, sge_socket* sock, EVENT_TYPE types) {
	assert(evt->efd);
	int op = 0, ret;
	uint32_t result;
	struct epoll_event event;

	result = (~types) & sock->events;
	if (result == 0) {
		op = EPOLL_CTL_DEL;
	} else {
		op = EPOLL_CTL_MOD;
	}
	event.events = calc_event(result);
	event.data.ptr = (void*)sock;

	ret = epoll_ctl(evt->efd, op, sock->fd, &event);
	if (ret < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}
	sock->events = result;
	return SGE_OK;
}

static int
poll_event(sge_event* evt, sge_socket** socks) {
	int i, num;
	struct epoll_event events[MAX_EPOLL_SIZE];
	struct epoll_event* ev;
	sge_socket* sock;

	num = epoll_wait(evt->efd, events, MAX_EPOLL_SIZE, 100);
	for (i = 0; i < num; ++i) {
		ev = &events[i];
		sock = (sge_socket*)ev->data.ptr;
		sock->options = 0;
		if ((ev->events & EPOLLIN) || (ev->events & EPOLLHUP)) {
			sock->options |= EVT_READ;
		}
		if (ev->events & EPOLLOUT) {
			sock->options |= EVT_WRITE;
		}
		if (ev->events & EPOLLERR) {
			sock->options |= EVT_ERROR;
		}
		socks[i] = sock;
	}

	return num;
}

static int
destroy_event(sge_event* evt) {
	sge_free(evt);
	return SGE_OK;
}

sge_event*
create_event() {
	sge_event* evt = sge_malloc(sizeof(*evt));
	evt->init = init_event;
	evt->add = add_event;
	evt->remove = remove_event;
	evt->poll = poll_event;
	evt->destroy = destroy_event;
	evt->efd = 0;
	return evt;
}
