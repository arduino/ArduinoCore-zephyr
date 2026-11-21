/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Uses the C++ standard library containers; exits 0 if all agree. */

#include <map>
#include <string>
#include <vector>

void setup() {
	std::vector<int> v;
	std::map<std::string, int> m;
	bool ok = true;

	for (int i = 1; i <= 10; i++) {
		v.push_back(i);
	}
	m["answer"] = std::stoi("42");
	m["sum"] = 0;
	for (int i : v) {
		m["sum"] += i;
	}
	ok &= (v.size() == 10);
	ok &= (m.at("sum") == 55);
	ok &= (std::to_string(m.at("answer")) + "-ok" == "42-ok");

	Serial.begin(115200);
	Serial.println(ok ? "cpp-ok" : "cpp-fail");
	exit(ok ? 0 : 1);
}

void loop() {
}
