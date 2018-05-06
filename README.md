## vex

Vex is a reverse HTTP proxy tunnel via secure SSH connections. It is compatible with `ssh` client.

### Establish tunnel with vexd server on bleenco.space (ssh client)

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

### Establish tunnel with vexd server on bleenco.space (vex client)

```sh
$ vex -s bleenco.space -p 2200 -ls localhost -lp 7500
```

`vex` client options:

```
Usage: vex [options]

Options:

-s, SSH server remote host (default: bleenco.space)

-p, SSH server remote port (default: 2200)

-ls, Local HTTP server host (default: localhost)

-lp, Local HTTP server port (default: 7500)
```

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

### Establishing tunnel with PuTTY and bleenco.space service

Lets say, you are running local webserver on port 7500.

Open PuTTY;

Host Name (or IP address): `bleenco.space` and Port: `2200`

<img src="https://user-images.githubusercontent.com/1796022/38806500-c09558a2-4179-11e8-88f1-d25a6f3fd4ab.png">

Go to `Category => Connection => SSH => Tunnels` and enter the following

<img src="https://user-images.githubusercontent.com/1796022/38806504-c3961bb8-4179-11e8-9559-b50ec5ca74cd.png">

Click `Open`, if it asks for login username just hit `Enter`.

You tunnel should now be established.

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
