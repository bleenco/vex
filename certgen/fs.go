package certgen

import (
	"fmt"
	"os"
	"path"

	"github.com/mitchellh/go-homedir"
)

func checkClientCertificate() bool {
	certPath, keyPath := getClientCertPaths()

	if fileExists(certPath) && fileExists(keyPath) {
		return true
	}

	return false
}

func getClientCertPaths() (string, string) {
	vexDir := getVexDirectory()
	if err := ensureDirectory(vexDir); err != nil {
		fmt.Fprintf(os.Stderr, "Cannot ensure vex directory (%s): %v", vexDir, err)
		os.Exit(1)
	}

	certPath := path.Join(vexDir, "client.crt")
	keyPath := path.Join(vexDir, "client.key")

	return certPath, keyPath
}

func ensureDirectory(dirPath string) error {
	if _, err := os.Stat(dirPath); os.IsNotExist(err) {
		if err := os.Mkdir(dirPath, os.ModePerm); err != nil {
			return err
		}
	}

	return nil
}

func getVexDirectory() string {
	homedir := getHomedir()
	return path.Join(homedir, ".vex")
}

func getHomedir() string {
	dir, err := homedir.Dir()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Cannot retreive users home dir: %v", err)
		os.Exit(1)
	}

	return dir
}

func fileExists(filePath string) bool {
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		return false
	}

	return true
}
