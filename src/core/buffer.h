#ifndef BUFFER_H_
#define BUFFER_H_

typedef struct sge_buffer sge_buffer;

sge_buffer* create_buffer(size_t size);
sge_buffer* create_buffer_ex(const char* str, size_t len);
sge_buffer* append_buffer(sge_buffer* buf, const char* str, size_t len);
size_t erase_buffer(sge_buffer* buf, size_t start, size_t len);
void destroy_buffer(void* buf);
const char* buffer_data(sge_buffer* buf, size_t* len);
int clear_buffer(sge_buffer* buf);
int empty_buffer(sge_buffer* buf);

#endif
