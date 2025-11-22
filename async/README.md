# HTTP/1.1 server in C
This is a very simple HTTP/1.1 server written in C that implements the GET request of the [HTTP/1.1 RfC-2616](https://datatracker.ietf.org/doc/html/rfc2616).
I was inspired to write this server by the great ["Beej's guide to Network Programming"](https://beej.us/guide/bgnet/html/). The
server runs fully asynchronous supporting up to MAX_CLIENT_SIZE connections and utilizes the Linux poll system call
to check for data on connected sockets.

## How to use
Put HTML files into the static folder, run the server and go to localhost:8080.
