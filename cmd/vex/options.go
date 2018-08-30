// Copyright (C) 2017 Michał Matczuk
// Use of this source code is governed by an AGPL-style
// license that can be found in the LICENSE file.

package main

import (
	"flag"
	"fmt"
	"os"
)

const usage1 string = `Usage: vex [OPTIONS] <command> [command args] [...]
options:
`

const usage2 string = `
Commands:
	vex id                      Show client identifier
	vex list                    List vex names from config file
	vex start [vex] [...]    Start vexs by name from config file
	vex start-all               Start all vexs defined in config file

Examples:
	vex start www ssh
	vex -config config.yaml -log-level 2 start ssh
	vex start-all

config.yaml:
	server_addr: SERVER_IP:5223
	vexs:
	  webui:
	    proto: http
	    addr: localhost:8080
	    auth: user:password
	    host: webui.my-vex-host.com
	  ssh:
	    proto: tcp
	    addr: 192.168.0.5:22
	    remote_addr: 0.0.0.0:22
`

func init() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, usage1)
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, usage2)
	}
}

type options struct {
	config   string
	logLevel int
	version  bool
	command  string
	args     []string
}

func parseArgs() (*options, error) {
	config := flag.String("config", "vex.yml", "Path to vex configuration file")
	logLevel := flag.Int("log-level", 1, "Level of messages to log, 0-3")
	version := flag.Bool("version", false, "Prints vex version")
	flag.Parse()

	opts := &options{
		config:   *config,
		logLevel: *logLevel,
		version:  *version,
		command:  flag.Arg(0),
	}

	if opts.version {
		return opts, nil
	}

	switch opts.command {
	case "":
		flag.Usage()
		os.Exit(2)
	case "id", "list":
		opts.args = flag.Args()[1:]
		if len(opts.args) > 0 {
			return nil, fmt.Errorf("list takes no arguments")
		}
	case "start":
		opts.args = flag.Args()[1:]
		if len(opts.args) == 0 {
			return nil, fmt.Errorf("you must specify at least one vex to start")
		}
	case "start-all":
		opts.args = flag.Args()[1:]
		if len(opts.args) > 0 {
			return nil, fmt.Errorf("start-all takes no arguments")
		}
	default:
		return nil, fmt.Errorf("unknown command %q", opts.command)
	}

	return opts, nil
}
