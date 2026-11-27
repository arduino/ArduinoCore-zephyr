# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# No-op pin control for native targets. The header directory is part of
# zephyr_interface, so it is also exported to sketches through the EDK.
target_sources(app PRIVATE pinctrl_emul.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
