#! /usr/bin/env python
#-*- coding:utf-8 -*-

import json

import sgeWeb.Request as Request
import sgeWeb.Response as Response

HTTP_STATUS_MAP = {
	100: "Continue",
	101: "Switching Protocols",
	102: "Processing",
	200: "OK",
	201: "Created",
	202: "Accepted",
	203: "Non-Authoritative Information",
	204: "No Content",
	205: "Reset Content",
	206: "Partial Content",
	207: "Multi-Status",
	208: "Already Reported",
	226: "IM Used",
	300: "Multiple Choices",
	301: "Moved Permanently",
	302: "Found",
	303: "See Other",
	304: "Not Modified",
	305: "Use Proxy",
	307: "Temporary Redirect",
	308: "Permanent Redirect",
	400: "Bad Request",
	401: "Unauthorized",
	402: "Payment Required",
	403: "Forbidden",
	404: "Not Found",
	405: "Method Not Allowed",
	406: "Not Acceptable",
	407: "Proxy Authentication Required",
	408: "Request Timeout",
	409: "Conflict",
	410: "Gone",
	411: "Length Required",
	412: "Precondition Failed",
	413: "Payload Too Large",
	414: "URI Too Long",
	415: "Unsupported Media Type",
	416: "Range Not Satisfiable",
	417: "Expectation Failed",
	421: "Misdirected Request",
	422: "Unprocessable Entity",
	423: "Locked",
	424: "Failed Dependency",
	426: "Upgrade Required",
	428: "Precondition Required",
	429: "Too Many Requests",
	431: "Request Header Fields Too Large",
	451: "Unavailable For Legal Reasons",
	500: "Internal Server Error",
	501: "Not Implemented",
	502: "Bad Gateway",
	503: "Service Unavailable",
	504: "Gateway Timeout",
	505: "HTTP Version Not Supported",
	506: "Variant Also Negotiates",
	507: "Insufficient Storage",
	508: "Loop Detected",
	510: "Not Extended",
	511: "Network Authentication Required"
}


class Connection(dict):

	def __init__(self):
		self.__parse_done__ = False
		self.__read_done__ = False
		self.__raw_message__ = b''
		self.headers = {}
		self.path = ''
		self.method = ''
		self.version = ''
		self.body = {}
	
	def close(self):
		''' 不用处理，底层替换 '''
		pass

	def send(self, msg):
		''' 不用处理，底层替换 '''
		pass

	def output(self, msg):
		self.send(msg)
		if self.__read_done__:
			self.close()

	def parse_start_line(self, str_header):
		[method, path, version] = str_header.split(b" ")
		self.method = method
		self.path = path
		self.version = version

	def parse_header(self, str_header):
		headers = {}
		lines = str_header.split(b"\r\n")
		for line in lines:
			[field, value] = line.split(b":", 1)
			k = field.strip().decode()
			headers[k] = value.strip()
		return headers

	def parse_request_header(self, str_header):
		[str_start_line, str_header] = str_header.split(b"\r\n", 1)
		self.parse_start_line(str_start_line)
		self.headers = self.parse_header(str_header)

	def parse_request_body(self, headers, str_body):
		if not "Content-Length" in headers or not "Content-Type" in headers:
			return True
		content_type = headers['Content-Type']
		if content_type.find(b"application/x-www-form-urlencoded") != -1:
			items = str_body.split(b"&")
			for item in items:
				[field, value] = item.split(b"=")
				k = field.strip().decode()
				self.body[k] = value.strip().decode()
			return True

		if content_type.find(b"multipart/form-data") != -1:
			args = {}
			flag = True
			fields = content_type.split(b";")
			for field in fields:
				k, sep, v = field.strip().partition(b"=")
				if k != b"boundary" or not v:
					continue
				flag = self.parse_multipart_form_data(v, str_body, args)
				if not flag:
					break
			if not flag:
				return False
			self.body = args
			return True

		if content_type.find(b"application/json") != -1:
			self.body = json.loads(str_body)
			return True
		return False

	def parse_multipart_form_data(self, boundary, data, args):
		if boundary.startswith(b'"') and boundary.endswith(b'"'):
			boundary = boundary[1:-1]
		final_boundary_index = data.rfind(b"--" + boundary + b"--")
		if final_boundary_index == -1:
			return False
		parts = data[:final_boundary_index].split(b"--" + boundary + b"\r\n")
		for part in parts:
			if not part:
				continue
			eoh = part.find(b"\r\n\r\n")
			if eoh == -1:
				continue
			headers = self.parse_header(part[:eoh])
			disp = headers.get("Content-Disposition", "")
			disposition, disp_params = self.parse_disposition(disp)
			if disposition != b"form-data" or not part.endswith(b"\r\n"):
				continue
			value = part[eoh + 4 : -2]
			if value == b'undefined':
				value = None
			if not disp_params.get("name"):
				continue
			name = disp_params["name"]
			if disp_params.get("filename"):
				if not name in args:
					args[name] = []
				ctype = headers.get("Content-Type", "application/unknown")
				args[name].append({
					"filename": disp_params["filename"],
					"content": value,
					"type": ctype
				})
			else:
				args[name] = value
		return True

	def parse_disposition(self, disp):
		items = disp.split(b";")
		disposition = items[0].strip()
		params = {}
		for opt in items[1:]:
			k, v = opt.split(b"=")
			params[k.strip().decode()] = v.strip().strip(b'"').decode()
		return disposition, params

	def parse_http_request(self):
		if not self.__raw_message__.find(b"\r\n\r\n"):
			return False
		[header, body] = self.__raw_message__.split(b"\r\n\r\n", 1)
		self.parse_request_header(header)
		if not self.parse_request_body(self.headers, body):
			return False
		self.__parse_done__ = True
		return True

	def format_status(self, res):
		return "HTTP/1.1 {status} {message}".format(status=res.status, message=HTTP_STATUS_MAP.get(res.status, "<unknown>"))

	def format_header(self, res):
		s_headers = []
		for k, v in res.headers.items():
			s_headers.append("{0}: {1}".format(k, v))
		if (res.body):
			s_headers.append("Content-Length: {0}".format(len(res.body)))
		return "\r\n".join(s_headers)

	def parse_http_response(self, res):
		return "{status}\r\n{header}\r\n\r\n{body}".format(
			status=self.format_status(res),
			header=self.format_header(res),
			body=res.body
		)

	def __on_message__(self, msg):
		self.__raw_message__ += msg
		return self.parse_http_request()

	def __on_read_done__(self):
		self.__read_done__ = True
		self.close()
		return True

	def __gen_object__(self):
		if not self.__parse_done__:
			return None

		req = Request.Request(self.method, self.path, self.version, self.headers, self.body, self.__raw_message__)
		res = Response.Response(self)
		return (req, res)
