VERSION=$1

mkdir -p distrib/linux_amd64
GOOS=linux GOARCH=amd64 go build && mv zephyr-sketch-tool distrib/linux_amd64/zephyr-sketch-tool

mkdir -p distrib/linux_arm64
GOOS=linux GOARCH=arm64 go build && mv zephyr-sketch-tool distrib/linux_arm64/zephyr-sketch-tool

mkdir -p distrib/macos_amd64
GOOS=darwin GOARCH=amd64 go build && mv zephyr-sketch-tool distrib/macos_amd64/zephyr-sketch-tool

mkdir -p distrib/windows_386
GOOS=windows GOARCH=386 go build && mv zephyr-sketch-tool* distrib/windows_386/zephyr-sketch-tool.exe

cd distrib

tar -czf zephyr-sketch-tool-${VERSION}-linux_amd64.tar.gz linux_amd64
tar -czf zephyr-sketch-tool-${VERSION}-linux_arm64.tar.gz linux_arm64
tar -czf zephyr-sketch-tool-${VERSION}-macos_amd64.tar.gz macos_amd64
zip -r zephyr-sketch-tool-${VERSION}-windows_386.zip windows_386
