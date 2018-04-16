package vexserver

import (
	"net/http"
	"net/http/httputil"
	"net/url"
	"strconv"
	"strings"
)

type Config struct {
	PrivateKeyFile string
	SSHListenAddr  string
	HTTPListenAddr string
}

type Server struct {
	sshServer  *SSHServer
	httpServer *HTTPServer
	config     *Config
}

func NewServer(config *Config) *Server {
	return &Server{
		sshServer:  NewSSHServer(),
		httpServer: NewHTTPServer(),
		config:     config,
	}
}

func (s *Server) Run() error {
	err := s.sshServer.Start(s.config.SSHListenAddr, s.config.PrivateKeyFile)
	if err != nil {
		return err
	}

	var h http.Handler = http.HandlerFunc(s.handleHTTP)
	err = s.httpServer.GoListenAndServe(s.config.HTTPListenAddr, h)
	if err != nil {
		return err
	}

	return s.httpServer.Wait()
}

func (s *Server) handleHTTP(w http.ResponseWriter, r *http.Request) {
	host := r.Host
	userID := host
	pos := strings.IndexByte(host, '.')
	if pos > 0 {
		userID = host[:pos]
	}

	if client, ok := s.sshServer.clients[userID]; ok {
		w.Header().Set("X-Proxy", "vexd")
		url, _ := url.Parse("http://" + client.Addr + ":" + strconv.Itoa(int(client.Port)))
		proxy := httputil.NewSingleHostReverseProxy(url)
		proxy.ServeHTTP(w, r)
	}

	w.WriteHeader(http.StatusNotFound)
	w.Write([]byte("Not found"))
}
