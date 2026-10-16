// SPDX-License-Identifier: MPL-2.0
// Copyright (c) Arduino s.r.l. and/or its affiliated companies

package main

import (
	"context"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"os/exec"
	"runtime"
	"path/filepath"
	"strings"

	"github.com/codeclysm/extract/v4"
	cp "github.com/otiai10/copy"
)

func main() {
	// Parse and check arguments
	if len(os.Args) > 1 && (os.Args[1] == "-h" || os.Args[1] == "--help") {
		fmt.Fprintf(os.Stderr, "Usage: %s [<git_core_path>] [forced_hash]\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  git_core_path: path to the ArduinoCore-zephyr git repository (default: current directory)\n")
		fmt.Fprintf(os.Stderr, "  forced_hash:   force a specific git hash (for debug only)\n")
		os.Exit(0)
	}

	gitCorePath := "."
	if len(os.Args) > 1 {
		gitCorePath = os.Args[1]
	}

	// Force a specific hash, for debug only
	inputHash := ""
	if len(os.Args) > 2 {
		inputHash = os.Args[2]
	}

	// Test if the provided path contains 'variants/' folder
	if _, err := os.Stat(filepath.Join(gitCorePath, "variants")); os.IsNotExist(err) {
		fmt.Println("Error: not an Arduino Core for Zephyr folder:", gitCorePath)
		return
	}

	// Make sure the path's last components are "hardware", any folder, and start with "zephyr"
	absPath, err := filepath.Abs(gitCorePath)
	if err != nil {
		fmt.Println("Error getting absolute path:", err)
		return
	}
	pathParts := strings.Split(filepath.ToSlash(absPath), "/")
	if len(pathParts) < 3 ||
	   pathParts[len(pathParts)-3] != "hardware" ||
	   !strings.HasPrefix(pathParts[len(pathParts)-1], "zephyr") {
		expPath := filepath.Join("hardware", "*", "zephyr")
		fmt.Println("Error: the core path should end with ", expPath)
		return
	}

	// Get the current git hash if not forced
	if inputHash == "" {
		cmd := exec.Command("git", "-C", gitCorePath, "describe", "--always", "HEAD")
		stdout, err := cmd.Output()
		if err != nil {
			fmt.Println("Error executing command:", err)
			return
		}

		inputHash = strings.TrimSpace(string(stdout))
	}
	fmt.Println("Git SHA:", inputHash)

	// Check for uncommitted changes in firmwares/ and variants/ folders
	cmd := exec.Command("git", "-C", gitCorePath, "ls-files", "--exclude-standard", "-dmo")
	stdout, err := cmd.Output()
	if err != nil {
		fmt.Println("Error executing command:", err)
		return
	}

	var lines []string
	var changes []string
	lines = strings.Split(string(stdout), "\n")
	for _, path := range lines {
		if strings.HasPrefix(path, "firmwares/") || strings.HasPrefix(path, "variants/") {
			changes = append(changes, path)
		}
	}

	if len(changes) > 0 {
		fmt.Println("The git repository contains uncommitted files:")
		for _, path := range changes {
			fmt.Println("- ", path)
		}
		fmt.Println("Please commit, stash or remove them before running this script.")
		return
	}

	// Create a temporary folder to download and extract the archive
	tmpDir, err := os.MkdirTemp("", "sync-zephyr-artifacts")
	if err != nil {
		fmt.Println("Error creating temp dir:", err)
		return
	}
	defer os.RemoveAll(tmpDir) // Clean up

	// Compose download URL from git hash
	var archive_stream io.Reader

	hash := inputHash
	for len(hash) >= 7 {
		filename := fmt.Sprintf("ArduinoCore-zephyr-%s.tar.bz2", hash)
		url := fmt.Sprintf("http://downloads.arduino.cc/cores/zephyr/%s", filename)
		fmt.Println("Trying URL:", url)
		// Download the zip file from the URL

		// Get the data
		resp, err := http.Get(url)
		if err != nil {
			fmt.Println("Error downloading", url, err)
			return
		}
		defer resp.Body.Close()

		// Check server response, try shorter hashes if not found
		if resp.StatusCode == http.StatusNotFound {
			resp.Body.Close()
			hash = hash[:len(hash)-1]
			continue
		} else if resp.StatusCode != http.StatusOK {
			fmt.Println("bad status:", resp.Status)
			return
		}

		archive_stream = resp.Body
		break
	}

	if archive_stream == nil {
		fmt.Println("Could not find a valid archive for git hash", inputHash)
		return
	}

	fmt.Println("Downloading and extracting...")

	// extract the archive just received to tmpDir/
	err = extract.Bz2(context.Background(), archive_stream, tmpDir, nil)

	if err != nil {
		fmt.Println("Error extracting archive:", err)
		return
	}

	checkoutApi := false
	apiPath := filepath.Join(gitCorePath, "cores", "arduino", "api")
	apiHeaderPath := filepath.Join(apiPath, "ArduinoAPI.h")
	_, err = os.Lstat(apiHeaderPath)
	if err != nil {
		// If the file doesn't exist, we will checkout the Arduino API
		// folder as well. Remove any leftover content for this.
		checkoutApi = true
		info, err := os.Lstat(apiPath)
		if err == nil {
			// On Windows, the user might create a junction for the api
			// folder; that will appear as a "regular" directory, but we
			// should not emove the linked content but only the junction itself.
			isJunction := runtime.GOOS == "windows"	&& info.Mode()&os.ModeIrregular != 0
			if info.IsDir() && !isJunction {
				// Regular directory: remove it entirely
				os.RemoveAll(apiPath)
			} else {
				// Anything else: remove just the file or the junction itself
				os.Remove(apiPath)
			}
		} else if !os.IsNotExist(err) {
			fmt.Println("Error checking api path:", err)
			return
		}

		// Add the folder to the ignored list to avoid pestering the user about this
		cmd := exec.Command("git", "-C", gitCorePath, "update-index", "--assume-unchanged", apiPath)
		_ = cmd.Run() // no error check, the api symlink may be gone in the future
	}

	// Remove old firmwares, variants/*/llext-edk files and shared variant headers
	os.RemoveAll(filepath.Join(gitCorePath, "firmwares"))
	os.RemoveAll(filepath.Join(gitCorePath, "variants", "include"))
	filepath.WalkDir(filepath.Join(gitCorePath, "variants"), func(path string, d os.DirEntry, err error) error {
		if err != nil {
			fmt.Println("Error:", err)
			return nil
		}

		if d.IsDir() && d.Name() == "llext-edk" {
			os.RemoveAll(path)
			return fs.SkipDir
		}
		return nil
	})

	// Copy the content of firmware folder to gitCorePath/firmware
	err = cp.Copy(filepath.Join(tmpDir, "ArduinoCore-zephyr", "firmwares"), filepath.Join(gitCorePath, "firmwares"))
	if err != nil {
		fmt.Println("Error copying firmware folder:", err)
		return
	}
	// Copy the content of variants folder to gitCorePath/variants
	err = cp.Copy(filepath.Join(tmpDir, "ArduinoCore-zephyr", "variants"), filepath.Join(gitCorePath, "variants"))
	if err != nil {
		fmt.Println("Error copying variants folder:", err)
		return
	}

	// Copy the content of api folder to gitCorePath/cores/arduino/api, if needed
	if checkoutApi {
		sourceApiPath := filepath.Join(tmpDir, "ArduinoCore-zephyr", "cores", "arduino", "api")
		err = cp.Copy(sourceApiPath, apiPath)
		if err != nil {
			fmt.Println("Error copying api folder:", err)
			return
		}
	}

	fmt.Println("Done.")
}
