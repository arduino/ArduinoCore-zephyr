# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# JSONObject prints its parsed and created objects on Serial, then loops
# doing nothing: run it until its last line is printed.

run_until '^myObject = \{"hello":"there!"' "JSONObject: run" --sketch="$SKETCH"
expect "JSONObject: parse" stdout '^myObject = \{"result":true,"count":42,"foo":"bar"\}'
expect "JSONObject: stringify" stdout '^JSON.stringify\(myObject\) = \{"hello":"world","true":true,"x1":42,"x2":42,"x3":42\}'
