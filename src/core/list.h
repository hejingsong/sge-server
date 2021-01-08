#ifndef LIST_H
#define LIST_H


typedef struct sge_list sge_list;
typedef struct sge_list_iter sge_list_iter;


sge_list* list_create();
int list_add(sge_list* list, void* data);
int list_del(sge_list* list);
int list_remove(sge_list_iter* iter);
int list_destroy(sge_list* list);

sge_list_iter* list_iter_create(sge_list* list);
int list_iter_next(sge_list_iter* iter);
int list_iter_end(sge_list_iter* iter);
void* list_iter_data(sge_list_iter* iter);
void list_iter_destroy(sge_list_iter* iter);


#endif
