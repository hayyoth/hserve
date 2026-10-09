# hserve

A tiny static file server written in C.

Serve local files over HTTP with a single command.

```text
      CLIENT                          HSERVE
        |                               |
        |  GET /index.html HTTP/1.1     |
        | ----------------------------> |
        |                         .-----------.
        |                         |  HSERVE   |
        |                         |  SERVER   |
        |                         '-----------'
        |                               |
        |  HTTP/1.1 200 OK              |
        |  Content-Type: text/html      |
        | <---------------------------- |
        |                               |

```

> **⚠️ Note:** `hserve` is intended for local web development and testing only. It has not undergone rigorous security testing. **Do not expose it to the public internet or use it in production.**

## Install

Requires a POSIX environment with a C compiler.

```sh
make
sudo make install
```

By default, `hserve` is installed to `/usr/local/bin`.

## Uninstall

```sh
sudo make uninstall
```

## Usage

```text
hserve [-p port] [-h host] [path]
```

Options:

* `-p` — Port to listen on (default: `8080`)
* `-h` — Host address to bind to (default: `127.0.0.1`)
* `--help` — Show help

`path` is the directory to serve (default: current directory).

## Examples

Serve the current directory:

```sh
hserve
```

Open `http://127.0.0.1:8080` in your browser.

Serve a specific directory:

```sh
hserve ./example-web
```

Use a different port:

```sh
hserve -p 3000 ./example-web
```

Bind to a specific address:

```sh
hserve -h 127.0.0.1 -p 8080 your-sites
```

Listen on all IPv4 interfaces:

```sh
hserve -h 0.0.0.0 -p 8080 ./your-sites
```

## Features

* Serve static files over HTTP
* Support `GET` and `HEAD` requests
* Set content types based on file extensions
* Support nested directories
* Restrict file access to the configured document root
* Handle multiple client connections
* No external dependencies beyond the POSIX environment

## Project Structure

```text
hserve/
├── example-web/   # Sample website
├── build/         # Compiled binary and object files
├── main.c         # Entry point
├── server.c       # Server setup and main loop
├── socket.c       # Socket handling
├── http.c         # HTTP request handling
├── file.c         # File access and content types
├── secure.c       # URL path validation
├── Makefile
└── README.md
```
