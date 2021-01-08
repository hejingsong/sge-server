#ifndef SGE_H_
#define SGE_H_

#include <stdlib.h>
#include <string.h>

#define SGE_OK  0
#define SGE_ERR -1

#define sge_malloc malloc
#define sge_free free

#define MAX_SOCK_NUM 1024

typedef enum {
    CMD_QUIT,
    CMD_NEW_CONN,
    CMD_MESSAGE,
    CMD_READDONE,
    CMD_CLOSE
} COMMAND_TYPE;

typedef struct {
    COMMAND_TYPE type;
    int id;
    void (*free)(void*);
    void* ud;
} sge_message;

typedef int (*cb_worker)(sge_message*);

#endif
