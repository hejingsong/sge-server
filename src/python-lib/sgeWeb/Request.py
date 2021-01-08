#! coding:utf-8

class Request(dict):

	def __init__(self, method, path, version, headers, body, raw):
		self.method = method
		self.path = path
		self.version = version
		self.headers = headers
		self.body = body
		self.raw = raw
		self.args = {}
		self.parsePath()

	def getPath(self):
		return self.path

	def header(self, key, value):
		return self.headers.get(key, value).decode()

	def get(self, field, value=None):
		return self.args.get(field, value)

	def post(self, field, value=None):
		return self.body.get(field, value)

	def parsePath(self):
		self.path = self.path.decode()
		result = self.path.split("?")
		if len(result) == 1:
			return
		self.path = result[0]
		str_args = result[1]
		args = str_args.split("&")
		for arg in args:
			[key, val] = arg.split("=")
			self.args[key] = val
