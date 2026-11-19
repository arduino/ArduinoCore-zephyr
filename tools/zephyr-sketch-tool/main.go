// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"os"

	"github.com/arduino/ArduinoCore-zephyr/tools/lib/cli"
)

func main() {
	cli.Init("zephyr-sketch-tool")

	var output = flag.String("output", "", "Output to a specific file (default: add -zsk.bin suffix)")
	var debug = flag.Bool("debug", false, "Enable debugging mode")
	var immediate = flag.Bool("immediate", false, "Start sketch immediately [UNO Q]")
	var wait_for_app = flag.Bool("wait_for_app", false, "Wait for the app to start [UNO Q]")
	var linked = flag.Bool("prelinked", false, "Provided file has already been linked to Zephyr")
	var force = flag.Bool("force", false, "Ignore safety checks and overwrite the header")
	var add_header = flag.Bool("add_header", false, "Add space for the header to the file")

	// OTA mode: produce .ota update files from a mangled sketch + loader.
	var ota = flag.Bool("ota", false, "OTA mode: produce .ota update files")
	var otaLoader = flag.String("ota-loader", "", "[ota] loader binary path")
	var otaSketch = flag.String("ota-sketch", "", "[ota] sketch binary path (the mangled -zsk.bin artifact)")
	var otaOffset = flag.String("ota-offset", "", "[ota] sketch offset in merged binary (hex)")
	var otaMagic = flag.String("ota-magic", "", "[ota] board magic number (hex, 32-bit)")

	cli.ParseFlags()

	if *ota {
		if err := runOTA(*otaLoader, *otaSketch, *otaOffset, *otaMagic, *otaSketch); err != nil {
			cli.Fatal("%v", err)
		}
		return
	}

	if flag.NArg() != 1 {
		fmt.Fprintf(os.Stderr, "Usage: %s [flags] <filename>\n", os.Args[0])
		flag.PrintDefaults()
		cli.Fatal("expected exactly one file argument")
	}
	filename := flag.Arg(0)

	// Read the file content
	content, err := os.ReadFile(filename)
	if err != nil {
		cli.Fatal("reading file: %v", err)
	}

	var ELF_HEADER = []byte { 0x7f, 0x45, 0x4c, 0x46 }
	var elf_header_found = bytes.HasPrefix(content, ELF_HEADER)
	if *add_header || (!*force && !elf_header_found) {
		fmt.Printf("File does not have an ELF header, adding empty space\n")

		var newContent = make([]byte, len(content)+16)
		copy(newContent[16:], content)
		content = newContent
	}

	if len(content) < 16 {
		cli.Fatal("%s: file too short (%d bytes)", filename, len(content))
	}

	// Create and fill custom header
	var header struct {
		ver uint8       // @ 0x07
		len uint32      // @ 0x08
		magic uint16    // @ 0x0c
		flags uint8     // @ 0x0e
	}

	header.ver = 1
	header.magic = 0x2341 // Arduino USB VID
	header.len = uint32(len(content))

	header.flags = 0
	if *debug {
		header.flags |= 0x01
	}
	if *linked {
		header.flags |= 0x02
	}
	if *immediate {
		header.flags |= 0x04
	}
	if *wait_for_app {
		header.flags |= 0x08
	}

	var bytes = make([]byte, 9)
	_, err = binary.Encode(bytes, binary.LittleEndian, header)
	if err != nil {
		cli.Fatal("encoding header: %v", err)
	}

	// Bytes 7 to 15 are free to use in current ELF specification. We will
	// use them to store the debug and linked flags.
	// Check if the target area is empty
	if !*force {
		for i := 7; i < 16; i++ {
			if content[i] != 0 {
				cli.Fatal("%s: target ELF header area is not empty, use -force to overwrite", filename)
			}
		}
	}

	// Change the header bytes in the content
	copy(content[7:16], bytes)

	// Create a new filename for the copy
	newFilename := *output
	if newFilename == "" {
		newFilename = filename + "-zsk.bin"
	}

	// Write the new content to the new file
	err = os.WriteFile(newFilename, content, 0644)
	if err != nil {
		cli.Fatal("writing to file: %v", err)
	}

	fmt.Printf("File %s saved as %s\n", filename, newFilename)
}
