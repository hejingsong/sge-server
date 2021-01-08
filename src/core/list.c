#include "core/sge.h"
#include "core/list.h"

typedef struct sge_list_node {
	struct sge_list_node* next;
	struct sge_list_node* prev;
	void* data;
} sge_list_node;


struct sge_list {
	sge_list_node head;
	sge_list_node deleted;
	size_t size;
};

struct sge_list_iter {
	sge_list_node* cur;
	sge_list_node* start;
	sge_list_node* end;
	sge_list* list;
};


static int
_list_add(sge_list_node* head, sge_list_node* node) {
	node->next = head;
	node->prev = head->prev;
	head->prev->next = node;
	head->prev = node;
	return SGE_OK;
}

static int
_list_del(sge_list_node* node) {
	if (!node) {
		return SGE_OK;
	}

	node->prev->next = node->next;
	node->next->prev = node->prev;
	sge_free(node);
	return SGE_OK;
}


sge_list*
list_create() {
	size_t s = sizeof(sge_list);
	sge_list* list = sge_malloc(s);
	memset(list, 0, s);
	list->head.prev = list->head.next = &list->head;
	list->deleted.prev = list->deleted.next = &list->deleted;
	return list;
}

int
list_add(sge_list* list, void* data) {
	sge_list_node* node = sge_malloc(sizeof(*node));
	node->data = data;
	_list_add(&list->head, node);

	list->size++;
	return SGE_OK;
}

int
list_remove(sge_list_iter* iter) {
	sge_list_node* node = sge_malloc(sizeof(*node));
	node->data = iter->cur;
	_list_add(&(iter->list->deleted), node);
	return SGE_OK;
}

int
list_del(sge_list* list) {
	sge_list_node* node = list->deleted.next;
	sge_list_node* end = &list->deleted;
	sge_list_node* next;

	while(node != end) {
		next = node->next;
		_list_del(node->data);
		_list_del(node);
		node = next;
	}

	return SGE_OK;
}

int
list_destroy(sge_list* list) {
	sge_list_node* p, *next, *end;

	end = &list->head;
	p = list->head.next;
	while(p != end) {
		next = p->next;
		sge_free(p);
		p = next;
	}

RET:
	sge_free(list);
	return SGE_OK;
}

sge_list_iter*
list_iter_create(sge_list* list) {
	sge_list_iter* iter = sge_malloc(sizeof(*iter));
	iter->start = iter->cur = list->head.next;
	iter->end = &list->head;
	iter->list = list;
	return iter;
}

int
list_iter_next(sge_list_iter* iter) {
	iter->cur = iter->cur->next;
	return SGE_OK;
}

int
list_iter_end(sge_list_iter* iter) {
	return iter->cur == iter->end;
}

void*
list_iter_data(sge_list_iter* iter) {
	return iter->cur->data;
}

void
list_iter_destroy(sge_list_iter* iter) {
	sge_free(iter);
}
