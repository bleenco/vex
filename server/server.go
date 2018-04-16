package vexserver

import (
	"net/http"
	"net/http/httputil"
	"net/url"
	"strconv"
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
	if len(s.sshServer.clients) > 0 {
		client := s.sshServer.clients["jan"]

		w.Header().Set("X-Proxy", "vexd")
		url, _ := url.Parse("http://localhost:" + strconv.Itoa(int(client.Port)))
		proxy := httputil.NewSingleHostReverseProxy(url)
		proxy.ServeHTTP(w, r)
	}

	w.WriteHeader(http.StatusNotFound)
	w.Write([]byte("Not found"))
}
