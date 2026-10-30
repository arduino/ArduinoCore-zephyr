// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"debug/elf"
	"flag"
	"fmt"
	"os"
	"sort"
	"strings"

	"github.com/arduino/ArduinoCore-zephyr/tools/lib/cli"
)

func main() {
	cli.Init("gen-rodata-ld")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s <input.elf> [output_script.ld] [link_mode]\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Analyzes an ELF file and generates a linker script fragment that\n")
		fmt.Fprintf(os.Stderr, "separates .rodata sections into:\n")
		fmt.Fprintf(os.Stderr, "  .rodata          - sections WITH relocations (copied to RAM)\n")
		fmt.Fprintf(os.Stderr, "  .rodata.noreloc  - sections WITHOUT relocations (kept in flash)\n")
		fmt.Fprintf(os.Stderr, "\nIf link_mode is 'static', generates an empty linker script.\n")
	}

	cli.ParseFlags()
	if flag.NArg() < 1 {
		flag.Usage()
		cli.Fatal("missing input file")
	}

	inputFile := flag.Arg(0)
	outputFile := "rodata_split.ld"
	if flag.NArg() >= 2 {
		outputFile = flag.Arg(1)
	}
	linkMode := "dynamic"
	if flag.NArg() >= 3 {
		linkMode = flag.Arg(2)
	}

	fmt.Printf("Generate rodata linker script (mode: %s)\n", linkMode)

	// For static linking, generate empty linker script
	if linkMode == "static" {
		out, err := os.Create(outputFile)
		if err != nil {
			cli.Fatal("%v", err)
		}
		defer out.Close()

		fmt.Fprintf(out, "/* Empty linker script for static linking mode */\n")
		fmt.Printf("Generated: %s\n", outputFile)
		return
	}

	f, err := elf.Open(inputFile)
	if err != nil {
		cli.Fatal("%v", err)
	}
	defer f.Close()

	// First pass: find all .rodata sections, indexed by section number
	type RodataSection struct {
		Name      string
		HasRelocs bool
	}

	rodataSections := make(map[uint32]*RodataSection)

	for i, section := range f.Sections {
		if strings.HasPrefix(section.Name, ".rodata") {
			rodataSections[uint32(i)] = &RodataSection{Name: section.Name}
		}
	}

	// Second pass: mark which rodata sections have relocations
	for _, section := range f.Sections {
		if section.Type == elf.SHT_REL || section.Type == elf.SHT_RELA {
			if rodata, exists := rodataSections[section.Info]; exists {
				rodata.HasRelocs = true
			}
		}
	}

	// Separate and sort
	withRelocs := []string{}
	withoutRelocs := []string{}

	for _, rodata := range rodataSections {
		if rodata.HasRelocs {
			withRelocs = append(withRelocs, rodata.Name)
		} else {
			withoutRelocs = append(withoutRelocs, rodata.Name)
		}
	}

	sort.Strings(withRelocs)
	sort.Strings(withoutRelocs)

	// Generate linker script
	out, err := os.Create(outputFile)
	if err != nil {
		cli.Fatal("%v", err)
	}
	defer out.Close()

	// Helper function to print a section
	printSection := func(out *os.File, sectionName string, sections []string) {
		fmt.Fprintf(out, "    %s : {\n", sectionName)
		for _, name := range sections {
			fmt.Fprintf(out, "        *(%s)\n", name)
		}
		fmt.Fprintf(out, "    }\n\n")
	}

	fmt.Fprintf(out, "/* Auto-generated linker script fragment for LLEXT\n")
	fmt.Fprintf(out, " * Separates .rodata sections based on relocation status\n")
	fmt.Fprintf(out, " */\n\n")
	fmt.Fprintf(out, "SECTIONS\n{\n")

	fmt.Fprintf(out, "    /* Read-only data WITH relocations - will be copied to RAM by LLEXT */\n")
	printSection(out, ".rodata", withRelocs)

	fmt.Fprintf(out, "    /* Read-only data WITHOUT relocations - kept in flash by LLEXT */\n")
	printSection(out, ".llext.rodata.noreloc", withoutRelocs)

	fmt.Fprintf(out, "    /* Merge all .rel.rodata.* sections into .rel.rodata */\n")
	printSection(out, ".rel.rodata", []string{".rel.rodata", ".rel.rodata.*"})

	fmt.Fprintf(out, "    /* Merge all .rela.rodata.* sections into .rela.rodata */\n")
	printSection(out, ".rela.rodata", []string{".rela.rodata", ".rela.rodata.*"})

	fmt.Fprintf(out, "}\n")

	fmt.Printf("Generated: %s\n", outputFile)
}
