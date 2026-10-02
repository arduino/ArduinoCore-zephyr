## MCUmgr

This directory packages the [MCUmgr] command-line tool from the Apache Mynewt
project as an Arduino tool, so the core can upload images to boards running
MCUboot.

**None of the code here is original work.** It is a copy of the upstream
MCUmgr CLI, and all rights to it belong to MCUmgr contributors.

[MCUmgr]: https://github.com/apache/mynewt-mcumgr-cli

### Building manually

To build the tool, you need to have the Go programming language installed; make
sure you have the `go` command available in your PATH. Then, use the `go build`
command to build the tool for your platform.

To build the full set of binaries for all platforms, run
`extra/package_tool.sh tools/mcumgr` from the repository root.
