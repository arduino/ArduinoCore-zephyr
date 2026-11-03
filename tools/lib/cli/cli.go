// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

// Package cli provides command line helpers shared by the core tools.
//
// Errors are reported on stderr as "<tool>: error: <msg>", a format that is
// recognized by CI log parsers looking for compiler-style diagnostics.
package cli

import (
	"flag"
	"fmt"
	"io"
	"os"
)

var toolName = "tool"

// Init sets the tool name used as prefix for error messages.
func Init(name string) {
	toolName = name
}

// Fatal prints an error message in a format recognized by CI tools and exits.
func Fatal(format string, args ...any) {
	fmt.Fprintf(os.Stderr, "%s: error: %s\n", toolName, fmt.Sprintf(format, args...))
	os.Exit(1)
}

// ParseFlags parses the command line, reporting invalid flags via Fatal().
func ParseFlags() {
	flag.CommandLine.Init(toolName, flag.ContinueOnError)
	flag.CommandLine.SetOutput(io.Discard)
	err := flag.CommandLine.Parse(os.Args[1:])
	flag.CommandLine.SetOutput(os.Stderr)
	if err == flag.ErrHelp {
		flag.Usage()
		os.Exit(0)
	} else if err != nil {
		Fatal("%v", err)
	}
}
