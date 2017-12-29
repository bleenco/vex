# VEX

Vex is an improved localtunnel or http tunnel written in ANSI C. This repository provides client and server implementation.

We improve localtunnel in following points:
* Server creates TCP tunnels on demand, only after receiving a request from the client. There are no *n* open sockets awaiting for incomming requests.
* (in progress) Supporting all protocols. Localtunnel supports only TCP tunnels for HTTP protocol.

## Building project

```sh
make
```

Build artifacts will be stored in `build/` directory.

## Running server

```
vex-server

Usage: vex-server [-p <port> [-h] -d <domain>]

Options:

 -h                Show this help message.
 -p <port>         Bind to this port number. Default: 1234
 -d <domain>       Use this domain when generating URLs.

Copyright (c) 2017 Bleenco GmbH http://bleenco.com
```

## Running client to connect to the server

```
vex

Usage: vex [-i <subdomain_id> -b <remote_port> -r <remote_host> -p <local_port> -l <local_host> [-h]]

Options:

 -h                        Show this help message.
 -i <subdomain_id>         Send request to use this subdomain. Randomly generated if not provided.
 -b <remote_port>          Connect to this remote port. Default: 1234
 -r <remote_host>          Connect to this remote host. Mandatory.
 -p <local_port>           Port where your service is running. Mandatory.
 -p <local_host>           Host where your service is running. Default: localhost.

Copyright (c) 2017 Bleenco GmbH http://bleenco.com
```

## Using bleenco.space server

We provide a hosted `vex` server with the `bleenco.space` root domain name free of any charge for you to use it. Feel free to try it out and use it on your projects.

## LICENSE

The MIT License

Copyright (c) 2017 Bleenco GmbH http://bleenco.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
