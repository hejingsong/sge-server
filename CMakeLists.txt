CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
MESSAGE(STATUS "CMake version: ${CMAKE_VERSION}")

PROJECT(sge-server)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF()
MESSAGE(STATUS "Build Type ${CMAKE_BUILD_TYPE}")

SET(PY_CONFIG python3-config)

EXECUTE_PROCESS(
    COMMAND ${PY_CONFIG} --includes
    COMMAND awk "{print $2}"
    COMMAND cut -c 3-
    OUTPUT_VARIABLE PY_INC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
EXECUTE_PROCESS(
    COMMAND ${PY_CONFIG} --libs
    COMMAND awk "{print $1}"
    COMMAND cut -c 3-
    OUTPUT_VARIABLE PY_LIB_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
EXECUTE_PROCESS(
    COMMAND ${PY_CONFIG} --ldflags
    COMMAND awk "{print $1}"
    COMMAND cut -c 3-
    OUTPUT_VARIABLE PY_LIB_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

INCLUDE_DIRECTORIES(./src)

# include
MESSAGE(STATUS "Python Include Path ${PY_INC}")
INCLUDE_DIRECTORIES(${PY_INC})

# libs
MESSAGE(STATUS "Python Library ${PY_LIB_DIR}")
MESSAGE(STATUS "Python Libs ${PY_LIB_NAME}")
FIND_LIBRARY(PY_LIB
    NAMES ${PY_LIB_NAME}
    PATHS ${PY_LIB_DIR}
)
LINK_LIBRARIES(${PY_LIB})

# pthread
SET(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-pthread")

SET(SRC
    src/main.c
    src/python-src/env.c
    src/os/server.c
    src/os/event.c
    src/os/socket.c
    src/core/queue.c
    src/core/buffer.c
    src/core/list.c
    src/core/log.c
)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC})