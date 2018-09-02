package main

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"net"
	"net/url"
	"os"

	"github.com/bleenco/vex/certgen"
	"github.com/bleenco/vex/log"
	"github.com/bleenco/vex/proto"
	"github.com/bleenco/vex/tunnel"

	"github.com/cenkalti/backoff"
	kingpin "gopkg.in/alecthomas/kingpin.v2"
)

var (
	addr       = kingpin.Flag("addr", "Remote address").Short('a').Default("0.0.0.0:8080").String()
	host       = kingpin.Flag("host", "Hostname").Short('h').Default("test.bleenco.space").String()
	protocol   = kingpin.Flag("protocol", "Protocol to use, http or tcp").Short('p').Default("http").Enum("http", "tcp")
	serverAddr = kingpin.Flag("serverAddr", "Remote server address").Short('s').Default("bleenco.space:5223").String()
)

func main() {
	kingpin.UsageTemplate(kingpin.CompactUsageTemplate).Version("0.1.0").Author("Bleenco GmbH")
	kingpin.Parse()

	logger := log.NewFilterLogger(log.NewStdLogger(), 1)

	certPath, keyPath := certgen.CheckAndGenerateCert()

	config := ClientConfig{
		TLSCrt: certPath,
		TLSKey: keyPath,
		Backoff: BackoffConfig{
			Interval:    DefaultBackoffInterval,
			Multiplier:  DefaultBackoffMultiplier,
			MaxInterval: DefaultBackoffMaxInterval,
			MaxTime:     DefaultBackoffMaxTime,
		},
		Tunnels: make(map[string]*Tunnel),
	}

	var err error

	if config.ServerAddr, err = normalizeAddress(*serverAddr); err != nil {
		fmt.Fprintf(os.Stderr, "Server address: %s: %v", config.ServerAddr, err)
		os.Exit(1)
	}

	t := &Tunnel{
		Protocol: *protocol,
		Host:     *host,
		Addr:     *addr,
	}

	switch t.Protocol {
	case proto.HTTP:
		if err := validateHTTP(t); err != nil {
			fmt.Fprintf(os.Stderr, "%s", err)
			os.Exit(1)
		}
	case proto.TCP, proto.TCP4, proto.TCP6:
		{
			if err := validateTCP(t); err != nil {
				fmt.Fprintf(os.Stderr, "%s", err)
				os.Exit(1)
			}
		}
	}

	config.Tunnels["default"] = t

	tlsconf, err := tlsConfig(&config)
	if err != nil {
		fatal("failed to configure tls: %s", err)
	}

	client, err := tunnel.NewClient(&tunnel.ClientConfig{
		ServerAddr:      config.ServerAddr,
		TLSClientConfig: tlsconf,
		Backoff:         expBackoff(config.Backoff),
		Tunnels:         tunnels(config.Tunnels),
		Proxy:           proxy(config.Tunnels, logger),
		Logger:          logger,
	})
	if err != nil {
		fatal("failed to create client: %s", err)
	}

	if err := client.Start(); err != nil {
		fatal("failed to start tunnels: %s", err)
	}

	os.Exit(0)
}

func tlsConfig(config *ClientConfig) (*tls.Config, error) {
	cert, err := tls.LoadX509KeyPair(config.TLSCrt, config.TLSKey)
	if err != nil {
		return nil, err
	}

	var roots *x509.CertPool
	if config.RootCA != "" {
		roots = x509.NewCertPool()
		rootPEM, err := ioutil.ReadFile(config.RootCA)
		if err != nil {
			return nil, err
		}
		if ok := roots.AppendCertsFromPEM(rootPEM); !ok {
			return nil, err
		}
	}

	host, _, err := net.SplitHostPort(config.ServerAddr)
	if err != nil {
		return nil, err
	}

	return &tls.Config{
		ServerName:         host,
		Certificates:       []tls.Certificate{cert},
		InsecureSkipVerify: roots == nil,
		RootCAs:            roots,
	}, nil
}

func expBackoff(c BackoffConfig) *backoff.ExponentialBackOff {
	b := backoff.NewExponentialBackOff()
	b.InitialInterval = c.Interval
	b.Multiplier = c.Multiplier
	b.MaxInterval = c.MaxInterval
	b.MaxElapsedTime = c.MaxTime

	return b
}

func tunnels(m map[string]*Tunnel) map[string]*proto.Tunnel {
	p := make(map[string]*proto.Tunnel)

	for name, t := range m {
		p[name] = &proto.Tunnel{
			Protocol: t.Protocol,
			Host:     t.Host,
			Auth:     t.Auth,
			Addr:     t.RemoteAddr,
		}
	}

	return p
}

func proxy(m map[string]*Tunnel, logger log.Logger) tunnel.ProxyFunc {
	httpURL := make(map[string]*url.URL)
	tcpAddr := make(map[string]string)

	for _, t := range m {
		switch t.Protocol {
		case proto.HTTP:
			u, err := url.Parse(t.Addr)
			if err != nil {
				fatal("invalid tunnel address: %s", err)
			}
			httpURL[t.Host] = u
		case proto.TCP, proto.TCP4, proto.TCP6:
			tcpAddr[t.RemoteAddr] = t.Addr
		}
	}

	return tunnel.Proxy(tunnel.ProxyFuncs{
		HTTP: tunnel.NewMultiHTTPProxy(httpURL, log.NewContext(logger).WithPrefix("proxy", "HTTP")).Proxy,
		TCP:  tunnel.NewMultiTCPProxy(tcpAddr, log.NewContext(logger).WithPrefix("proxy", "TCP")).Proxy,
	})
}

func fatal(format string, a ...interface{}) {
	fmt.Fprintf(os.Stderr, format, a...)
	fmt.Fprint(os.Stderr, "\n")
	os.Exit(1)
}
