#include <Python.h>

#include "core/sge.h"
#include "core/log.h"
#include "core/config.h"
#include "core/buffer.h"
#include "os/server.h"

#include "python-src/common.h"
#include "python-src/env.h"

#define MAX_FILE_SIZE 10240

#define PARSE_STRING(DICT, NAME, OBJ, IGNORE)									\
do {																			\
	PyObject* value = PyDict_GetItemString((DICT), (#NAME));					\
	if (NULL == value) {														\
		if (IGNORE)																\
			break;																\
		fprintf(stderr, "can't found config.%s variable.\n", (#NAME));			\
		return -1;																\
	}																			\
																				\
	Py_ssize_t size = 0;														\
	const char* s = PyUnicode_AsUTF8AndSize(value, &size);						\
	char* tmp = (char*)malloc(sizeof(char) * (size + 1));						\
	strncpy(tmp, s, size);														\
	tmp[size] = '\0';															\
	(OBJ)->NAME = tmp;															\
	Py_DECREF(value);															\
} while(0)

#define PY_FUNCTION_ENTRY()														\
PyObject* py_result;															\
int py_result_code;


#define CALL_PY_FUNCTION(func, format, ...)										\
py_result = PyObject_CallFunction(func, format, ##__VA_ARGS__);					\
if (!py_result || py_result == Py_None || py_result == Py_False) {				\
	CHECK_SCRIPT_ERROR();														\
	py_result_code = SGE_ERR;													\
} else {																		\
	py_result_code = SGE_OK;													\
}



static int new_conn(sge_message* msg);
static int on_message(sge_message* msg);
static int on_read_done(sge_message* msg);
static int output_error(int id);
static PyObject* call_cb(PyObject* conn);
static PyObject* py_close_conn(PyObject* conn, PyObject* args);
static PyObject* py_send_conn(PyObject* conn, PyObject* msg);
static int close_conn(int id);
static int conn_id(PyObject* conn);


static PyObject* CALLBACK_FUNC = NULL;
static PyObject* CONNECTIONS[MAX_SOCK_NUM];
static PyObject* CLS_CONNECTION = NULL;
static const cb_worker MESSAGE_CBS[] = {
	NULL,
	new_conn,
	on_message,
	on_read_done,
	NULL
};


int
output_error(int id) {
	static const char* err_50x = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 146\r\n\r\n<html><head><title>502 Bad Gateway</title></head><body><center><h1>502 Bad Gateway</h1></center><hr><center>SgeServer 0.0.1</center></body></html>";
	static const size_t err_50x_len = 240;
	sge_buffer* buf = create_buffer_ex(err_50x, err_50x_len);
	sendto_server(CMD_MESSAGE, id, destroy_buffer, buf);
	close_conn(id);
	return SGE_OK;
}

PyObject*
create_conn(int id) {
	PyObject* module = NULL;
	PyObject* conn = NULL;
	PY_FUNCTION_ENTRY();

	if (!CLS_CONNECTION) {
		module = PyImport_ImportModule("sgeWeb.Connection");
		if (NULL == module) {
			CHECK_SCRIPT_ERROR();
			return NULL;
		}

		PyObject* cls = PyObject_GetAttrString(module, "Connection");
		if (NULL == cls) {
			CHECK_SCRIPT_ERROR();
			goto RET;
		}
		CLS_CONNECTION = cls;
	}

	conn = CALL_PY_FUNCTION(CLS_CONNECTION, NULL);
	if (py_result_code == SGE_ERR) {
		goto RET;
	}

	static PyMethodDef def_close = {"close", py_close_conn, METH_NOARGS, "close connection."};
	static PyMethodDef def_send = {"send", py_send_conn, METH_O, "send content"};
	PyObject_SetAttrString(conn, "__raw_id__", PyLong_FromLong(id));
	PyObject_SetAttrString(conn, "close", PyCFunction_New(&def_close, conn));
	PyObject_SetAttrString(conn, "send", PyCFunction_New(&def_send, conn));
RET:
	Py_XDECREF(module);
	return conn;
}

int
new_conn(sge_message* msg) {
	PyObject* conn = create_conn(msg->id);
	if (NULL == conn) {
		return SGE_ERR;
	}
	CONNECTIONS[msg->id] = conn;
	return SGE_OK;
}

int
on_message(sge_message* msg) {
	PyObject* conn = CONNECTIONS[msg->id];
	assert(conn);
	PyObject* func = PyObject_GetAttrString(conn, "__on_message__");
	assert(func);

	PY_FUNCTION_ENTRY();
	int result = SGE_ERR;
	PyObject* ret = NULL, *arg = NULL;
	sge_buffer* buf = msg->ud;
	size_t len = 0;
	const char* str = buffer_data(buf, &len);
	if (len == 0) {
		goto RET;
	}
	arg = PyBytes_FromStringAndSize(str, len);

	ret = CALL_PY_FUNCTION(func, "O", arg);
	if (py_result_code == SGE_ERR) {
		goto SUCCESS;
	}
	ret = call_cb(conn);
	if (ret == Py_False && HAVE_SCRIPT_ERROR()) {
		CHECK_SCRIPT_ERROR();
		goto RET;
	}

SUCCESS:
	result = SGE_OK;
RET:
	Py_XDECREF(arg);
	Py_XDECREF(func);
	if (result == SGE_ERR) {
		output_error(msg->id);
	}
	return result;
}

int
on_read_done(sge_message* msg) {
	PyObject* conn = CONNECTIONS[msg->id];
	assert(conn);
	PyObject* func = PyObject_GetAttrString(conn, "__on_read_done__");
	assert(func);

	PY_FUNCTION_ENTRY();
	PyObject* ret = CALL_PY_FUNCTION(func, NULL);
	if (py_result_code == SGE_ERR) {
		goto RET;
	}
RET:
	Py_DECREF(ret);
	Py_XDECREF(func);
	return SGE_OK;
}

PyObject*
call_cb(PyObject* conn) {
	PyObject* func = PyObject_GetAttrString(conn, "__gen_object__");
	assert(func);
	PY_FUNCTION_ENTRY();
	PyObject* objs = CALL_PY_FUNCTION(func, NULL);
	if (py_result_code == SGE_ERR) {
		Py_DECREF(objs);
		return Py_False;
	}
	PyObject* req = PyTuple_GetItem(objs, 0);
	PyObject* res = PyTuple_GetItem(objs, 1);
	PyObject* result = CALL_PY_FUNCTION(CALLBACK_FUNC, "OO", req, res);
	if (py_result_code == SGE_ERR) {
		Py_DECREF(objs);
		return Py_False;
	}
	Py_DECREF(objs);
	return Py_True;
}

PyObject*
py_close_conn(PyObject* conn, PyObject* args) {
	int id = conn_id(conn);
	close_conn(id);
	Py_RETURN_TRUE;
}

PyObject*
py_send_conn(PyObject* conn, PyObject* msg) {
	if (!PyUnicode_Check(msg)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be str.");
		Py_RETURN_NONE;
	}

	size_t output_len = 0;
	const char* s_output = PyUnicode_AsUTF8AndSize(msg, &output_len);
	if (output_len) {
		int id = conn_id(conn);
		sge_buffer* output_buf = create_buffer_ex(s_output, output_len);
		sendto_server(CMD_MESSAGE, id, destroy_buffer, output_buf);
	}
	Py_RETURN_TRUE;
}

int
close_conn(int id) {
	Py_DECREF(CONNECTIONS[id]);
	CONNECTIONS[id] = NULL;
	sendto_server(CMD_CLOSE, id, NULL, NULL);
	return SGE_OK;
}

int
conn_id(PyObject* conn) {
	PyObject* py_raw_id = PyObject_GetAttrString(conn, "__raw_id__");
	int id = PyLong_AS_LONG(py_raw_id);
	Py_DECREF(py_raw_id);
	return id;
}

static int
on_request(sge_message* msg) {
	cb_worker cb = MESSAGE_CBS[msg->type];
	if (!cb) {
		ERROR("unknown message type: %d", msg->type);
		return;
	}
	return cb(msg);
}


static int
get_filename(const char* file, char* name) {
	int len = 0;
	char* base = basename((char *)file);
	char* p = strstr(base, ".");
	if (NULL == p) {
		len = strlen(base);
	} else {
		len = p - base;
	}
	if (len == 0) {
		ERROR("module name len is zero.\n");
		return SGE_ERR;
	}
	strncpy(name, base, len);
	name[len + 1] = '\0';
	return SGE_OK;
}

static int
parser_daemon(PyObject* py_config, sge_config* config) {
	int daemon = 0;
	int code = SGE_OK;
	PyObject* py_daemon = NULL;

	py_daemon = PyDict_GetItemString(py_config, "daemon");
	if (NULL == py_daemon) {
		code = SGE_OK;
		goto RET;
	}

	if (!PyBool_Check(py_daemon)) {
		fprintf(stderr, "config.daemon must be boolean\n");
		code = SGE_ERR;
		goto RET;
	}

	if (py_daemon == Py_False) {
		goto RET;
	}
	PARSE_STRING(py_config, logfile, config, 0);

RET:
	Py_XDECREF(py_daemon);
	config->daemon = daemon;
	return code;
}

static int
parse_config(PyObject* py_config, sge_config* config) {
	PARSE_STRING(py_config, workdir, config, 0);
	PARSE_STRING(py_config, entry_file, config, 0);
	PARSE_STRING(py_config, entry_func, config, 0);
	PARSE_STRING(py_config, socket, config, 0);
	PARSE_STRING(py_config, user, config, 1);
	return parser_daemon(py_config, config);
}

static int
add_syspath(const char* path) {
	PyObject* syspath = PySys_GetObject("path");
	PyObject* dir = PyUnicode_FromString(path);
	PyObject* lib_dir = PyUnicode_Concat(dir, PyUnicode_FromString("/python-lib"));
	PyList_Insert(syspath, 0, lib_dir);
	PyList_Insert(syspath, 1, dir);
	Py_DECREF(dir);
	Py_DECREF(lib_dir);
	Py_DECREF(syspath);
	return SGE_OK;
}

static int
load_entry_file(sge_config* config) {
	add_syspath(config->workdir);

	PyObject* module = PyImport_ImportModule(config->entry_file);
	if (NULL == module) {
		CHECK_SCRIPT_ERROR();
		return SGE_ERR;
	}

	PyObject* func = PyObject_GetAttrString(module, config->entry_func);
	if (NULL == func) {
		CHECK_SCRIPT_ERROR();
		return SGE_ERR;
	}

	Py_DECREF(module);
	CALLBACK_FUNC = func;

	return SGE_OK;
}

int
init_env() {
	Py_Initialize();
	if (!Py_IsInitialized()) {
		return SGE_ERR;
	}
	PyEval_InitThreads();
	return SGE_OK;
}

int
load_config(const char* file, sge_config* config) {
	int len = 0;
	int retcode = SGE_OK;
	FILE* fp = fopen(file, "rb");
	if (NULL == fp) {
		SYS_ERROR();
		return SGE_ERR;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (len > MAX_FILE_SIZE) {
		fclose(fp);
		ERROR("config file is too big.\n");
		return SGE_ERR;
	}

	char buffer[MAX_FILE_SIZE];
	fread(buffer, MAX_FILE_SIZE, 1, fp);
	buffer[len] = '\0';
	fclose(fp);

	char filename[64];
	PyObject* code = NULL, *module = NULL, *py_config = NULL;
	if (get_filename(file, filename) == SGE_ERR) {
		return SGE_ERR;
	}

	code = Py_CompileString(buffer, file, Py_file_input);
	if (NULL == code) {
		goto ERROR;
	}

	module = PyImport_ExecCodeModule(filename, code);
	if (NULL == module) {
		goto ERROR;
	}

	py_config = PyObject_GetAttrString(module, "config");
	if (NULL == py_config) {
		goto ERROR;
	}

	if (parse_config(py_config, config) == SGE_ERR) {
		goto ERROR;
	}

	if (load_entry_file(config) == SGE_ERR) {
		goto ERROR;
	}

	config->cb = on_request;
	goto RET;

ERROR:
	retcode = SGE_ERR;
	CHECK_SCRIPT_ERROR();
RET:
	Py_XDECREF(py_config);
	Py_XDECREF(module);
	Py_XDECREF(code);
	return retcode;
	return SGE_OK;
}

int
destroy_env() {
	Py_Finalize();
	return SGE_OK;
}
