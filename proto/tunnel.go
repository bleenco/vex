package proto

// Tunnel describes a single tunnel between client and server. When connecting
// client send tunnels to server. If client gets connected server proxies
// connections to given Host and Addr to client.
type Tunnel struct {
	// Protocol specifies tunnel protocol, must be one of protocols known
	// by the server.
	Protocol string
	// Host specifies HTTP request host, it's required for HTTP and WS
	Host string
	// Auth specifies HTTP basic auth credentials in form "user:password",
	// if set server would protect HTTP and WS tunnels with basic auth.
	Auth string
	// Addr specifies TCP address server would listen on, it's required
	// for TCP tunnels.
	Addr string
}
