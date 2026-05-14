// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

// ota-pack creates OTA update files for Arduino boards.
//
// Usage:
//
//	ota-pack [flags]
package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"hash/crc32"
	"os"
	"strconv"
	"strings"
)

// LZSS parameters matching the Arduino OTA decoder
const (
	lzssEI   = 11
	lzssEJ   = 4
	lzssN    = 1 << lzssEI       // ring buffer size = 2048
	lzssF    = (1 << lzssEJ) + 1 // max match length = 17
	lzssMask = lzssN - 1
)

func main() {
	loaderPath := flag.String("loader", "", "loader binary path")
	sketchPath := flag.String("sketch", "", "sketch binary path")
	offsetStr := flag.String("offset", "", "sketch offset in merged binary (hex)")
	outputPath := flag.String("output", "", "output .ota file path")
	magicStr := flag.String("magic", "", "board magic number (hex, required)")
	compress := flag.Bool("compress", false, "enable LZSS compression")
	sketchOnly := flag.Bool("sketch-only", false, "wrap raw sketch in OTA header (no loader merge)")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [--sketch-only] -sketch <file> -output <file> [options]\n\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Creates an OTA update file with header and optional LZSS compression.\n\n")
		flag.PrintDefaults()
	}

	flag.Parse()

	if *magicStr == "" {
		fatalf("-magic is required")
	}
	magic, err := parseHex(*magicStr)
	fatal(err, "parse magic")

	var payloadInput []byte

	if *sketchOnly {
		if *loaderPath != "" || *offsetStr != "" {
			fatalf("--sketch-only: -loader and -offset are not allowed")
		}
		if *sketchPath == "" || *outputPath == "" {
			flag.Usage()
			os.Exit(1)
		}

		payloadInput, err = os.ReadFile(*sketchPath)
		fatal(err, "read sketch")

		fmt.Printf("Sketch-only mode: %d bytes\n", len(payloadInput))
	} else {
		if *loaderPath == "" || *sketchPath == "" || *offsetStr == "" || *outputPath == "" {
			flag.Usage()
			os.Exit(1)
		}

		offset, err := parseHex(*offsetStr)
		fatal(err, "parse offset")

		loader, err := os.ReadFile(*loaderPath)
		fatal(err, "read loader")

		sketch, err := os.ReadFile(*sketchPath)
		fatal(err, "read sketch")

		if int64(len(loader)) > offset {
			fatalf("loader (%d bytes) exceeds offset (0x%X)", len(loader), offset)
		}

		// Merge: loader + 0xFF padding + sketch at offset
		payloadInput = append(padFF(loader, int(offset)), sketch...)
		fmt.Printf("Merged: %d bytes (loader %d + sketch %d)\n", len(payloadInput), len(loader), len(sketch))
	}

	// Compress
	payload := payloadInput
	if *compress {
		payload = lzssEncode(payloadInput)
		fmt.Printf("LZSS: %d -> %d bytes (%.1f%%)\n",
			len(payloadInput), len(payload),
			100.0*float64(len(payload))/float64(len(payloadInput)))
	}

	var version [8]byte
	version[0] = 1
	if *compress {
		version[0] |= 0x40
	}

	// Assemble body: magic(4) + version(8) + payload
	// CRC-32 covers this entire body
	body := make([]byte, 0, 4+8+len(payload))
	body = binary.LittleEndian.AppendUint32(body, uint32(magic))
	body = append(body, version[:]...)
	body = append(body, payload...)

	crcVal := crc32.ChecksumIEEE(body)

	// Final file: len(4) + crc32(4) + body
	out := make([]byte, 0, 8+len(body))
	out = binary.LittleEndian.AppendUint32(out, uint32(len(body)))
	out = binary.LittleEndian.AppendUint32(out, crcVal)
	out = append(out, body...)

	err = os.WriteFile(*outputPath, out, 0644)
	fatal(err, "write output")

	fmt.Printf("OTA: %s (%d bytes, magic 0x%08X)\n", *outputPath, len(out), magic)
}

// padFF pads data with 0xFF up to size.
func padFF(data []byte, size int) []byte {
	buf := make([]byte, size)
	copy(buf, data)
	for i := len(data); i < size; i++ {
		buf[i] = 0xFF
	}
	return buf
}

// --- LZSS Encoder ---

type bitWriter struct {
	data  []byte
	accum uint32
	nbits int
}

func (w *bitWriter) write(value, bits int) {
	w.accum = (w.accum << uint(bits)) | uint32(value&((1<<uint(bits))-1))
	w.nbits += bits
	for w.nbits >= 8 {
		w.nbits -= 8
		w.data = append(w.data, byte(w.accum>>uint(w.nbits)))
		w.accum &= (1 << uint(w.nbits)) - 1
	}
}

func (w *bitWriter) flush() {
	if w.nbits > 0 {
		w.data = append(w.data, byte(w.accum<<uint(8-w.nbits)))
		w.nbits = 0
		w.accum = 0
	}
}

func lzssEncode(input []byte) []byte {
	var ring [lzssN]byte
	for i := range ring {
		ring[i] = ' '
	}

	w := &bitWriter{}
	r := lzssN - lzssF
	pos := 0

	for pos < len(input) {
		bestLen := 1
		bestPos := 0

		maxLen := lzssF
		if rem := len(input) - pos; rem < maxLen {
			maxLen = rem
		}

		for i := 0; i < lzssN; i++ {
			if ring[i] != input[pos] {
				continue
			}
			safeDist := (r - i) & lzssMask
			if safeDist == 0 {
				safeDist = lzssN
			}
			safeLen := maxLen
			if safeDist < safeLen {
				safeLen = safeDist
			}
			matchLen := 1
			for matchLen < safeLen && ring[(i+matchLen)&lzssMask] == input[pos+matchLen] {
				matchLen++
			}
			if matchLen > bestLen {
				bestLen = matchLen
				bestPos = i
			}
		}

		if bestLen >= 2 {
			w.write(0, 1)
			w.write(bestPos, lzssEI)
			w.write(bestLen-2, lzssEJ)
		} else {
			bestLen = 1
			w.write(1, 1)
			w.write(int(input[pos]), 8)
		}

		for k := 0; k < bestLen; k++ {
			ring[r] = input[pos+k]
			r = (r + 1) & lzssMask
		}
		pos += bestLen
	}

	w.flush()
	return w.data
}

func parseHex(s string) (int64, error) {
	s = strings.TrimSpace(s)
	if strings.HasPrefix(s, "0x") || strings.HasPrefix(s, "0X") {
		return strconv.ParseInt(s[2:], 16, 64)
	}
	return strconv.ParseInt(s, 0, 64)
}

func fatal(err error, context string) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s: %v\n", context, err)
		os.Exit(1)
	}
}

func fatalf(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "Error: "+format+"\n", args...)
	os.Exit(1)
}
