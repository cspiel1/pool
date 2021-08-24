#!/usr/bin/env python3
"""
Very simple HTTP server in python for logging requests
Usage::
    ./server.py [<port>]
"""
from http.server import BaseHTTPRequestHandler, HTTPServer
import logging


htmlform= \
"<h2>Pool Saltwater System</h2>" \
"" \
"<form action=\"\" method=\"post\">\n" \
"<label for=\"stime\">Start time:</label><br>\n" \
"<input type=\"time\" id=\"stime\" name=\"stime\" min=\"08:00\" max=\"19:00\" value=\"11:54:00\"><br>\n" \
"  <br>\n" \
"<label for=\"duration\">Duration</label><br>\n" \
"<input type=\"range\" id=\"duration\" name=\"duration\" min=\"1\" max=\"8\"><br>\n" \
"  <br>\n" \
"  <input type=\"submit\" value=\"Ok\"><br>\n" \
"  <br>\n" \
"  <br>\n" \
"  <br>\n" \
"  <input type=\"radio\" id=\"no\" name=\"command\" checked=\"checked\">\n" \
"  <label for=\"upgrade\">-- none --</label><br>\n" \
"  <input type=\"radio\" id=\"upgrade\" name=\"command\" value=\"upgrade\">\n" \
"  <label for=\"upgrade\">Upgrade</label><br>\n" \
"  <input type=\"radio\" id=\"reboot\" name=\"command\" value=\"reboot\">\n" \
"  <label for=\"reboot\">Reboot</label><br>\n" \
"</form>\n" \

class S(BaseHTTPRequestHandler):
    def _set_response(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()

    def do_GET(self):
        logging.info("GET request,\nPath: %s\nHeaders:\n%s\n", str(self.path), str(self.headers))
        self._set_response()
        self.wfile.write(htmlform.format(self.path).encode('utf-8'))

    def do_POST(self):
        content_length = int(self.headers['Content-Length']) # <--- Gets the size of data
        post_data = self.rfile.read(content_length) # <--- Gets the data itself
        logging.info("POST request,\nPath: %s\nHeaders:\n%s\n\nBody:\n%s\n",
                str(self.path), str(self.headers), post_data.decode('utf-8'))

        self._set_response()
        self.wfile.write("POST request for {}".format(self.path).encode('utf-8'))

def run(server_class=HTTPServer, handler_class=S, port=8080):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info('Starting httpd at port %s\n', port)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('Stopping httpd...\n')

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
