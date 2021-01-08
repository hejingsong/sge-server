#! /usr/bin/env python
#-*- coding:utf-8 -*-


def start(request, response):
    response.end("<H1>Hello SgeServer.</H1>")
    return True
