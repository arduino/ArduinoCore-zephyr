export ZEPHYR_SDK_INSTALL_DIR=/ssd/zephyr-sdk-0.16.8/

set -e

board=$1
variant=$2

if [[ $# -eq 0 ]]; then
board=arduino_giga_r1//m7
variant=arduino_giga_r1_m7
fi

(west build loader -b $board -p && west build -t llext-edk)
(tar xvfp build/zephyr/llext-edk.tar.xz --directory variants/$variant/)
(cp build/zephyr/zephyr.elf firmwares/zephyr-$variant.elf)
(cp build/zephyr/zephyr.bin firmwares/zephyr-$variant.bin)