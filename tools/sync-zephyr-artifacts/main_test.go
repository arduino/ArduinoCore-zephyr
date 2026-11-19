// SPDX-License-Identifier: MPL-2.0
// Copyright (c) Arduino s.r.l. and/or its affiliated companies

package main

import (
	"context"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"testing"

	"github.com/codeclysm/extract/v4"
)

// serve the files of dir over HTTP, as downloads.arduino.cc/cores/zephyr
func serve(t *testing.T, dir string) string {
	srv := httptest.NewServer(http.FileServer(http.Dir(dir)))
	t.Cleanup(srv.Close)
	return srv.URL
}

// make dir/name, a tar archive of ArduinoCore-zephyr/marker, compressed with
// the given tar option ("-I zstd" or "-j")
func makeArchive(t *testing.T, dir, name string, tarOpts ...string) {
	src := t.TempDir()
	os.MkdirAll(filepath.Join(src, "ArduinoCore-zephyr"), 0o755)
	os.WriteFile(filepath.Join(src, "ArduinoCore-zephyr", "marker"), []byte(name), 0o644)
	args := append(tarOpts, "-cf", filepath.Join(dir, name), "-C", src, "ArduinoCore-zephyr")
	if out, err := exec.Command("tar", args...).CombinedOutput(); err != nil {
		t.Fatalf("tar: %v: %s", err, out)
	}
}

// check that findArchive gets the expected archive for hash, and that it
// extracts to the marker of that archive
func checkFound(t *testing.T, url, hash, want string) {
	body, err := findArchive(url, hash)
	if err != nil {
		t.Fatalf("findArchive(%q): %v", hash, err)
	}
	defer body.Close()
	out := t.TempDir()
	if err := extract.Archive(context.Background(), body, out, nil); err != nil {
		t.Fatalf("extract: %v", err)
	}
	got, err := os.ReadFile(filepath.Join(out, "ArduinoCore-zephyr", "marker"))
	if err != nil || string(got) != want {
		t.Fatalf("hash %q: got %q (%v), want %q", hash, got, err, want)
	}
}

func TestFindArchivePrefersZstd(t *testing.T) {
	dir := t.TempDir()
	makeArchive(t, dir, "ArduinoCore-zephyr-abcdef12.tar.zst", "-I", "zstd")
	makeArchive(t, dir, "ArduinoCore-zephyr-abcdef12.tar.bz2", "-j")
	checkFound(t, serve(t, dir), "abcdef12", "ArduinoCore-zephyr-abcdef12.tar.zst")
}

func TestFindArchiveFallsBackToBz2(t *testing.T) {
	dir := t.TempDir()
	makeArchive(t, dir, "ArduinoCore-zephyr-abcdef12.tar.bz2", "-j")
	checkFound(t, serve(t, dir), "abcdef12", "ArduinoCore-zephyr-abcdef12.tar.bz2")
}

func TestFindArchiveTruncatesHash(t *testing.T) {
	dir := t.TempDir()
	makeArchive(t, dir, "ArduinoCore-zephyr-abcdef1.tar.zst", "-I", "zstd")
	checkFound(t, serve(t, dir), "abcdef1234", "ArduinoCore-zephyr-abcdef1.tar.zst")
}

func TestFindArchiveNotFound(t *testing.T) {
	if body, err := findArchive(serve(t, t.TempDir()), "abcdef12"); err == nil {
		body.Close()
		t.Fatal("findArchive found an archive in an empty folder")
	}
}
