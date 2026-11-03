// Copyright (c) Arduino s.r.l. and/or its affiliated companies
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"bufio"
	"bytes"
	"crypto/elliptic"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"github.com/arduino/ArduinoCore-zephyr/tools/lib/cli"
)

// MCUboot Header & Configuration Constants
const (
	ImageMagic   uint32 = 0x96f3b83d
	TlvInfoMagic uint16 = 0x6907
	TlvKeyHash   uint8  = 0x01
	TlvSha256    uint8  = 0x10
	TlvEcdsa256  uint8  = 0x22
	HeaderSize   uint16 = 0x400
	// TLV area: info header + SHA256 + KEYHASH + ECDSA-P256 (DER, at most 72 B)
	TlvMaxLen uint32 = 4 + (4 + 32) + (4 + 32) + (4 + 72)
)

const (
	ConfigMcubootUpdateFooterSize = "CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE"
	ConfigRomStartOffset          = "CONFIG_ROM_START_OFFSET"
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
	ConfigCustomFlashAlignActive = "CONFIG_CUSTOM_FLASH_ALIGN_ACTIVE"
)

func main() {
	cli.Init("zephyr-sketch-tool")

	var output = flag.String("output", "", "Output file (required with --sign; default: add -zsk.bin suffix)")
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

	// Standalone MCUboot signing, without sketch processing.
	var sign = flag.Bool("sign", false, "Sign a standalone MCUboot image with a reserved header")
	var headerSize = flag.Uint("header_size", 0, "[sign] Reserved MCUboot header size in bytes")
	var slotSize = flag.Uint("slot_size", 0, "[sign] Maximum signed image size in bytes")

	// Squash / Sign flags
	var squash = flag.Bool("squash", false, "Squash the loader and sketch, then sign the image")
	var loader_bin = flag.String("loader_bin", "", "Path to the binary loader to be used")
	var pem_file = flag.String("pem_file", "", "Path to the PEM file containing keys")
	var erase_flash_dim = flag.Uint("erase_flash_dim", 0, "Minimum erasing flash sector dimension in bytes")
	var image_version = flag.String("image_version", "1.0.0.0", "Image version for the header (default 1.0.0.0)")

	cli.ParseFlags()

	if *sign {
		if *squash || *ota || flag.NArg() != 1 || *output == "" {
			cli.Fatal("--sign requires one input file and --output, and cannot be combined with --squash or --ota")
		}
		if err := signStandalone(flag.Arg(0), *output, *pem_file, *image_version, *headerSize, *slotSize); err != nil {
			cli.Fatal("%v", err)
		}
		return
	}

	if *ota {
		// Note: runOTA is assumed to be defined elsewhere in the package.
		if err := runOTA(*otaLoader, *otaSketch, *otaOffset, *otaMagic, *otaSketch); err != nil {
			cli.Fatal("%v", err)
		}
		return
	}

	// Intercept empty sketch with --squash enabled
	if *squash && flag.NArg() == 0 {
		if *loader_bin == "" {
			cli.Fatal("--loader_bin is required when --squash is used")
		}
		fmt.Printf("Running squash process without a sketch file.\n")
		signAndSquash(*loader_bin, "", *pem_file, *erase_flash_dim, *image_version)
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

	var ELF_HEADER = []byte{0x7f, 0x45, 0x4c, 0x46}
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
		cli.Fatal("encoding header: %v", err)
	}
	headerBytes := headerBuf.Bytes()

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
	copy(content[7:16], headerBytes)

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

	// If squash is requested, execute the signing phase
	// newFilename acts as the sketch_bin_file
	if *squash {
		if *loader_bin == "" {
			cli.Fatal("--loader_bin is required when --squash is used")
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
   
	/*
	 * Building output filename
	 * ----------------------------------------------- */
	/* 
    * If the sketch file binary is provided then the output name will be
    * <sketch-filename>.signed.bin (this will however contain loader + sketch)
	 * It the sketch file is not provided then a file is created in 
    * in <cache>/arduino/firmwares
    */
	if sketchBinFile != "" {
		output_file = AddSignedModifier(sketchBinFile);
	} else {
		output_loader_path, ok := OutLoaderSignedPath();
		if !ok {
			log.Fatalf("Error while creating cache folder!");
		}
		output_loader_file := filepath.Join(output_loader_path, filepath.Base(loaderBinFile));
		output_file = OutPathFromBin(output_loader_file)
	}

	/*
	 * CALCULATING LOADER SIZE
	 * ---------------------------*/
	loader_len := uint32(len(loader_bin))
	fmt.Printf("   >>> Loader size %d (0x%08X)\n", loader_len, loader_len)

	var custom_padding_is_present bool = false
	var mcuboot_footer_size uint32 = 0
	// Reserved MCUboot header size; defaults to HeaderSize but is overridden
	// below with CONFIG_ROM_START_OFFSET when available, so it is not tied
	// to a specific board.
	var header_size uint32 = uint32(HeaderSize)

	if config_file != "undefined" {
		fmt.Println("--- Parsing configuration file")
		/*
		 * VERIFYING (from configuration) if custom padding alignment
		 * (CONFIG_CUSTOM_FLASH_ALIGN_ACTIVE) was selected, as opposed to
		 * alignment based on the Devicetree erase block size.
		 * --------------------------------------------------- */
		custom_padding_is_present, err = HasConfig(config_file, ConfigCustomFlashAlignActive)
		if err != nil {
			log.Fatalf("WARNING: problem during config file parsing")
		}

		/*
		 * READING MCUboot's update footer size, needed to compute how much
		 * of slot0 is actually available to the signed image.
		 * Config values are emitted by Kconfig in hex (e.g. "0x50"), so the
		 * base must be auto-detected rather than assumed decimal.
		 * --------------------------------------------------- */
		if footerStr, err := GetConfigValue(config_file, ConfigMcubootUpdateFooterSize); err == nil {
			if val, convErr := strconv.ParseUint(footerStr, 0, 32); convErr == nil {
				mcuboot_footer_size = uint32(val)
			} else {
				fmt.Printf("WARNING: unable to parse %s value %q\n", ConfigMcubootUpdateFooterSize, footerStr)
			}
		} else {
			fmt.Printf("WARNING: %s not found in config file\n", ConfigMcubootUpdateFooterSize)
		}

		/*
		 * READING the reserved MCUboot header size from the board's own
		 * configuration (CONFIG_ROM_START_OFFSET) instead of relying on a
		 * value hardcoded for a specific board.
		 * --------------------------------------------------- */
		if romOffsetStr, err := GetConfigValue(config_file, ConfigRomStartOffset); err == nil {
			if val, convErr := strconv.ParseUint(romOffsetStr, 0, 32); convErr == nil {
				header_size = uint32(val)
			} else {
				fmt.Printf("WARNING: unable to parse %s value %q, defaulting to 0x%X\n", ConfigRomStartOffset, romOffsetStr, header_size)
			}
		} else {
			fmt.Printf("WARNING: %s not found in config file, defaulting to 0x%X\n", ConfigRomStartOffset, header_size)
		}
	} else {
		log.Fatalf("Error: unable to find config file")
	}

	// MCUboot header (32 B) + sketch layout words written at 32..47
	const minHeaderSize = 48
	if header_size < minHeaderSize || header_size > 0xFFFF {
		log.Fatalf("Error: %s=0x%X: the loader must reserve %d..65535 bytes for the MCUboot header",
			ConfigRomStartOffset, header_size, minHeaderSize)
	}

	var slot0_dim uint32 = 0
	var slot1_dim uint32 = 0

	if dts_file != "undefined" {
		fmt.Println("--- Parsing dts file")
		dtsBytes, err := os.ReadFile(dts_file)
		if err != nil {
			log.Fatalf("Error: unable to read dts file: %v", err)
		}

		slot0_dim, slot1_dim, err = GetPartitionSizes(string(dtsBytes))
		if err != nil {
			log.Fatalf("Validation Error: %v", err)
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

	/*
	 * VERIFY THE RESERVED HEADER AREA IS BLANK
	 * -----------------------------------------
	 * The loader is expected to provide a blank (all-zero) MCUboot header
	 * area, written here. Refuse to sign (and overwrite) an input whose
	 * header area is not zero, as imgtool does, to avoid silently signing
	 * an already-signed/non-blank image.
	 * --------------------------------------------------- */
	if total_size < header_size {
		log.Fatalf("Error: image size (%d) is smaller than the reserved header size (%d)", total_size, header_size)
	}
	for _, b := range image[0:header_size] {
		if b != 0 {
			log.Fatalf("Error: reserved header area is not blank; refusing to sign a non-blank/already-signed image")
		}
	}

	/* LOAD AND PARSE THE PRIVATE KEY */
	key, err := readSigningKey(pemFile)
	if err != nil {
		log.Fatal(err)
	}

	if key.Curve != elliptic.P256() {
		log.Fatalf("Error: %s is not an ECDSA P-256 key (TLV 0x22 requires P-256)", pemFile)
	}

	/* WRITE INTO THE PADDING THE INFORMATION */
	binary.LittleEndian.PutUint32(image[32:36], sketch_offset)
	binary.LittleEndian.PutUint32(image[36:40], uint32(eraseFlashDim))

	var block_num uint32 = 1
	if eraseFlashDim > 0 {
		erase := uint32(eraseFlashDim)
		// Round up, including the TLV area appended after the image so the
		// erased region also covers the trailing signature/hash metadata.
		block_num = (sketch_len + TlvMaxLen + erase - 1) / erase
	}
	binary.LittleEndian.PutUint32(image[40:44], block_num)

	if custom_padding_is_present {
		binary.LittleEndian.PutUint32(image[44:48], 2)
	} else {
		binary.LittleEndian.PutUint32(image[44:48], 1)
	}

	// signingVersion is shared with --sign and validates the version string
	// rather than silently defaulting it.
	imgVersion, err := signingVersion(versionStr)
	if err != nil {
		log.Fatal(err)
	}

	// Metadata is complete; use the same MCUboot signer as standalone images.
	signed, err := signMCUbootImage(image, key, uint16(header_size), imgVersion)
	if err != nil {
		log.Fatal(err)
	}

	// Size Validation & Save
	totalImageSize := uint32(len(signed))
	if slot0_dim > 0 {
		maxImageSize := slot0_dim - mcuboot_footer_size
		if totalImageSize > maxImageSize {
			log.Fatalf("Error: Final image size (%d) exceeds slot0 size minus MCUboot's trailer (%d)!", totalImageSize, maxImageSize)
		}
	} else {
		fmt.Println("WARNING: slot0 size unknown, skipping final image size validation")
	}

	err = os.WriteFile(output_file, signed, 0644)
	if err != nil {
		log.Fatalf("Failed to write output file(%s), error %v", output_file, err)
	}

	fmt.Println("   === IMAGE built and signed successfully")
	fmt.Printf("   === Output file: %s\n", output_file)
}

// appendTLVs handles the exact struct packing required by MCUboot
func appendTLVs(img *bytes.Buffer, keyHash []byte, imgHash []byte, sig []byte) {
	binary.Write(img, binary.LittleEndian, TlvInfoMagic)
	// Size is dynamic because ECDSA ASN.1 signatures vary slightly in length (header + TLVs)
	binary.Write(img, binary.LittleEndian, uint16(80+len(sig)))

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

	// 3. Write ECDSA Signature TLV (Type 0x22)
	binary.Write(img, binary.LittleEndian, TlvEcdsa256)
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

func OutLoaderSignedPath() (string, bool) {
	cacheDir, err := os.UserCacheDir()
	var rv bool = true;
	if err != nil {
			log.Fatal("Could not find cache directory:", err)
	}

	appCacheDir := filepath.Join(cacheDir, "arduino/firmwares");
	if err := os.MkdirAll(appCacheDir, 0755); err != nil {
		fmt.Println("[ERROR]:Failed to create app cache directory!");
		rv = false;
	}
	return appCacheDir, rv
}

// AddSignedModifier inserts ".signed" into the file name, never returning
// the input path unmodified: "s.ino.bin-zsk.bin" -> "s.ino.signed.bin-zsk.bin",
// "y.bin" -> "y.signed.bin", "y" -> "y.signed".
func AddSignedModifier(fullPath string) string {
	dir := filepath.Dir(fullPath)
	fileName := filepath.Base(fullPath)

	var newFileName string
	if strings.Contains(fileName, ".ino.") {
		newFileName = strings.Replace(fileName, ".ino.", ".ino.signed.", 1)
	} else if i := strings.Index(fileName, "."); i > 0 {
		newFileName = fileName[:i] + ".signed" + fileName[i:]
	} else {
		newFileName = fileName + ".signed"
	}
	return filepath.Join(dir, newFileName)
}
