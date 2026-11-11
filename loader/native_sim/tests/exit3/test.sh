# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# exit(n) from a sketch ends zephyr.exe with exit code n; sketch files
# that cannot be run make it fail with exit code 1.

run 3 "exit3: exit() code" --sketch="$SKETCH"
expect "exit3: Serial output" stdout '^hello native'

run 1 "exit3: missing sketch file" --sketch="$OUT/nonexistent"
expect "exit3: missing sketch file error" stderr 'sketch: cannot open'

# valid header, but not a prelinked sketch: cannot be run on native_sim
if "$(cli_property runtime.tools.zephyr-sketch-tool.path)/zephyr-sketch-tool" \
		-output "$OUT/dynamic.bin" "${SKETCH%-zsk.bin}" > "$OUT/tool.log" 2>&1 ; then
	run 1 "exit3: non-prelinked sketch" --sketch="$OUT/dynamic.bin"
else
	report fail "exit3: non-prelinked sketch creation" "$OUT/tool.log"
fi

head -c 5M /dev/zero > "$OUT/big.bin"
run 1 "exit3: oversized sketch file" --sketch="$OUT/big.bin"
expect "exit3: oversized sketch file error" stderr 'sketch: .*too large'
