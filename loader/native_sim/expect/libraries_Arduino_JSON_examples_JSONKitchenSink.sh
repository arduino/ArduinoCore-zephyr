# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# JSONKitchenSink prints values of each JSON type on Serial, then loops doing
# nothing: run it until its last line is printed.

run_until '^myObject = \{"foo":"bar","blah":\{"abc":42\}\}$' "JSONKitchenSink: run" --sketch="$SKETCH"
expect "JSONKitchenSink: double" stdout '^myDouble = 4242.4242$'
expect "JSONKitchenSink: string" stdout '^myString = "Hello World!"$'
expect "JSONKitchenSink: array" stdout '^myArray = \[42,42.5\]$'
