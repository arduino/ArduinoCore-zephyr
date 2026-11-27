# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# Drive the inputs of the sketch's interrupt pins with the --gpio_irq events:
# CHANGE on pin 2 sees both edges, RISING on pin 3 only its rise, FALLING on
# pin 4 only its fall, and the detached pin 5 none.

run_until '^2 1 1 0$' "interrupt: handlers run for their edges" \
	--sketch="$SKETCH" \
	--gpio_irq=2@100:1,2@200:0,3@300:1,3@400:0,4@500:1,4@600:0,5@700:1,5@800:0

expect "interrupt: counters start at 0" stdout '^0 0 0 0$'
expect_none "interrupt: no count above its edges" stdout \
	'^([3-9]|[1-9][0-9]+) |^2 ([2-9]|[1-9][0-9]+) |^2 1 ([2-9]|[1-9][0-9]+) |^2 1 1 [1-9]'
