#include "core/sge.h"

#include "os/socket.h"

sge_socket*
create_socket(int fd) {
	sge_socket* sock = sge_malloc(sizeof(*sock));
	memset(sock, 0, sizeof(sock));
	sock->fd = fd;
	sock->status = SOCKET_AVAILABLE;
	sock->w_buf = create_buffer(512);
	return sock;
}

void
destroy_socket(sge_socket* sock) {
	destroy_buffer(sock->w_buf);
	sge_free(sock);
}
