package tunnel

import (
	"fmt"
	"io"
	"net"

	"github.com/bleenco/vex/logger"
	"github.com/bleenco/vex/proto"
)

// TCPProxy forwards TCP streams.
type TCPProxy struct {
	// localAddr specifies default TCP address of the local server.
	localAddr string
	// localAddrMap specifies mapping from ControlMessage.ForwardedHost to
	// local server address, keys may contain host and port, only host or
	// only port. The order of precedence is the following
	// * host and port
	// * port
	// * host
	localAddrMap map[string]string
	// logger is the proxy logger.
	logger *logger.Logger
}

// NewTCPProxy creates new direct TCPProxy, everything will be proxied to
// localAddr.
func NewTCPProxy(localAddr string, log *logger.Logger) *TCPProxy {
	if log == nil {
		log = logger.NewLogger(false)
	}

	return &TCPProxy{
		localAddr: localAddr,
		logger:    log,
	}
}

// NewMultiTCPProxy creates a new dispatching TCPProxy, connections may go to
// different backends based on localAddrMap.
func NewMultiTCPProxy(localAddrMap map[string]string, log *logger.Logger) *TCPProxy {
	if log == nil {
		log = logger.NewLogger(false)
	}

	return &TCPProxy{
		localAddrMap: localAddrMap,
		logger:       log,
	}
}

// Proxy is a ProxyFunc.
func (p *TCPProxy) Proxy(w io.Writer, r io.ReadCloser, msg *proto.ControlMessage) {
	switch msg.ForwardedProto {
	case proto.TCP, proto.TCP4, proto.TCP6, proto.UNIX:
	// ok
	default:
		p.logger.Errorf("tcp proxy: unsupported protocol, control message: %s", msg)
		return
	}

	target := p.localAddrFor(msg.ForwardedHost)
	if target == "" {
		p.logger.Errorf("tcp proxy: no target, control message: %s", msg)
		return
	}

	local, err := net.DialTimeout("tcp", target, DefaultTimeout)
	if err != nil {
		p.logger.Errorf("tcp proxy: dial failed, target: %s, control message: %s, reason: %s", target, msg, err)
		return
	}
	defer local.Close()

	if err := keepAlive(local); err != nil {
		p.logger.Errorf("tcp proxy: TCP keepalive for tunneled connection failed, target: %s, control message: %s, err: %s", target, msg, err)
	}

	done := make(chan struct{})
	go func() {
		transfer(flushWriter{w}, local, p.logger)
		close(done)
	}()

	transfer(local, r, p.logger)

	<-done
}

func (p *TCPProxy) localAddrFor(hostPort string) string {
	if len(p.localAddrMap) == 0 {
		return p.localAddr
	}

	// try hostPort
	if addr := p.localAddrMap[hostPort]; addr != "" {
		return addr
	}

	// try port
	host, port, _ := net.SplitHostPort(hostPort)
	if addr := p.localAddrMap[port]; addr != "" {
		return addr
	}

	// try 0.0.0.0:port
	if addr := p.localAddrMap[fmt.Sprintf("0.0.0.0:%s", port)]; addr != "" {
		return addr
	}

	// try host
	if addr := p.localAddrMap[host]; addr != "" {
		return addr
	}

	return p.localAddr
}
