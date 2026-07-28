// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

// OTA-mode file generation. A single runOTA() call produces four artifacts
// derived from the same inputs, sharing the OTA header
// (len + crc32 + magic + version + payload):
//
//	<output> .ota             — sketch only, uncompressed
//	<output> .lzss.ota        — sketch only, LZSS-compressed
//	<output> -bundle.ota      — loader + 0xFF padding + sketch, uncompressed
//	<output> -bundle.lzss.ota — loader + 0xFF padding + sketch, LZSS-compressed

package main

import (
	"encoding/binary"
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

func runOTA(loaderPath, sketchPath, offsetStr, magicStr, outputBase string) error {
	// The hook runs unconditionally for every board, but only OTA-enabled
	// boards set build.ota.magic. An empty magic means "not configured" —
	// silently skip so the uniform recipe is safe for non-OTA boards.
	if magicStr == "" {
		return nil
	}
	if loaderPath == "" || sketchPath == "" || offsetStr == "" {
		return fmt.Errorf("ota: -ota-loader, -ota-sketch and -ota-offset are all required when -ota-magic is set")
	}

	magic, err := parseHex(magicStr)
	if err != nil {
		return fmt.Errorf("parse magic: %w", err)
	}

	offset, err := parseHex(offsetStr)
	if err != nil {
		return fmt.Errorf("parse offset: %w", err)
	}

	loader, err := os.ReadFile(loaderPath)
	if err != nil {
		return fmt.Errorf("read loader: %w", err)
	}

	sketch, err := os.ReadFile(sketchPath)
	if err != nil {
		return fmt.Errorf("read sketch: %w", err)
	}

	if int64(len(loader)) > offset {
		return fmt.Errorf("loader (%d bytes) exceeds offset (0x%X)", len(loader), offset)
	}

	merged := append(padFF(loader, int(offset)), sketch...)

	// Tolerate callers passing either "<base>" or "<base>.ota" — strip the
	// trailing ".ota" so the four derivatives don't end up as "<base>.ota.ota".
	base := strings.TrimSuffix(outputBase, ".ota")

	fmt.Printf("Inputs: loader %d bytes, sketch %d bytes, merged %d bytes\n",
		len(loader), len(sketch), len(merged))

	variants := []struct {
		suffix   string
		payload  []byte
		compress bool
		header   bool
		label    string
	}{
		{".ota", sketch, false, true, "sketch"},
		{".lzss.ota", sketch, true, true, "sketch (LZSS)"},
		{"-bundle.bin", merged, false, false, "loader+sketch"},
		{"-bundle.ota", merged, false, true, "loader+sketch"},
		{"-bundle.lzss.ota", merged, true, true, "loader+sketch (LZSS)"},
	}

	for _, v := range variants {
		path := base + v.suffix

		out := v.payload
		if v.header {
			out = buildOTA(uint32(magic), v.payload, v.compress)
		}

		if err := os.WriteFile(path, out, 0644); err != nil {
			return fmt.Errorf("write %s: %w", path, err)
		}
		fmt.Printf("OTA: %s (%d bytes, %s)\n", path, len(out), v.label)
	}
	return nil
}

// buildOTA wraps the payload (optionally LZSS-encoded) in the OTA file
// envelope: len(4) | crc32(4) | magic(4) | version(8) | payload.
func buildOTA(magic uint32, raw []byte, compress bool) []byte {
	payload := raw
	if compress {
		payload = lzssEncode(raw)
	}

	var version [8]byte
	version[0] = 1
	if compress {
		version[0] |= 0x40
	}

	body := make([]byte, 0, 4+8+len(payload))
	body = binary.LittleEndian.AppendUint32(body, magic)
	body = append(body, version[:]...)
	body = append(body, payload...)

	crcVal := crc32.ChecksumIEEE(body)

	out := make([]byte, 0, 8+len(body))
	out = binary.LittleEndian.AppendUint32(out, uint32(len(body)))
	out = binary.LittleEndian.AppendUint32(out, crcVal)
	out = append(out, body...)
	return out
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
