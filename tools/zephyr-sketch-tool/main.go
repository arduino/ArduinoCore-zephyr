// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bufio"
	"bytes"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/binary"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

// MCUboot Header & Configuration Constants
const (
	ImageMagic   uint32 = 0x96f3b83d
	TlvInfoMagic uint16 = 0x6907
	TlvKeyHash   uint8  = 0x01
	TlvSha256    uint8  = 0x10
	TlvRsa2048   uint8  = 0x20
	HeaderSize   uint16 = 0x400
	SlotSize     int    = 1540096
)

type ImageVersion struct {
	Major    uint8
	Minor    uint8
	Revision uint16
	BuildNum uint32
}

type ImageHeader struct {
	Magic     uint32
	LoadAddr  uint32
	HdrSize   uint16
	PTLVSize  uint16
	ImageSize uint32
	Flags     uint32
	Version   ImageVersion
	Pad1      uint32
}

const (
	ConfigPaddingEraseDim = "FLASH_ALIGN_BASED_ON_DT_ERASE_DIM"
)

func main() {
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

	// Squash / Sign flags
	var squash = flag.Bool("squash", false, "Squash the loader and sketch, then sign the image")
	var loader_bin = flag.String("loader_bin", "", "Path to the binary loader to be used")
	var pem_file = flag.String("pem_file", "", "Path to the PEM file containing keys")
	var erase_flash_dim = flag.Uint("erase_flash_dim", 0, "Minimum erasing flash sector dimension in bytes")
	var image_version = flag.String("image_version", "1.0.0.0", "Image version for the header (default 1.0.0.0)")

	flag.Parse()

	if *ota {
		// Note: runOTA is assumed to be defined elsewhere in the package.
		if err := runOTA(*otaLoader, *otaSketch, *otaOffset, *otaMagic, *otaSketch); err != nil {
			fmt.Printf("OTA error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	// Intercept empty sketch with --squash enabled
	if *squash && flag.NArg() == 0 {
		if *loader_bin == "" {
			fmt.Printf("Error: --loader_bin is required when --squash is used.\n")
			return
		}
		fmt.Printf("Running squash process without a sketch file.\n")
		signAndSquash(*loader_bin, "", *pem_file, *erase_flash_dim, *image_version)
		return
	}

	if flag.NArg() != 1 {
		fmt.Printf("Usage: %s [flags] <filename>\n", os.Args[0])
		flag.PrintDefaults()
		return
	}
	filename := flag.Arg(0)

	// Read the file content
	content, err := os.ReadFile(filename)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	var ELF_HEADER = []byte{0x7f, 0x45, 0x4c, 0x46}
	var elf_header_found = bytes.Compare(ELF_HEADER, content[0:4]) == 0
	if *add_header || (!*force && !elf_header_found) {
		fmt.Printf("File does not have an ELF header, adding empty space\n")

		var newContent = make([]byte, len(content)+16)
		copy(newContent[16:], content)
		content = newContent
	}

	// Create and fill custom header
	var header struct {
		ver   uint8  // @ 0x07
		len   uint32 // @ 0x08
		magic uint16 // @ 0x0c
		flags uint8  // @ 0x0e
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

	// Use backward compatible binary.Write via buffer to support Go versions < 1.23
	var headerBuf bytes.Buffer
	err = binary.Write(&headerBuf, binary.LittleEndian, header)
	if err != nil {
		fmt.Printf("Error encoding header: %v\n", err)
		return
	}
	headerBytes := headerBuf.Bytes()

	// Bytes 7 to 15 are free to use in current ELF specification. We will
	// use them to store the debug and linked flags.
	// Check if the target area is empty
	if !*force {
		for i := 7; i < 16; i++ {
			if content[i] != 0 {
				fmt.Printf("Target ELF header area is not empty. Use --force to overwrite\n")
				return
			}
		}
	}

	// Change the header bytes in the content
	copy(content[7:16], headerBytes)

	// Create a new filename for the copy
	newFilename := *output
	if newFilename == "" {
		newFilename = filename + "-zsk.bin"
	}

	// Write the new content to the new file
	err = os.WriteFile(newFilename, content, 0644)
	if err != nil {
		fmt.Printf("Error writing to file: %v\n", err)
		return
	}

	fmt.Printf("File %s saved as %s\n", filename, newFilename)

	// If squash is requested, execute the signing phase
	// newFilename acts as the sketch_bin_file
	if *squash {
		if *loader_bin == "" {
			fmt.Printf("Error: --loader_bin is required when --squash is used.\n")
			return
		}
		signAndSquash(*loader_bin, newFilename, *pem_file, *erase_flash_dim, *image_version)
	}
}

/*
 *  The loader binary file name zephyr.bin file produced by sysbuild has already
 *  an initial padding of 1024 bytes.
 *  This is done to have a binary files that is ready for the following signing
 *  binary phase: a blank MCUboot header is already provided and can simply
 *  be written with the correct information.
 *  The zephyr.signed.bin file is the zephyr.bin file signed. This means that
 *  the first 32 bytes of the header are written and to the end of the file the
 *  TLV section containing signature information is added.
 *  Here we use zephyr.bin file (NOT the signed version) to avoid to have also
 *  the additional TLV part in the final Image.
 *  However please note that we do not add the header because a blank one has
 *  been already provided by the sysbuild process.
 *  Depending on CONFIG PARAMETER of the variants the padding is automatically
 *  added
 */
func signAndSquash(loaderBinFile string, sketchBinFile string, pemFile string, eraseFlashDim uint, versionStr string) {
	fmt.Printf("\n--- Starting Squash & Sign Process ---\n")
	var config_file string = "undefined"
	var dts_file string = "undefined"
	var output_file string = "undefined"

	var loader_bin []byte
	var sketch_bin []byte
	var err error

	/*
	 * READING LOADER BINARY FILE
	 * ---------------------------*/
	fmt.Printf("+++ Loader file (%s) found!\n", loaderBinFile)
	loader_bin, err = os.ReadFile(loaderBinFile)
	if err != nil {
		log.Fatalf("Error reading loader binary file: %v", err)
	}

	/*
	 * Gathering config and dts filename from bin one
	 * ----------------------------------------------- */
	config_file = ConfigPathFromBin(loaderBinFile)
	dts_file = DtsPathFromBin(loaderBinFile)
	output_file = OutPathFromBin(loaderBinFile)

	/*
	 * CALCULATING LOADER SIZE
	 * ---------------------------*/
	loader_len := uint32(len(loader_bin))
	fmt.Printf("   >>> Loader size %d (0x%08X)\n", loader_len, loader_len)

	var custom_padding_is_present bool = false

	if config_file != "undefined" {
		fmt.Println("--- Parsing configuration file")
		/*
		 * VERIFYING (from configuration) if PADDING was added
		 * --------------------------------------------------- */
		custom_padding_is_present, err = HasConfig(config_file, ConfigPaddingEraseDim)
		if err != nil {
			log.Fatalf("WARNING: problem during config file parsing")
		}
	} else {
		fmt.Println("Error: unable to find config file")
	}

	var slot0_dim uint32 = 0
	var slot1_dim uint32 = 0

	if dts_file != "undefined" {
		fmt.Println("--- Parsing dts file")
		dtsBytes, err := os.ReadFile(dts_file)
		if err != nil {
			fmt.Println("WARNING: Unable to read dts file")
		}

		slot0_dim, slot1_dim, err = GetPartitionSizes(string(dtsBytes))
		if err != nil {
			fmt.Printf("Validation Error: %v\n", err)
		} else {
			fmt.Printf("   >>> Slot 0 size: %d bytes (0x%X)\n", slot0_dim, slot0_dim)
			fmt.Printf("   >>> Slot 1 size: %d bytes (0x%X)\n", slot1_dim, slot1_dim)
		}
	}

	if slot0_dim != slot1_dim {
		fmt.Println("WARNING: slot0 and slot1 have different sizes")
	}

	/*
	 * READING SKETCH BINARY FILE
	 * ---------------------------*/
	var sketch_len uint32 = 0
	if sketchBinFile != "" {
		fmt.Printf("+++ Sketch file (bin) %s found!\n", sketchBinFile)
		sketch_bin, err = os.ReadFile(sketchBinFile)
		if err != nil {
			log.Fatalf("Error reading sketch binary file: %v", err)
		} else {
			sketch_len = uint32(len(sketch_bin))
		}
	} else {
		fmt.Println("WARNING: sketch bin file not defined")
	}

	fmt.Printf("   >>> sketch len = %d (0x%08X)\n", sketch_len, sketch_len)

	/*
	 * CALCULATING PADDING
	 * --------------------*/
	var padding_len uint32 = 0 // Maintained as explicit 0 per cleaned sign.go functionality
	align_padding := make([]byte, padding_len)

	fmt.Println("--- SUMMARY:")
	fmt.Printf("   >>> padding len = %d (0x%08X)\n", padding_len, padding_len)

	/*
	 * CALCULATING SKETCH OFFSET
	 * -------------------------*/
	sketch_offset := uint32(padding_len + loader_len)
	fmt.Printf("   >>> sketch offset = %d (0x%x)\n", sketch_offset, sketch_offset)

	/*
	 * Getting total size available
	 * ---------------------------- */
	total_size := loader_len + padding_len + sketch_len
	fmt.Printf("   >>> total size = %d (0x%08X)\n", total_size, total_size)

	if slot0_dim > 0 && total_size > slot0_dim {
		log.Fatalf("ERROR: Image size greater than slot dimension")
	}

	/*
	 * MAKE NEW IMAGE
	 * ----------------*/
	image := make([]byte, total_size)

	/* COPY LOADER */
	pos := copy(image, loader_bin)

	/* COPY PADDING */
	pos += copy(image[pos:], align_padding)

	/* COPY SKETCH (if present) */
	if sketch_len > 0 {
		pos += copy(image[pos:], sketch_bin)
	}

	/* LOAD AND PARSE THE PRIVATE KEY */
	if pemFile == "" {
		log.Fatalf("Failed to read key: PEM file path is empty")
	}
	keyFile, err := os.ReadFile(pemFile)
	if err != nil {
		log.Fatalf("Failed to read key: %v", err)
	}

	block, _ := pem.Decode(keyFile)
	if block == nil {
		log.Fatalf("Failed to parse PEM block")
	}

	privKey, err := x509.ParsePKCS1PrivateKey(block.Bytes)
	if err != nil {
		log.Fatalf("Failed to parse RSA key: %v", err)
	}

	/* CONSTRUCT IMAGE HEADER */
	imgVersion := parseVersionString(versionStr)
	header := ImageHeader{
		Magic:     ImageMagic,
		LoadAddr:  0x0,
		HdrSize:   HeaderSize,
		PTLVSize:  0,
		ImageSize: uint32(len(image)) - uint32(HeaderSize),
		Flags:     0x0,
		Version:   imgVersion,
	}

	/* OVERWRITE THE FIRST 32 BYTE OF THE IMAGE WITH THE NEW HEADER */
	var headerBuf bytes.Buffer
	err = binary.Write(&headerBuf, binary.LittleEndian, header)
	if err != nil {
		log.Fatalf("Failed to write header: %v", err)
	}
	copy(image[0:32], headerBuf.Bytes())

	/* WRITE INTO THE PADDING THE INFORMATION */
	binary.LittleEndian.PutUint32(image[32:36], sketch_offset)
	binary.LittleEndian.PutUint32(image[36:40], uint32(eraseFlashDim))
	
	var block_num uint32 = 1
	if eraseFlashDim > 0 {
		block_num = (sketch_len / uint32(eraseFlashDim)) + 1
	}
	binary.LittleEndian.PutUint32(image[40:44], block_num)
	
	if custom_padding_is_present {
		binary.LittleEndian.PutUint32(image[44:48], 2)
	} else {
		binary.LittleEndian.PutUint32(image[44:48], 1)
	}

	/*
	 * CALCULATE THE HASH OF THE WHOLE IMAGE
	 */
	imgHash := sha256.Sum256(image)

	/* Calculate the KEYHASH (SHA-256 of the PKCS#1 DER-encoded Public Key) */
	pubKeyBytes := x509.MarshalPKCS1PublicKey(&privKey.PublicKey)
	keyHash := sha256.Sum256(pubKeyBytes)

	/* Generate RSA-PSS Signature */
	signature, err := rsa.SignPSS(rand.Reader, privKey, crypto.SHA256, imgHash[:], &rsa.PSSOptions{
		SaltLength: rsa.PSSSaltLengthEqualsHash,
	})
	if err != nil {
		log.Fatalf("Failed to sign payload: %v", err)
	}

	// Buffer for final output
	var finalImg bytes.Buffer
	finalImg.Write(image)
	appendTLVs(&finalImg, keyHash[:], imgHash[:], signature)

	// Size Validation & Save
	totalImageSize := finalImg.Len()
	if totalImageSize > SlotSize {
		log.Fatalf("Error: Final image size (%d) exceeds the defined slot size (%d)!", totalImageSize, SlotSize)
	}

	err = os.WriteFile(output_file, finalImg.Bytes(), 0644)
	if err != nil {
		log.Fatalf("Failed to write output file(%s), error %v", output_file, err)
	}

	fmt.Println("   === IMAGE built and signed successfully")
	fmt.Printf("   === Output file: %s\n", output_file)
}

func parseVersionString(ver string) ImageVersion {
	parts := strings.Split(ver, ".")
	iv := ImageVersion{Major: 1, Minor: 0, Revision: 0, BuildNum: 0}
	
	if len(parts) > 0 {
		if val, err := strconv.ParseUint(parts[0], 10, 8); err == nil {
			iv.Major = uint8(val)
		}
	}
	if len(parts) > 1 {
		if val, err := strconv.ParseUint(parts[1], 10, 8); err == nil {
			iv.Minor = uint8(val)
		}
	}
	if len(parts) > 2 {
		if val, err := strconv.ParseUint(parts[2], 10, 16); err == nil {
			iv.Revision = uint16(val)
		}
	}
	if len(parts) > 3 {
		if val, err := strconv.ParseUint(parts[3], 10, 32); err == nil {
			iv.BuildNum = uint32(val)
		}
	}
	return iv
}

// appendTLVs handles the exact struct packing required by MCUboot
func appendTLVs(img *bytes.Buffer, keyHash []byte, imgHash []byte, sig []byte) {
	binary.Write(img, binary.LittleEndian, TlvInfoMagic)
	binary.Write(img, binary.LittleEndian, uint16(336))

	// 1. Write SHA256 TLV (Type 0x10)
	binary.Write(img, binary.LittleEndian, TlvSha256)
	binary.Write(img, binary.LittleEndian, uint8(0))
	binary.Write(img, binary.LittleEndian, uint16(len(imgHash)))
	img.Write(imgHash)

	// 2. Write KeyHash TLV (Type 0x01)
	binary.Write(img, binary.LittleEndian, TlvKeyHash)
	binary.Write(img, binary.LittleEndian, uint8(0))
	binary.Write(img, binary.LittleEndian, uint16(len(keyHash)))
	img.Write(keyHash)

	// 3. Write RSA Signature TLV (Type 0x20)
	binary.Write(img, binary.LittleEndian, TlvRsa2048)
	binary.Write(img, binary.LittleEndian, uint8(0))
	binary.Write(img, binary.LittleEndian, uint16(len(sig)))
	img.Write(sig)
}

func GetConfigValue(filePath string, configName string) (string, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	// Ensure clean prefix matching (e.g., CONFIG_FOO=)
	targetConfig := strings.TrimSuffix(configName, "=")
	searchPrefix := targetConfig + "="

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())

		// Skip comments and empty lines
		if len(line) == 0 || strings.HasPrefix(line, "#") {
			continue
		}

		if strings.HasPrefix(line, searchPrefix) {
			// Extract the value after the "="
			value := strings.TrimPrefix(line, searchPrefix)

			// Optional: Remove surrounding quotes if it's a string configuration
			value = strings.Trim(value, `"`)

			return value, nil
		}
	}

	if err := scanner.Err(); err != nil {
		return "", err
	}

	// Return an error if the loop finishes without finding the config
	return "", fmt.Errorf("configuration '%s' not found", targetConfig)
}

func HasConfig(filePath string, configName string) (bool, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return false, err
	}
	defer file.Close()

	// Ensure clean prefix matching (e.g., CONFIG_FOO=)
	targetConfig := strings.TrimSuffix(configName, "=")
	searchPrefix := targetConfig + "="

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())

		// Skip comments (which includes "# CONFIG_XYZ is not set") and empty lines
		if len(line) == 0 || strings.HasPrefix(line, "#") {
			continue
		}

		if strings.HasPrefix(line, searchPrefix) {
			return true, nil
		}
	}

	if err := scanner.Err(); err != nil {
		return false, err
	}

	return false, nil
}

// extractNodeBlock finds a labeled DTS node (e.g., slot0_partition: partition@xxx { ... })
// and returns the string contents inside its curly braces.
func extractNodeBlock(dtsContent, nodeName string) (string, error) {
	// Match pattern: nodeName: [anything but {] { [capture block] }
	pattern := fmt.Sprintf(`(?s)%s:\s*[^{]*\{([^}]+)\}`, regexp.QuoteMeta(nodeName))
	re := regexp.MustCompile(pattern)

	match := re.FindStringSubmatch(dtsContent)
	if len(match) < 2 {
		return "", errors.New("node block not found")
	}
	return match[1], nil
}

func GetPartitionSizes(dtsContent string) (uint32, uint32, error) {
	// 1. Verify boot_partition exists and has the correct label
	bootBlock, err := extractNodeBlock(dtsContent, "boot_partition")
	if err != nil {
		return 0, 0, errors.New("boot_partition not found in DTS")
	}

	labelRegex := regexp.MustCompile(`label\s*=\s*"([^"]+)"`)
	labelMatch := labelRegex.FindStringSubmatch(bootBlock)
	if len(labelMatch) < 2 || labelMatch[1] != "mcuboot" {
		return 0, 0, errors.New("boot_partition does not have the required 'mcuboot' label")
	}

	// 2. Extract and parse slot0_partition
	slot0Block, err := extractNodeBlock(dtsContent, "slot0_partition")
	if err != nil {
		return 0, 0, errors.New("slot0_partition not found in DTS")
	}

	slot0Size, err := parseRegSize(slot0Block)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to parse size for slot0_partition: %v", err)
	}

	// 3. Extract and parse slot1_partition
	slot1Block, err := extractNodeBlock(dtsContent, "slot1_partition")
	if err != nil {
		return 0, 0, errors.New("slot1_partition not found in DTS")
	}

	slot1Size, err := parseRegSize(slot1Block)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to parse size for slot1_partition: %v", err)
	}

	return slot0Size, slot1Size, nil
}

// parseRegSize extracts the size value from a Zephyr DTS 'reg' property.
// It assumes the standard format: reg = <offset size>;
func parseRegSize(block string) (uint32, error) {
	// Match pattern: reg = < offset size > capturing both hex (0x...) or decimal
	re := regexp.MustCompile(`reg\s*=\s*<\s*(0x[0-9a-fA-F]+|\d+)\s+(0x[0-9a-fA-F]+|\d+)\s*>`)
	match := re.FindStringSubmatch(block)

	if len(match) < 3 {
		return 0, errors.New("reg property missing or invalid format")
	}

	sizeStr := match[2]

	// ParseUint with base 0 automatically handles both '0x' prefixed hex and standard decimal
	rvUint64, err := strconv.ParseUint(sizeStr, 0, 32)

	return uint32(rvUint64), err
}

// ConfigPathFromBin replaces the file extension of the given path with ".config"
func ConfigPathFromBin(binPath string) string {
	// filepath.Ext gets the current extension (e.g., ".bin")
	// strings.TrimSuffix removes it, and we append the new one
	return strings.TrimSuffix(binPath, filepath.Ext(binPath)) + ".config"
}

func DtsPathFromBin(binPath string) string {
	return strings.TrimSuffix(binPath, filepath.Ext(binPath)) + ".dts"
}

func OutPathFromBin(binPath string) string {
	return strings.TrimSuffix(binPath, filepath.Ext(binPath)) + ".signed.bin"
}
