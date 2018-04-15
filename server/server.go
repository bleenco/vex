package vexserver

import (
	"net/http"
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
	w.WriteHeader(http.StatusNotFound)
	w.Write([]byte("Not found"))
}
