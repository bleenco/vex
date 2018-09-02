package certgen

import (
	"crypto/ecdsa"
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"net"
	"os"
	"time"
)

// CheckAndGenerateCert checks if clients certificate exists and if not
// generates a new self-signed X.509 certificate for a TLS connections.
func CheckAndGenerateCert() (string, string) {
	certPath, keyPath := getClientCertPaths()
	if !checkClientCertificate() {
		generateCertAndKey(certPath, keyPath)
	}

	return certPath, keyPath
}

func generateCertAndKey(certPath, keyPath string) {
	host, err := os.Hostname()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Cannot get computers hostname: %v", err)
		os.Exit(1)
	}
	validFor := time.Duration(356 * 24 * time.Hour)
	isCa := false
	rsaBits := 2048

	priv, err := rsa.GenerateKey(rand.Reader, rsaBits)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to generate private key: %v", err)
		os.Exit(1)
	}

	serialNumberLimit := new(big.Int).Lsh(big.NewInt(1), 128)
	serialNumber, err := rand.Int(rand.Reader, serialNumberLimit)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to generate serial number: %v", err)
		os.Exit(1)
	}

	notBefore := time.Now()
	notAfter := notBefore.Add(validFor)

	template := x509.Certificate{
		SerialNumber: serialNumber,
		Subject: pkix.Name{
			Organization: []string{"Acme Co"},
		},
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth},
		BasicConstraintsValid: true,
	}

	if ip := net.ParseIP(host); ip != nil {
		template.IPAddresses = append(template.IPAddresses, ip)
	} else {
		template.DNSNames = append(template.DNSNames, host)
	}

	if isCa {
		template.IsCA = true
		template.KeyUsage |= x509.KeyUsageCertSign
	}

	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, publicKey(priv), priv)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to create certificate: %v", err)
		os.Exit(1)
	}

	certOut, err := os.Create(certPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to open %s for writing: %v", certPath, err)
		os.Exit(1)
	}
	if err := pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: derBytes}); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to write data to %s: %v", certPath, err)
		os.Exit(1)
	}
	if err := certOut.Close(); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to close %s: %v", certPath, err)
		os.Exit(1)
	}

	keyOut, err := os.OpenFile(keyPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to open %s for writing: %v", keyPath, err)
		os.Exit(1)
	}
	if err := pem.Encode(keyOut, pemBlockForKey(priv)); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to write data to %s: %v", keyPath, err)
		os.Exit(1)
	}
	if err := keyOut.Close(); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to close %s: %v", keyPath, err)
		os.Exit(1)
	}
}

func publicKey(priv interface{}) interface{} {
	switch k := priv.(type) {
	case *rsa.PrivateKey:
		return &k.PublicKey
	case *ecdsa.PrivateKey:
		return &k.PublicKey
	default:
		return nil
	}
}

func pemBlockForKey(priv interface{}) *pem.Block {
	switch k := priv.(type) {
	case *rsa.PrivateKey:
		return &pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(k)}
	case *ecdsa.PrivateKey:
		b, err := x509.MarshalECPrivateKey(k)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Unable to marshal ECDSA private key: %v", err)
			os.Exit(1)
		}
		return &pem.Block{Type: "EC PRIVATE KEY", Bytes: b}
	default:
		return nil
	}
}
