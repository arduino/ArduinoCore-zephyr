// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bufio"
	"debug/elf"
	"fmt"
	"os"
	"strings"

	"github.com/arduino/ArduinoCore-zephyr/tools/lib/cli"
)

func main() {
	cli.Init("zephyr-check-size")

	if len(os.Args) != 5 {
		fmt.Fprintf(os.Stderr, "Usage: %s <link_mode> <config_path> <upload_file> <elf_file>\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Computes sketch size for Arduino IDE.\n\n")
		fmt.Fprintf(os.Stderr, "Arguments:\n")
		fmt.Fprintf(os.Stderr, "  link_mode    'static' or 'dynamic'\n")
		fmt.Fprintf(os.Stderr, "  config_path  Path to Zephyr .config file\n")
		fmt.Fprintf(os.Stderr, "  upload_file  Path to upload binary (for flash size)\n")
		fmt.Fprintf(os.Stderr, "  elf_file     Path to ELF file (for section analysis)\n\n")
		cli.Fatal("expected 4 arguments, got %d", len(os.Args)-1)
	}

	linkMode := os.Args[1]
	configPath := os.Args[2]
	uploadFile := os.Args[3]
	elfFile := os.Args[4]

	// Flash size = filesystem size of the upload file
	flashSize, err := fileSize(uploadFile)
	if err != nil {
		cli.Fatal("reading upload file: %v", err)
	}

	// Heap size = sum of relevant ELF sections
	heapSize, err := computeHeapSize(elfFile, linkMode, configPath)
	if err != nil {
		cli.Fatal("computing heap size: %v", err)
	}

	fmt.Printf(".text %12d\n", flashSize)
	fmt.Printf(".data %12d\n", heapSize)
}

func fileSize(path string) (int64, error) {
	info, err := os.Stat(path)
	if err != nil {
		return 0, err
	}
	return info.Size(), nil
}

func computeHeapSize(elfFile, linkMode, configPath string) (uint64, error) {
	f, err := elf.Open(elfFile)
	if err != nil {
		return 0, err
	}
	defer f.Close()

	noReloc := false
	if linkMode == "dynamic" {
		noReloc = checkConfigFlag(configPath, "CONFIG_LLEXT_RODATA_NO_RELOC")
	}

	var total uint64
	for _, section := range f.Sections {
		if section.Flags&elf.SHF_ALLOC == 0 {
			continue
		}

		if linkMode == "static" {
			// // Static mode: only writable sections (.data, .bss) use RAM
			if section.Flags&elf.SHF_WRITE != 0 {
				total += section.Size
			}
		} else {
			// Dynamic mode: all allocated sections go to LLEXT heap.
			if noReloc && strings.HasPrefix(section.Name, ".llext.rodata.noreloc") {
				continue
			}
			total += section.Size
		}
	}

	return total, nil
}

func checkConfigFlag(configPath, key string) bool {
	f, err := os.Open(configPath)
	if err != nil {
		return false
	}
	defer f.Close()

	target := key + "=y"
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		if strings.TrimSpace(scanner.Text()) == target {
			return true
		}
	}
	return false
}
