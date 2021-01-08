#! coding:utf-8

class Response(dict):

	def __init__(self, conn):
		self.conn = conn
		self.status = 200
		self.headers = {}
		self.cookie = {}
		self.body = ''

	def set_header(self, headers):
		self.headers.update(headers)

	def set_cookie(self, key, value='', expire=0 , path='/', domain='', secure=False, httponly=False, samesite=''):
		pass

	def set_status(self, code):
		self.status = code

	def end(self, body):
		self.body = body
		self._send()

	def _send(self):
		msg = self.conn.parse_http_response(self)
		self.conn.output(msg)
