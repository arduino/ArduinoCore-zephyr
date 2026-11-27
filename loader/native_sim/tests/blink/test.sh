# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Run Blink for one simulated second: the emulated GPIO log shows the LED
# pin (13) toggling, and never the input pin 7.

run 0 "blink: run for 1s" --sketch="$SKETCH" -stop_at=1

values=$(sed -n -e 's/^\[gpio\] .* pin 13 = \([01]\)$/\1/p' "$OUT/stdout" | tr -d '\n')
if [ ${#values} -ge 4 ] && ! grep -qE '00|11' <<< "$values" ; then
	report pass "blink: pin 13 toggled ${#values} times"
else
	echo "pin 13 values: '$values'" > "$OUT/details"
	report fail "blink: pin 13 toggles" "$OUT/details"
fi

if ! grep -qE '^\[gpio\] .* pin 7 = ' "$OUT/stdout" ; then
	report pass "blink: input pin 7 not logged"
else
	report fail "blink: input pin 7 not logged" "$OUT/stdout"
fi
