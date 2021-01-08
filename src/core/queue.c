#include <assert.h>
#include "core/queue.h"
#include "core/spinlock.h"


struct sge_queue {
	int r;
	int w;
	int cap;
	int used;
	sge_spinlock lock;
	void** data;
};

static void**
create_container(int size) {
	size_t s = sizeof(void**) * size;
	void** container = sge_malloc(s);
	memset(container, 0, s);
	return container;
}

static void
destroy_container(void* p) {
	sge_free(p);
}

static int
is_full(sge_queue* q) {
	return q->used == q->cap;
}

static int
is_empty(sge_queue* q) {
	return q->used == 0;
}

static void*
_dequeue(sge_queue* q) {
	if (is_empty(q)) {
		return NULL;
	}
	void *d = q->data[q->r];
	q->r = (q->r + 1) % q->cap;
	q->used--;
	return d;
}

static int
expand(sge_queue* q) {
	int i;
	int used = q->used;
	size_t cap = q->cap * 2;
	void** new = create_container(cap);

	for (i = 0; i < used; ++i) {
		new[i] = _dequeue(q);
	}
	q->r = 0;
	q->cap = cap;
	q->w = q->used = used;

	destroy_container(q->data);
	q->data = new;
	return q->used;
}

static int
_enqueue(sge_queue* q, void* data) {
	if (is_full(q)) {
		expand(q);
	}
	q->data[q->w] = data;
	q->w = (q->w + 1) % q->cap;
	q->used++;
	return q->used;
}


sge_queue*
create_queue(size_t size) {
	size_t s = sizeof(sge_queue);
	sge_queue* q = sge_malloc(s);
	memset(q, 0, s);
	SPIN_INIT(q);
	q->data = create_container(size);
	q->cap = size;
	return q;
}

void
destroy_queue(sge_queue* q) {
	SPIN_DESTROY(q);
	destroy_container((void*)q->data);
	sge_free(q);
}

int
enqueue(sge_queue* q, void* data) {
	assert(q);
	SPIN_LOCK(q);
	int size = _enqueue(q, data);
	SPIN_UNLOCK(q);
	return size;
}

int
dequeue(sge_queue* q, void** ud) {
	assert(q);
	SPIN_LOCK(q);
	*ud = _dequeue(q);
	SPIN_UNLOCK(q);
	return q->used;
}
