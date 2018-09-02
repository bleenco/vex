package tunnel

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"path"
	"strings"
	"time"

	"github.com/gobwas/ws"
	"github.com/gobwas/ws/wsutil"

	"github.com/bleenco/vex/id"
	"github.com/bleenco/vex/logger"
	"github.com/bleenco/vex/proto"
	_ "github.com/bleenco/vex/statik" // dashboard UI static files

	"github.com/rakyll/statik/fs"
	"golang.org/x/net/http2"
)

// ServerConfig defines configuration for the Server.
type ServerConfig struct {
	// Addr is TCP address to listen for client connections. If empty ":0"
	// is used.
	Addr string
	// Domain specifies the root domain where server is hosted.
	Domain string
	// AutoSubscribe if enabled will automatically subscribe new clients on
	// first call.
	AutoSubscribe bool
	// TLSConfig specifies the tls configuration to use with tls.Listener.
	TLSConfig *tls.Config
	// Listener specifies optional listener for client connections. If nil
	// tls.Listen("tcp", Addr, TLSConfig) is used.
	Listener net.Listener
	// Logger is optional logger. If nil logging is disabled.
	Logger *logger.Logger
}

// Server is responsible for proxying public connections to the client over a
// tunnel connection.
type Server struct {
	*registry
	config *ServerConfig

	listener   net.Listener
	connPool   *connPool
	httpClient *http.Client
	fs         http.FileSystem
	logger     *logger.Logger
}

// NewServer creates a new Server.
func NewServer(config *ServerConfig) (*Server, error) {
	listener, err := listener(config)
	if err != nil {
		return nil, fmt.Errorf("listener failed: %s", err)
	}

	log := config.Logger
	if log == nil {
		log = logger.NewLogger(false)
	}

	statikFS, err := fs.New()
	if err != nil {
		return nil, err
	}

	s := &Server{
		registry: newRegistry(log),
		config:   config,
		listener: listener,
		fs:       statikFS,
		logger:   log,
	}

	t := &http2.Transport{}
	pool := newConnPool(t, s.disconnected)
	t.ConnPool = pool
	s.connPool = pool
	s.httpClient = &http.Client{
		Transport: t,
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}

	return s, nil
}

func listener(config *ServerConfig) (net.Listener, error) {
	if config.Listener != nil {
		return config.Listener, nil
	}

	if config.Addr == "" {
		return nil, errors.New("missing Addr")
	}
	if config.TLSConfig == nil {
		return nil, errors.New("missing TLSConfig")
	}

	return net.Listen("tcp", config.Addr)
}

// disconnected clears resources used by client, it's invoked by connection pool
// when client goes away.
func (s *Server) disconnected(identifier id.ID) {
	s.logger.Infof("disconnected, identifier: %s", identifier)

	i := s.registry.clear(identifier)
	if i == nil {
		return
	}
	for _, l := range i.Listeners {
		s.logger.Debugf("close listener, identifier: %s, addr: %s", identifier, l.Addr())
		l.Close()
	}
}

// Start starts accepting connections form clients. For accepting http traffic
// from end users server must be run as handler on http server.
func (s *Server) Start() {
	addr := s.listener.Addr().String()

	s.logger.Infof("start, addr: %s", addr)

	for {
		conn, err := s.listener.Accept()
		if err != nil {
			if strings.Contains(err.Error(), "use of closed network connection") {
				s.logger.Infof("control connection listener closed, addr: %s", addr)
				return
			}

			s.logger.Errorf("accept or control connection failed, addr: %s, reason: %s", addr, err)
			continue
		}

		if err := keepAlive(conn); err != nil {
			s.logger.Errorf("TCP keepalive for control connection failed, addr: %s, reason: %s", addr, err)
		}

		go s.handleClient(tls.Server(conn, s.config.TLSConfig))
	}
}

func (s *Server) handleClient(conn net.Conn) {
	log := logger.NewLoggerWithPrefix(conn.RemoteAddr().String(), s.logger.Debug)

	log.Infof("try connect")

	var (
		identifier id.ID
		req        *http.Request
		resp       *http.Response
		tunnels    map[string]*proto.Tunnel
		err        error
		ok         bool

		inConnPool bool
	)

	tlsConn, ok := conn.(*tls.Conn)
	if !ok {
		log.Errorf("invalid connection type, reason: %v", fmt.Errorf("expected TLS conn, got %T", conn))
		goto reject
	}

	identifier, err = id.PeerID(tlsConn)
	if err != nil {
		log.Errorf("certificate error: %v", err)
		goto reject
	}

	log = logger.NewLoggerWithPrefix(identifier.String(), s.logger.Debug)

	if s.config.AutoSubscribe {
		s.Subscribe(identifier)
	} else if !s.IsSubscribed(identifier) {
		log.Errorf("unknown client")
		goto reject
	}

	if err = conn.SetDeadline(time.Time{}); err != nil {
		log.Errorf("setting infinite deadline failed, reason: %s", err)
		goto reject
	}

	if err := s.connPool.AddConn(conn, identifier); err != nil {
		log.Errorf("adding connection failed, reason: %s", err)
		goto reject
	}
	inConnPool = true

	req, err = http.NewRequest(http.MethodConnect, s.connPool.URL(identifier), nil)
	if err != nil {
		log.Errorf("handshake request creating failed, reason: %s", err)
		goto reject
	}

	{
		ctx, cancel := context.WithTimeout(context.Background(), DefaultTimeout)
		defer cancel()
		req = req.WithContext(ctx)
	}

	resp, err = s.httpClient.Do(req)
	if err != nil {
		log.Errorf("handshake failed, reason: %s", err)
		goto reject
	}

	if resp.StatusCode != http.StatusOK {
		err = fmt.Errorf("Status %s", resp.Status)
		log.Errorf("handshake failed, reason: %v", err)
		goto reject
	}

	if resp.ContentLength == 0 {
		err = fmt.Errorf("Tunnels Content-Legth: 0")
		log.Errorf("handshake failed, reason: %v", err)
		goto reject
	}

	if err = json.NewDecoder(&io.LimitedReader{R: resp.Body, N: 126976}).Decode(&tunnels); err != nil {
		log.Errorf("handshake failed, reason: %v", err)
		goto reject
	}

	if len(tunnels) == 0 {
		err = fmt.Errorf("No tunnels")
		log.Errorf("handshake failed, reason: %v", err)
		goto reject
	}

	if err = s.addTunnels(tunnels, identifier); err != nil {
		log.Errorf("handshake failed, reason: %v", err)
		goto reject
	}

	log.Infof("connected")

	return

reject:
	log.Errorf("rejected")

	if inConnPool {
		s.notifyError(err, identifier)
		s.connPool.DeleteConn(identifier)
	}

	conn.Close()
}

// notifyError tries to send error to client.
func (s *Server) notifyError(serverError error, identifier id.ID) {
	if serverError == nil {
		return
	}

	req, err := http.NewRequest(http.MethodConnect, s.connPool.URL(identifier), nil)
	if err != nil {
		s.logger.Errorf("client error notification failed, identifier: %s, err: %v", identifier, err)
		return
	}

	req.Header.Set(proto.HeaderError, serverError.Error())

	ctx, cancel := context.WithTimeout(context.Background(), DefaultTimeout)
	defer cancel()

	s.httpClient.Do(req.WithContext(ctx))
}

// addTunnels invokes addHost or addListener based on data from proto.Tunnel. If
// a tunnel cannot be added whole batch is reverted.
func (s *Server) addTunnels(tunnels map[string]*proto.Tunnel, identifier id.ID) error {
	i := &RegistryItem{
		Hosts:     []*HostAuth{},
		Listeners: []net.Listener{},
	}

	var err error
	for name, t := range tunnels {
		switch t.Protocol {
		case proto.HTTP:
			i.Hosts = append(i.Hosts, &HostAuth{t.Host, NewAuth(t.Auth)})
		case proto.TCP, proto.TCP4, proto.TCP6, proto.UNIX:
			var l net.Listener
			l, err = net.Listen(t.Protocol, t.Addr)
			if err != nil {
				goto rollback
			}

			s.logger.Debugf("open listener, identifier: %s, addr: %s", identifier, l.Addr())

			i.Listeners = append(i.Listeners, l)
		default:
			err = fmt.Errorf("unsupported protocol for tunnel %s: %s", name, t.Protocol)
			goto rollback
		}
	}

	err = s.set(i, identifier)
	if err != nil {
		goto rollback
	}

	for _, l := range i.Listeners {
		go s.listen(l, identifier)
	}

	return nil

rollback:
	for _, l := range i.Listeners {
		l.Close()
	}

	return err
}

// Unsubscribe removes client from registry, disconnects client if already
// connected and returns it's RegistryItem.
func (s *Server) Unsubscribe(identifier id.ID) *RegistryItem {
	s.connPool.DeleteConn(identifier)
	return s.registry.Unsubscribe(identifier)
}

// Ping measures the RTT response time.
func (s *Server) Ping(identifier id.ID) (time.Duration, error) {
	return s.connPool.Ping(identifier)
}

func (s *Server) listen(l net.Listener, identifier id.ID) {
	addr := l.Addr().String()

	for {
		conn, err := l.Accept()
		if err != nil {
			if strings.Contains(err.Error(), "use of closed network connection") {
				s.logger.Debugf("listener closed, identifier: %s, addr: %s", identifier, addr)
				return
			}

			s.logger.Errorf("accept of connection failed, identifier: %s, addr: %s, err: %v", identifier, addr, err)
			continue
		}

		msg := &proto.ControlMessage{
			Action:         proto.ActionProxy,
			ForwardedHost:  l.Addr().String(),
			ForwardedProto: l.Addr().Network(),
		}

		if err := keepAlive(conn); err != nil {
			s.logger.Errorf("TCP keepalive for tunneled connection failed, identifier: %s, control message: %s, err: %v", identifier, msg, err)
		}

		go func() {
			if err := s.proxyConn(identifier, conn, msg); err != nil {
				s.logger.Errorf("proxy error, identifier: %s, control message: %s, err: %v", identifier, msg, err)
			}
		}()
	}
}

// ServeHTTP proxies http connection to the client.
func (s *Server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.Host == s.config.Domain {
		if r.URL.String() == "/ws" {
			conn, _, _, err := ws.UpgradeHTTP(r, w)
			if err != nil {
				// handle error
			}

			go func() {
				defer conn.Close()

				for {
					msg, op, err := wsutil.ReadClientData(conn)
					if err != nil {
						return
					}
					err = wsutil.WriteServerMessage(conn, op, msg)
					if err != nil {
						return
					}
				}
			}()
		} else {
			http.FileServer(&routerWrapper{s.fs}).ServeHTTP(w, r)
		}
	} else {
		resp, err := s.RoundTrip(r)
		if err == errUnauthorised {
			w.Header().Set("WWW-Authenticate", "Basic realm=\"User Visible Realm\"")
			http.Error(w, err.Error(), http.StatusUnauthorized)
			return
		}
		if err != nil {
			s.logger.Errorf("round trip failed, addr: %s, host: %s, url: %s, err: %v", r.RemoteAddr, r.Host, r.URL, err)

			http.Error(w, err.Error(), http.StatusBadGateway)
			return
		}
		defer resp.Body.Close()

		copyHeader(w.Header(), resp.Header)
		w.WriteHeader(resp.StatusCode)

		transfer(w, resp.Body, logger.NewLoggerWithPrefix("dir: client to user, dst: "+r.RemoteAddr+" src: "+r.Host, s.logger.Debug))
	}
}

// RoundTrip is http.RoundTriper implementation.
func (s *Server) RoundTrip(r *http.Request) (*http.Response, error) {
	identifier, auth, ok := s.Subscriber(r.Host)
	if !ok {
		return nil, errClientNotSubscribed
	}

	outr := r.WithContext(r.Context())
	if r.ContentLength == 0 {
		outr.Body = nil // Issue 16036: nil Body for http.Transport retries
	}
	outr.Header = cloneHeader(r.Header)

	if auth != nil {
		user, password, _ := r.BasicAuth()
		if auth.User != user || auth.Password != password {
			return nil, errUnauthorised
		}
		outr.Header.Del("Authorization")
	}

	setXForwardedFor(outr.Header, r.RemoteAddr)

	scheme := r.URL.Scheme
	if scheme == "" {
		if r.TLS != nil {
			scheme = proto.HTTPS
		} else {
			scheme = proto.HTTP
		}
	}
	if r.Header.Get("X-Forwarded-Host") == "" {
		outr.Header.Set("X-Forwarded-Host", r.Host)
		outr.Header.Set("X-Forwarded-Proto", scheme)
	}

	msg := &proto.ControlMessage{
		Action:         proto.ActionProxy,
		ForwardedHost:  r.Host,
		ForwardedProto: scheme,
	}

	return s.proxyHTTP(identifier, outr, msg)
}

func (s *Server) proxyConn(identifier id.ID, conn net.Conn, msg *proto.ControlMessage) error {
	s.logger.Debugf("proxy connection, identifier: %s, control message: %s", identifier, msg)

	defer conn.Close()

	pr, pw := io.Pipe()
	defer pr.Close()
	defer pw.Close()

	req, err := s.connectRequest(identifier, msg, pr)
	if err != nil {
		return err
	}

	ctx, cancel := context.WithCancel(context.Background())
	req = req.WithContext(ctx)

	done := make(chan struct{})
	go func() {
		transfer(pw, conn, logger.NewLoggerWithPrefix("dir: user to client, dst: "+identifier.String()+" src: "+conn.RemoteAddr().String(), s.logger.Debug))
		cancel()
		close(done)
	}()

	resp, err := s.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("io error: %s", err)
	}
	defer resp.Body.Close()

	transfer(conn, resp.Body, logger.NewLoggerWithPrefix("dir: client to user, dst: "+conn.RemoteAddr().String()+" src: "+identifier.String(), s.logger.Debug))

	<-done

	s.logger.Debugf("proxy connection done, identifier: %s, control message: %s", identifier, msg)

	return nil
}

func (s *Server) proxyHTTP(identifier id.ID, r *http.Request, msg *proto.ControlMessage) (*http.Response, error) {
	s.logger.Debugf("proxy HTTP, identifier: %s, control message: %s", identifier, msg)

	pr, pw := io.Pipe()
	defer pr.Close()
	defer pw.Close()

	req, err := s.connectRequest(identifier, msg, pr)
	if err != nil {
		return nil, fmt.Errorf("proxy request error: %s", err)
	}

	go func() {
		cw := &countWriter{pw, 0}
		err := r.Write(cw)
		if err != nil {
			s.logger.Debugf("proxy error, identifier: %s, control message: %s, reason: %v", identifier, msg, err)
		}

		s.logger.Debugf("transferred, identifier: %s, bytes: %d, dir: user to client, dst: %s, src: %s", identifier, cw.count, r.Host, r.RemoteAddr)

		if r.Body != nil {
			r.Body.Close()
		}
	}()

	resp, err := s.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("io error: %s", err)
	}

	s.logger.Debugf("proxy HTTP done, identifier: %s, control message: %s, status code: %s", identifier, msg, resp.StatusCode)

	return resp, nil
}

// connectRequest creates HTTP request to client with a given identifier having
// control message and data input stream, output data stream results from
// response the created request.
func (s *Server) connectRequest(identifier id.ID, msg *proto.ControlMessage, r io.Reader) (*http.Request, error) {
	req, err := http.NewRequest(http.MethodPut, s.connPool.URL(identifier), r)
	if err != nil {
		return nil, fmt.Errorf("could not create request: %s", err)
	}
	msg.WriteToHeader(req.Header)

	return req, nil
}

// Addr returns network address clients connect to.
func (s *Server) Addr() string {
	if s.listener == nil {
		return ""
	}
	return s.listener.Addr().String()
}

// Stop closes the server.
func (s *Server) Stop() {
	s.logger.Infof("stop")

	if s.listener != nil {
		s.listener.Close()
	}
}

type routerWrapper struct {
	assets http.FileSystem
}

func (r *routerWrapper) Open(name string) (http.File, error) {
	ret, err := r.assets.Open(name)
	if !os.IsNotExist(err) || path.Ext(name) != "" {
		return ret, err
	}

	return r.assets.Open("/index.html")
}
