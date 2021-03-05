#! /usr/bin/env python
#-*- coding:utf-8 -*-

config = {
    "workdir": "/root/sge-server/example",
    "entry_file": "main",
    "entry_func": "start",
    "daemon": False,
    "logfile": "./web.log",
    "socket": "/tmp/sge.sock",
    # "user": "www",
    "libdir": "../src/python-lib"
}
