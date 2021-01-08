#include "core/sge.h"
#include "core/buffer.h"

struct sge_buffer {
	size_t cap;
	size_t used;
	char data[0];
};

sge_buffer*
create_buffer(size_t size) {
	size_t s = sizeof(char) * size + sizeof(sge_buffer);
	sge_buffer* b = sge_malloc(s);
	memset(b, 0, s);
	b->cap = size;
	return b;
}

sge_buffer*
create_buffer_ex(const char* str, size_t len) {
	sge_buffer* buf = create_buffer(len);
	return append_buffer(buf, str, len);
}

sge_buffer*
append_buffer(sge_buffer* buf, const char* str, size_t len) {
	size_t remain = buf->cap - buf->used;
	if (remain >= len) {
		memcpy(buf->data + buf->used, str, len);
		buf->used += len;
	} else {
		size_t new_cap = buf->cap + len;
		sge_buffer* new = create_buffer(new_cap);
		memcpy(new->data, buf->data, buf->used);
		memcpy(new->data + buf->used, str, len);
		new->used = buf->used + len;
		destroy_buffer(buf);
		buf = new;
	}
	return buf;
}

size_t
erase_buffer(sge_buffer* buf, size_t start, size_t len) {
	int remain = buf->used - start - len;
	memcpy(buf->data + start, buf->data + start + len, remain);
	buf->used -= len;
	return buf->used;
}

void
destroy_buffer(void* buf) {
	sge_free(buf);
}

const char*
buffer_data(sge_buffer* buf, size_t* len) {
	*len = buf->used;
	return buf->data;
}

int
empty_buffer(sge_buffer* buf) {
	return buf->used == 0;
}

int
clear_buffer(sge_buffer* buf) {
	buf->used = 0;
	return SGE_OK;
}
