# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# JSONArray prints its parsed and created arrays on Serial, then loops doing
# nothing: run it until its last line is printed.

run_until '^myArray\[5\] = 42$' "JSONArray: run" --sketch="$SKETCH"
expect "JSONArray: parse" stdout '^myArray\[2\] = apple$'
expect "JSONArray: stringify" stdout '^JSON.stringify\(myArray\) = \[false,4242.5,"orange","world",true,42\]$'
