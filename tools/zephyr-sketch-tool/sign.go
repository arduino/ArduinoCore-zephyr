// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"encoding/binary"
	"encoding/pem"
	"fmt"
	"os"
	"strconv"
	"strings"
)

func readSigningKey(pemFile string) (*ecdsa.PrivateKey, error) {
	keyData, err := os.ReadFile(pemFile)
	if err != nil {
		return nil, fmt.Errorf("read signing key: %w", err)
	}
	block, _ := pem.Decode(keyData)
	if block == nil {
		return nil, fmt.Errorf("signing key is not PEM encoded")
	}
	key, err := x509.ParseECPrivateKey(block.Bytes)
	if err != nil {
		return nil, fmt.Errorf("parse signing key: %w", err)
	}
	return key, nil
}

// signMCUbootImage fills the reserved MCUboot header and appends signature TLVs.
// Bytes after the standard 32-byte header are preserved, including any sketch metadata.
// Input policy (key curve, version, payload and slot limits) belongs to the caller.
func signMCUbootImage(image []byte, key *ecdsa.PrivateKey, headerSize uint16, version ImageVersion) ([]byte, error) {
	if headerSize < 32 || len(image) < 32 {
		return nil, fmt.Errorf("image must reserve at least 32 bytes for the MCUboot header")
	}

	header := ImageHeader{
		Magic: ImageMagic, HdrSize: headerSize,
		ImageSize: uint32(len(image) - int(headerSize)), Version: version,
	}
	var headerBuf bytes.Buffer
	if err := binary.Write(&headerBuf, binary.LittleEndian, header); err != nil {
		return nil, err
	}
	copy(image[:32], headerBuf.Bytes())
	digest := sha256.Sum256(image)
	publicKey, err := x509.MarshalPKIXPublicKey(&key.PublicKey)
	if err != nil {
		return nil, err
	}
	keyHash := sha256.Sum256(publicKey)
	signature, err := ecdsa.SignASN1(rand.Reader, key, digest[:])
	if err != nil {
		return nil, err
	}
	var output bytes.Buffer
	output.Write(image)
	appendTLVs(&output, keyHash[:], digest[:], signature)
	return output.Bytes(), nil
}

// Accept both Arduino's major.minor.revision.build and Zephyr's major.minor.revision+build.
func signingVersion(value string) (ImageVersion, error) {
	if strings.Contains(value, "+") {
		parts := strings.Split(value, "+")
		if len(parts) != 2 || strings.Count(parts[0], ".") != 2 || strings.Contains(parts[1], ".") {
			return ImageVersion{}, fmt.Errorf("invalid image version %q", value)
		}
	}
	parts := strings.Split(strings.Replace(value, "+", ".", 1), ".")
	if len(parts) != 3 && len(parts) != 4 {
		return ImageVersion{}, fmt.Errorf("invalid image version %q", value)
	}
	var fields [4]uint64
	for i, part := range parts {
		bits := []int{8, 8, 16, 32}[i]
		v, err := strconv.ParseUint(part, 10, bits)
		if err != nil {
			return ImageVersion{}, fmt.Errorf("invalid image version %q: %w", value, err)
		}
		fields[i] = v
	}
	return ImageVersion{uint8(fields[0]), uint8(fields[1]), uint16(fields[2]), uint32(fields[3])}, nil
}

func signStandalone(input, output, pemFile, versionString string, headerSize, slotSize uint) error {
	if headerSize < 32 || headerSize > 0xffff || slotSize == 0 {
		return fmt.Errorf("--sign requires --header_size (32..65535) and a positive --slot_size")
	}
	version, err := signingVersion(versionString)
	if err != nil {
		return err
	}
	image, err := os.ReadFile(input)
	if err != nil {
		return err
	}
	if uint64(len(image)) > uint64(slotSize) {
		return fmt.Errorf("image exceeds slot size %d", slotSize)
	}
	if len(image) <= int(headerSize) {
		return fmt.Errorf("image must contain a reserved MCUboot header and a payload")
	}
	key, err := readSigningKey(pemFile)
	if err != nil {
		return err
	}
	if key.Curve != elliptic.P256() {
		return fmt.Errorf("MCUboot signing requires an ECDSA P-256 key")
	}
	signed, err := signMCUbootImage(image, key, uint16(headerSize), version)
	if err != nil {
		return err
	}
	if uint64(len(signed)) > uint64(slotSize) {
		return fmt.Errorf("signed image exceeds slot size %d", slotSize)
	}
	return os.WriteFile(output, signed, 0644)
}
