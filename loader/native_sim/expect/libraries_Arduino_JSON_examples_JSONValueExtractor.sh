# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0
#
# JSONValueExtractor prints an object, then each of its values, recursively,
# then loops doing nothing: run it until the last nested value is printed.

run_until '^"hij" : "bar"$' "JSONValueExtractor: run" --sketch="$SKETCH"
expect "JSONValueExtractor: object" stdout '^\{"foo":"bar","blah":\{"abc":42,"efg":"pod","cde":\{"pan1":"King","pan2":3.14\}\},"jok":\{"hij":"bar"\}\}$'
expect "JSONValueExtractor: nested value" stdout '^"pan2" : 3.14$'
