## vex

Vex is a reverse HTTP proxy tunnel via secure SSH connections. It is compatible with `ssh` client.

<p align="center">
  <img src="https://user-images.githubusercontent.com/1796022/38793799-f23c981a-4152-11e8-9e6c-382ce6536c87.png">
</p>

### Establish tunnel via running vex server on bleenco.space

Let's say you are running HTTP server locally on port 6500, then command would be:

```sh
ssh -R 10100:localhost:6500 bleenco.space -p 2200
```

where 10100 is randomly picked port to bind SSH listener on server to establish tunnels. You should pick that random and don't use 10100 if not free.

2200 is port where vex daemon (server) is running and localhost:6500 is local HTTP server.

Example output:

```sh
$ ssh -R 10500:localhost:6500 bleenco.space -p 2200
[vexd] Generated URL: http://23c41c01.bleenco.space
```

Then open generated URL in the browser to check if works, then share the URL if needed.

### Run cross-compilation build

```sh
make clean && make build
```

### Running server

```sh
./build/vex-server-linux_amd64 --help
```

```
Usage: vexd [options]

Options:

-d, Domain name that HTTP server is hosted on. It is
used for generating subdomain IDs (defaults to the
environment variable VEX_DOMAIN and falls back to local.net)

-k, Path to file of a ECDSA private key. All SSH communication
will be secured using this key (defaults to the VEX_KEY environment
variable falls back to id_rsa)

-s, SSH server listen address (defaults to VEX_SSH_SERVER and
falls back to 0.0.0.0:2200)

-http, HTTP server listen address (defaults to VEX_HTTP_SERVER and
falls back to 0.0.0.0:2000)
```

### Licence

```
The MIT License

Copyright (c) 2018 Bleenco GmbH https://bleenco.com

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
```
