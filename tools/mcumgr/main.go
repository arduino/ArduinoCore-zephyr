/*
 * Copyright (c) 2026 RAKwireless Technology Limited
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * This file mirrors mcumgr/mcumgr.go from github.com/apache/mynewt-mcumgr-cli
 * (Apache-2.0) on top of the same newtmgr libraries.
 */

package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"mynewt.apache.org/newt/util"
	"mynewt.apache.org/newtmgr/newtmgr/cli"
	"mynewt.apache.org/newtmgr/newtmgr/config"
	"mynewt.apache.org/newtmgr/newtmgr/nmutil"
	"mynewt.apache.org/newtmgr/nmxact/nmserial"
)

func stopXport() {
	x, err := cli.GetXportIfOpen()
	if err == nil {
		// Don't attempt to close a serial transport.  Attempting to close
		// the serial port while a read is in progress (in MacOS) just
		// blocks until the read completes.  Instead, let the OS close the
		// port on termination.
		if _, ok := x.(*nmserial.SerialXport); !ok {
			x.Stop()
		}
	}
}

func closeSesn() {
	s, err := cli.GetSesnIfOpen()
	if err == nil {
		s.Close()
	}
}

func cleanup() {
	closeSesn()
	stopXport()
}

func main() {
	nmutil.ToolInfo = nmutil.ToolInfoType{
		ExeName:       "mcumgr",
		ShortName:     "mcumgr",
		LongName:      "mcumgr",
		VersionString: "0.0.0-dev",
		CfgFilename:   ".mcumgr.cp.json",
	}

	if err := config.InitGlobalConnProfileMgr(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s\n", err.Error())
		os.Exit(1)
	}

	defer cleanup()
	cli.SetOnExit(cleanup)

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan)

	go func() {
		for {
			s := <-sigChan
			switch s {
			case os.Interrupt, syscall.SIGTERM:
				cli.SilenceErrors()
				cli.NmExit(1)

			case syscall.SIGQUIT:
				util.PrintStacks()
			}
		}
	}()

	cli.Commands().Execute()
}
