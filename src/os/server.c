#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <assert.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "core/sge.h"
#include "core/log.h"
#include "core/list.h"
#include "core/queue.h"
#include "os/server.h"
#include "os/event.h"

#define MAX_WORKER_NUM 128
#define DEFAULT_READ_SIZE 1024
#define CHECK_ARG(msg) \
if (msg->id < 0 || msg->id > MAX_SOCK_NUM) {		\
	ERROR("invalid fd: %d", msg->id);				\
	break;										\
}													\
s = SERVER.socks[msg->id];							\
if (!s) {											\
	ERROR("SERVER.socks[%d] is null", msg->id);		\
	break;										\
}


struct sge_server {
	sge_event* event;
	sge_socket* socks[MAX_SOCK_NUM];
	sge_queue* worker_queue;
	sge_queue* server_queue;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t tids[MAX_WORKER_NUM];
	uint32_t worker_num;
	uint32_t sock_num;
	uint8_t run;
};

static struct sge_server SERVER;
static sge_list* DELAY_CLOSE_SOCKS = NULL;

static sge_socket* create_conn(int fd);
static int init_server(sge_config* config);
static int on_accept(sge_socket* sock);
static int on_conn_readable(sge_socket* sock);
static int on_conn_writeable(sge_socket* sock);
static int on_read_done(sge_socket* sock);
static int set_non_block(sge_socket* sock);
static int add_socket(struct sge_server* server, sge_socket* sock);
static int write_socket_data(sge_socket* sock, sge_buffer* buf);
static int try_close_socket(sge_socket* sock);
static int close_socket(sge_socket* sock);
static void _destroy_socket(sge_socket* sock);
static void* worker(void* arg);
static int start_worker(sge_config* config);
static int sendto_worker(COMMAND_TYPE type, int id, void (*cb_free)(void*), void* data);
static int awake_worker();
static int wait_worker();
static int deal_request();
static int check_socket();


static int
change_user(const char* user) {
	struct passwd pwd;
	struct passwd *result;
	char* buf = NULL;
	size_t bufsize = 0;
	int ret = 0;

	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1) {
		bufsize = 16384;
	}

	buf = sge_malloc(bufsize);
	if (buf == NULL) {
		SYS_ERROR();
		return SGE_ERR;
	}

	ret = getpwnam_r(user, &pwd, buf, bufsize, &result);
	if (result == NULL) {
		if (ret == 0) {
			ERROR("not found user: %s", user);
		} else {
			errno = ret;
			SYS_ERROR();
		}
		goto FAILURE;
	}

	if (seteuid(pwd.pw_uid) < 0) {
		SYS_ERROR();
		goto FAILURE;
	}

	return SGE_OK;
FAILURE:
	sge_free(buf);
	return SGE_ERR;
}

static int
change_workdir(const char* path) {
	if (chdir(path)) {
		SYS_ERROR();
		return SGE_ERR;
	}
	return SGE_OK;
}

static int
daemonize(const char* logfile) {
	if (daemon(1, 1) < 0) {
		goto ERROR;
	}

	int fd = open(logfile, O_RDWR|O_CREAT|O_CLOEXEC|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd < 0) {
		goto ERROR;
	}
	if (dup2(fd, STDIN_FILENO) < 0) {
		goto ERROR;
	};
	if (dup2(fd, STDOUT_FILENO) < 0) {
		goto ERROR;
	}
	if (dup2(fd, STDERR_FILENO) < 0) {
		goto ERROR;
	}
	return SGE_OK;
ERROR:
	SYS_ERROR();
	return SGE_ERR;
}

static void
handle_signal(int signo) {
	switch (signo) {
		case SIGINT:
			SERVER.run = 0;
		break;
		default:
			WARNING("unknown signal %d", signo);
		break;
	}
}

static int
enable_signal(int signo) {
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    return sigaction(signo, &sa, NULL);
}

static int
init_signal() {
    enable_signal(SIGINT);
    return SGE_OK;
}

static int
write_socket(sge_socket* sock, const char* str, size_t len) {
	int ret;

	ret = write(sock->fd, str, len);
	if (ret < 0) {
		SYS_ERROR();
		if (errno != EINTR || errno != EAGAIN) {
			sendto_worker(CMD_CLOSE, sock->fd, NULL, NULL);
			clear_buffer(sock->w_buf);
			close_socket(sock);
			return SGE_ERR;
		}
		ret = 0;
	}
	return ret;
}

static int
init_unix_socket(const char* sock) {
	int fd, retcode;
	struct sockaddr_un addr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}

	unlink(sock);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sock);
	retcode = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (retcode < 0) {
		SYS_ERROR();
		retcode = SGE_ERR;
		goto RET;
	}

	retcode = listen(fd, 512);
	if (retcode < 0) {
		SYS_ERROR();
		retcode = SGE_ERR;
		goto RET;
	}

	return fd;
RET:
	close(fd);
	return retcode;
}

static int
init_port(const char* host, const char* port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	int retcode = SGE_OK;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	s = getaddrinfo(host, port, &hints, &result);
	if (s != 0) {
		SYS_ERROR();
		return SGE_ERR;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		SYS_ERROR();
		close(sfd);
	}

	if (NULL == rp) {
		retcode = SGE_ERR;
		goto RET;
	}

	retcode = listen(sfd, 512);
	if (retcode < 0) {
		close(sfd);
		SYS_ERROR();
		retcode = SGE_ERR;
		goto RET;
	}

	retcode = sfd;
RET:
	freeaddrinfo(result);
	return retcode;
}

static sge_socket*
init_listener(const char* sock) {
	int fd, ret;
	char *p = strstr((char*)sock, ":");
	if (p) {
		char host[128];
		char port[6];
		int host_len = p - sock;
		int port_len = strlen(sock) - host_len - 1;
		strncpy(host, sock, host_len);
		strncpy(port, p+1, port_len);
		host[host_len] = '\0';
		port[port_len] = '\0';
		fd = init_port(host, port);
	} else {
		fd = init_unix_socket(sock);
	}
	if (fd < 0) {
		return NULL;
	}

	sge_socket* listener = create_socket(fd);
	listener->on_read = on_accept;
	listener->on_write = NULL;

	return listener;
}

sge_socket*
create_conn(int fd) {
	sge_socket* conn;

	if (SERVER.socks[fd]) {
		conn = SERVER.socks[fd];
		conn->status = SOCKET_AVAILABLE;
	} else {
		conn = create_socket(fd);
	}
	return conn;
}

int
on_accept(sge_socket* sock) {
	int clt;
	struct sockaddr_in sockaddr;
	socklen_t size;

	clt = accept(sock->fd, (struct sockaddr*)&sockaddr, &size);
	if (clt < 0) {
		SYS_ERROR();
		return SGE_OK;
	}
	if (SERVER.sock_num >= MAX_SOCK_NUM) {
		WARNING("Too many connections. current connection num: %d", SERVER.sock_num);
		close(clt);
		return SGE_OK;
	}

	sge_socket* conn = create_conn(clt);
	set_non_block(conn);
	conn->on_read = on_conn_readable;
	conn->on_write = on_conn_writeable;
	add_socket(&SERVER, conn);
	if (SERVER.event->add(SERVER.event, conn, EVT_READ) == SGE_ERR) {
		_destroy_socket(conn);
		return SGE_ERR;
	}
	sendto_worker(CMD_NEW_CONN, clt, NULL, (void*)&(conn->fd));
	return SGE_OK;
}

int
on_conn_readable(sge_socket* sock) {
	int nread;
	char buf[DEFAULT_READ_SIZE];

	nread = read(sock->fd, buf, DEFAULT_READ_SIZE);
	if (nread < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}
	if (nread == 0 && sock->status == SOCKET_AVAILABLE) {
		on_read_done(sock);
		return SGE_OK;
	}
	sge_buffer* b = create_buffer_ex(buf, nread);
	sendto_worker(CMD_MESSAGE, sock->fd, destroy_buffer, (void*)b);
	return SGE_OK;
}

int
on_conn_writeable(sge_socket* sock) {
	int nwrite;
	size_t len;

	const char* str = buffer_data(sock->w_buf, &len);
	nwrite = write_socket(sock, str, len);
	if (nwrite == SGE_ERR) {
		return SGE_ERR;
	}
	erase_buffer(sock->w_buf, 0, nwrite);
	if (len == nwrite) {
		SERVER.event->remove(SERVER.event, sock, EVT_WRITE);
	}
	return SGE_OK;
}

int
on_read_done(sge_socket* sock) {
	SERVER.event->remove(SERVER.event, sock, EVT_READ);
	shutdown(sock->fd, SHUT_RD);
	sock->status = SOCKET_HALFCLOSE;
	return sendto_worker(CMD_READDONE, sock->fd, NULL, NULL);
}

int
set_non_block(sge_socket* sock) {
	int flags = 0;
	flags = fcntl(sock->fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sock->fd, F_SETFL, flags);
	return SGE_OK;
}

int
add_socket(struct sge_server* server, sge_socket* sock) {
	server->sock_num++;
	server->socks[sock->fd] = sock;
	return SGE_OK;
}

int
write_socket_data(sge_socket* sock, sge_buffer* buf) {
	int nwrite;
	size_t len;
	const char* str = buffer_data(buf, &len);

	nwrite = write_socket(sock, str, len);
	if (nwrite == SGE_ERR) {
		return SGE_ERR;
	}

	sock->w_buf = append_buffer(sock->w_buf, str + nwrite, len - nwrite);
	SERVER.event->add(SERVER.event, sock, EVT_WRITE);
	return SGE_OK;
}

int
try_close_socket(sge_socket* sock) {
	if (empty_buffer(sock->w_buf)) {
		_destroy_socket(sock);
		return SGE_OK;
	}
	return SGE_ERR;
}

int
close_socket(sge_socket* sock) {
	if (try_close_socket(sock) == SGE_OK) {
		return SGE_OK;
	}
	list_add(DELAY_CLOSE_SOCKS, (void*)sock);
	return SGE_OK;
}

void
_destroy_socket(sge_socket* sock) {
	if (sock->status == SOCKET_CLOSED) {
		return;
	}
	SERVER.sock_num--;
	if (sock->events) {
		SERVER.event->remove(SERVER.event, sock, sock->events);
	}
	sock->status = SOCKET_CLOSED;
	sock->on_write = sock->on_read = NULL;
	close(sock->fd);
}

void*
worker(void* arg) {
	sge_message* msg;
	cb_worker cb = arg;
	while(SERVER.run) {
		dequeue(SERVER.worker_queue, (void**)&msg);
		if (msg == NULL) {
			pthread_mutex_lock(&(SERVER.mutex));
			pthread_cond_wait(&(SERVER.cond), &(SERVER.mutex));
			pthread_mutex_unlock(&(SERVER.mutex));
			continue;
		}
		cb(msg);
		if (msg->free) {
			msg->free(msg->ud);
		}
		sge_free(msg);
	}
	return NULL;
}

int
start_worker(sge_config* config) {
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, worker, config->cb);
	if (ret < 0) {
		SYS_ERROR();
		return SGE_ERR;
	}
	SERVER.tids[SERVER.worker_num++] = tid;
	return SGE_OK;
}

int
sendto_worker(COMMAND_TYPE type, int id, void (*cb_free)(void*), void* data) {
	sge_message* msg = sge_malloc(sizeof(*msg));
	msg->id = id;
	msg->free = cb_free;
	msg->type = type;
	msg->ud = data;
	int size = enqueue(SERVER.worker_queue, (void*)msg);
	if (size == 1) {
		awake_worker();
	}
	return SGE_OK;
}

int
awake_worker() {
	pthread_mutex_lock(&(SERVER.mutex));
	pthread_cond_broadcast(&(SERVER.cond));
	pthread_mutex_unlock(&(SERVER.mutex));
	return SGE_OK;
}

int
wait_worker() {
	void *result;
	int i = 0;

	awake_worker();

	for (; i < SERVER.worker_num; ++i) {
		pthread_join(SERVER.tids[i], &result);
		INFO("worker[%d] exit.", SERVER.tids[i]);
	}
	INFO("all worker exit.");
	return SGE_OK;
}

int
deal_request() {
	sge_socket* s;
	sge_message* msg;

	dequeue(SERVER.server_queue, (void**)&msg);
	if (msg == NULL) {
		return SGE_OK;
	}

	while(msg) {
		switch (msg->type) {
			case CMD_MESSAGE:
				CHECK_ARG(msg);
				write_socket_data(s, (sge_buffer*)msg->ud);
			break;
			case CMD_CLOSE:
				CHECK_ARG(msg);
				close_socket(s);
			break;
			default:
				WARNING("unknown message type: %d", msg->type);
			break;
		}
		if (msg->free) {
			msg->free(msg->ud);
		}
		sge_free(msg);
		dequeue(SERVER.server_queue, (void**)&msg);
	}
	return SGE_OK;
}

int
check_socket() {
	sge_socket* sock;
	sge_list_iter* iter = list_iter_create(DELAY_CLOSE_SOCKS);

	for (; !list_iter_end(iter); list_iter_next(iter)) {
		sock = list_iter_data(iter);
		if (try_close_socket(sock) == SGE_OK) {
			list_remove(iter);
		}
	}

	list_iter_destroy(iter);
	list_del(DELAY_CLOSE_SOCKS);
	return SGE_OK;
}

int
init_server(sge_config* config) {
	if (config->user && SGE_ERR == change_user(config->user)) {
		return SGE_ERR;
	}

	if (change_workdir(config->workdir) == SGE_ERR) {
		return SGE_ERR;
	}

	if (config->daemon && daemonize(config->logfile) == SGE_ERR) {
		return SGE_ERR;
	}

	if (init_signal() == SGE_ERR) {
		return SGE_ERR;
	}

	DELAY_CLOSE_SOCKS = list_create();
	assert(DELAY_CLOSE_SOCKS);

	SERVER.sock_num = 0;
	memset(SERVER.socks, 0, sizeof(SERVER.socks));
	SERVER.event = create_event();
	if (SERVER.event->init(SERVER.event) == SGE_ERR) {
		return SGE_ERR;
	}

	sge_socket* listener = init_listener(config->socket);
	if (NULL == listener) {
		return SGE_ERR;
	}
	set_non_block(listener);
	if (SERVER.event->add(SERVER.event, listener, EVT_READ) == SGE_ERR) {
		return SGE_ERR;
	}
	add_socket(&SERVER, listener);
	SERVER.worker_queue = create_queue(8);
	SERVER.server_queue = create_queue(8);
	pthread_mutex_init(&(SERVER.mutex), NULL);
	pthread_cond_init(&(SERVER.cond), NULL);
	return SGE_OK;
}

// export
int
start_server(sge_config* config) {
	assert(SERVER.run == 0);
	int i = 0, active_num = 0;
	sge_socket* socks[MAX_SOCK_NUM];
	sge_socket* s;

	if (init_server(config) == SGE_ERR) {
		return SGE_ERR;
	}

	if (start_worker(config) == SGE_ERR) {
		return SGE_ERR;
	}

	SERVER.run = 1;
	while(SERVER.run) {
		deal_request();
		active_num = SERVER.event->poll(SERVER.event, socks);
		for (i = 0; i < active_num; ++i) {
			s = socks[i];
			if (s->options & EVT_READ) {
				s->on_read(s);
			}
			if (s->options & EVT_WRITE) {
				s->on_write(s);
			}
		}
		check_socket();
	}

	wait_worker();
	INFO("server gone away.");
	return SGE_OK;
}

int
destroy_server() {
	int i = 0;
	sge_socket* s;

	destroy_queue(SERVER.worker_queue);
	destroy_queue(SERVER.server_queue);
	list_destroy(DELAY_CLOSE_SOCKS);
	for (i = 0; i < MAX_SOCK_NUM; ++i) {
		s = SERVER.socks[i];
		if (!s) {
			continue;
		}
		_destroy_socket(s);
	}
	SERVER.event->destroy(SERVER.event);
	pthread_mutex_destroy(&(SERVER.mutex));
	pthread_cond_destroy(&(SERVER.cond));
	return SGE_OK;
}

int
sendto_server(COMMAND_TYPE type, int id, void (*cb_free)(void*), void* data) {
	sge_message* msg = sge_malloc(sizeof(*msg));
	msg->id = id;
	msg->free = cb_free;
	msg->type = type;
	msg->ud = data;
	enqueue(SERVER.server_queue, (void*)msg);
	return SGE_OK;
}
