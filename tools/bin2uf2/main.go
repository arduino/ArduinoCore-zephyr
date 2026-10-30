// Copyright (c) Arduino s.r.l. and/or its affiliated companies
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"unsafe"

	"github.com/arduino/ArduinoCore-zephyr/tools/lib/cli"
)

// UF2 block constants with fixed-size types.
const (
	magic1          uint32 = 0x0A324655
	magic2          uint32 = 0x9E5D5157
	magic3          uint32 = 0x0AB16F30
	flags           uint32 = 0x00002000 // familyID present
	payloadSize     uint32 = 256
	blockSize       uint32 = 512
	dataSectionSize uint32 = 476
)

// UF2Block defines the structure of a UF2 block, used as a data container.
// The Payload array is sized to hold the entire data section, so the unused
// portion of the array acts as our padding.
type UF2Block struct {
	Magic1      uint32
	Magic2      uint32
	Flags       uint32
	TargetAddr  uint32
	PayloadSize uint32
	BlockNo     uint32
	NumBlocks   uint32
	FamilyID    uint32
	Payload     [dataSectionSize]byte
	Magic3      uint32
}

// Calculate the offset of the NumBlocks field within the block struct.
const numBlocksOffset = unsafe.Offsetof(UF2Block{}.NumBlocks)

func main() {
	cli.Init("bin2uf2")

	// Customize the default usage message to be more explicit.
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s <addr> <familyID> <source file> <destination file>\n", os.Args[0])
		fmt.Fprintln(os.Stderr, "Converts a binary file to the UF2 format.")
		fmt.Fprintln(os.Stderr, "\nArguments:")
		fmt.Fprintln(os.Stderr, "  addr         Starting memory address in hexadecimal format (e.g. 0x100E0000)")
		fmt.Fprintln(os.Stderr, "  familyID     Family ID of the target device in hexadecimal format (e.g. 0xe48bff56)")
		fmt.Fprintln(os.Stderr, "  source file  Input binary file")
		fmt.Fprintln(os.Stderr, "  dest file    Output UF2 file")
	}

	cli.ParseFlags()

	// Check for the correct number of positional arguments.
	if len(flag.Args()) != 4 {
		flag.Usage()
		cli.Fatal("expected 4 arguments, got %d", flag.NArg())
	}

	// Parse the address from the first positional argument.
	parsedAddr, err := strconv.ParseUint(strings.TrimPrefix(flag.Arg(0), "0x"), 16, 32)
	if err != nil {
		cli.Fatal("invalid address format: %v", err)
	}
	address := uint32(parsedAddr)

	// Parse the familyID from the second positional argument.
	parsedFamilyID, err := strconv.ParseUint(strings.TrimPrefix(flag.Arg(1), "0x"), 16, 32)
	if err != nil {
		cli.Fatal("invalid familyID format: %v", err)
	}
	familyID := uint32(parsedFamilyID)

	srcPath := flag.Arg(2)
	dstPath := flag.Arg(3)

	// Open source file
	src, err := os.Open(srcPath)
	if err != nil {
		cli.Fatal("could not open source file %s: %v", srcPath, err)
	}
	defer src.Close()

	// Create destination file
	dst, err := os.Create(dstPath)
	if err != nil {
		cli.Fatal("could not create destination file %s: %v", dstPath, err)
	}
	defer dst.Close()

	var blockNum uint32
	var totalBlocks uint32
	// This slice is a temporary buffer for reading one payload-worth of data.
	readBuffer := make([]byte, payloadSize)

	// Main loop to read source and write UF2 blocks
	for {
		bytesRead, err := io.ReadFull(src, readBuffer)
		if err == io.EOF {
			break
		}
		if err != nil && err != io.ErrUnexpectedEOF {
			cli.Fatal("failed reading from source file %s: %v", srcPath, err)
		}

		// Zero out the unused portion of the buffer for partial reads
		for i := bytesRead; i < int(payloadSize); i++ {
			readBuffer[i] = 0
		}

		// Create the block struct and populate its fields.
		block := UF2Block{
			Magic1:      magic1,
			Magic2:      magic2,
			Flags:       flags,
			TargetAddr:  address,
			PayloadSize: payloadSize,
			BlockNo:     blockNum,
			NumBlocks:   0, // Placeholder, will be updated later.
			FamilyID:    familyID,
			Magic3:      magic3,
		}
		// Copy the data from our read buffer into the beginning of the
		// larger Payload array. The rest of the array remains zero, acting as padding.
		copy(block.Payload[:], readBuffer)

		// --- Write the block to disk piece-by-piece ---
		// 1. Write the header fields
		binary.Write(dst, binary.LittleEndian, block.Magic1)
		binary.Write(dst, binary.LittleEndian, block.Magic2)
		binary.Write(dst, binary.LittleEndian, block.Flags)
		binary.Write(dst, binary.LittleEndian, block.TargetAddr)
		binary.Write(dst, binary.LittleEndian, block.PayloadSize)
		binary.Write(dst, binary.LittleEndian, block.BlockNo)
		binary.Write(dst, binary.LittleEndian, block.NumBlocks)
		binary.Write(dst, binary.LittleEndian, block.FamilyID)

		// 2. Write the entire 476-byte data section (payload + padding) in one go.
		if _, err := dst.Write(block.Payload[:]); err != nil {
			cli.Fatal("failed writing data section to %s: %v", dstPath, err)
		}

		// 3. Write the final magic number
		if err := binary.Write(dst, binary.LittleEndian, block.Magic3); err != nil {
			cli.Fatal("failed writing final magic to %s: %v", dstPath, err)
		}

		address += payloadSize
		blockNum++

		if err == io.EOF || bytesRead < int(payloadSize) {
			break
		}
	}

	totalBlocks = blockNum

	// After writing all blocks, seek back and update the totalBlocks field in each header
	for i := uint32(0); i < totalBlocks; i++ {
		// Calculate the offset using our safe constant instead of a magic number.
		offset := int64(i)*int64(blockSize) + int64(numBlocksOffset)
		_, err := dst.Seek(offset, io.SeekStart)
		if err != nil {
			cli.Fatal("failed seeking in destination file %s: %v", dstPath, err)
		}
		if err := binary.Write(dst, binary.LittleEndian, totalBlocks); err != nil {
			cli.Fatal("failed updating total blocks in %s: %v", dstPath, err)
		}
	}

	fmt.Printf("Successfully converted %s to %s (%d blocks written).\n", srcPath, dstPath, totalBlocks)
}
