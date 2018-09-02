package logger

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/fatih/color"
)

type logWriter struct{}

func (w *logWriter) Write(bytes []byte) (int, error) {
	timeColor := color.New(color.FgMagenta, color.BgBlack).SprintFunc()
	timeString := timeColor("[" + time.Now().Format("2006-01-02 15:04:05") + "]")
	return fmt.Print(timeString + " " + string(bytes))
}

// Logger defines default logger
type Logger struct {
	prefix      string
	logger      *log.Logger
	Info, Debug bool
}

// NewLogger creates and returns new default logger
func NewLogger(debug bool) *Logger {
	return newLoggerFlag(log.Ldate|log.Ltime, debug)
}

// NewLoggerWithPrefix returns new logger with prefix
func NewLoggerWithPrefix(prefix string, debug bool) *Logger {
	logger := log.New(os.Stdout, "", 0)
	logger.SetOutput(new(logWriter))

	return &Logger{
		prefix: prefix + ": ",
		logger: logger,
		Info:   true,
		Debug:  debug,
	}
}

func newLoggerFlag(flag int, debug bool) *Logger {
	logger := log.New(os.Stdout, "", 0)
	logger.SetOutput(new(logWriter))

	return &Logger{
		prefix: "",
		logger: logger,
		Info:   true,
		Debug:  debug,
	}
}

// Infof prints info type message
func (l *Logger) Infof(f string, args ...interface{}) {
	if l.Info {
		info := color.New(color.FgCyan, color.BgBlack).SprintFunc()
		prefix := info("info")
		l.logger.Printf(l.prefix+prefix+": "+f, args...)
	}
}

// Debugf prints debug type message
func (l *Logger) Debugf(f string, args ...interface{}) {
	if l.Debug {
		debug := color.New(color.FgYellow, color.BgBlack).SprintFunc()
		prefix := debug("debug")
		l.logger.Printf(l.prefix+prefix+": "+f, args...)
	}
}

// Errorf prints error type message
func (l *Logger) Errorf(f string, args ...interface{}) {
	err := color.New(color.FgRed, color.BgBlack).SprintFunc()
	prefix := err("error")
	l.logger.Printf(l.prefix+prefix+": "+f, args...)
}
