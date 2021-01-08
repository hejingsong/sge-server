#ifndef COMMON_H_
#define COMMON_H_


#define HAVE_SCRIPT_ERROR() PyErr_Occurred()

#define CHECK_SCRIPT_ERROR()            \
if (HAVE_SCRIPT_ERROR()) {              \
    PyErr_Print();                      \
}

#endif
