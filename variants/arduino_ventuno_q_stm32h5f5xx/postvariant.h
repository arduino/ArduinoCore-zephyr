/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* This dance is necessary to force the inclusion of "Arduino_RouterBridge.h"
 * during the library discovery phase, so that the library include path is
 * added to the sketch build process.
 *
 * This will "discover" the stub in the libraries/stubs/ folder if the library
 * is not installed, and the actual header if it is.
 */

#if ARDUINO_LIBRARY_DISCOVERY_PHASE
#include "Arduino_RouterBridge.h"
#else
#if __has_include("Arduino_RouterBridge.h")
#include "Arduino_RouterBridge.h"
#endif
#endif
