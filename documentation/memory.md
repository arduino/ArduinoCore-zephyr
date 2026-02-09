# Loader Memory Usage

This document describes the memory usage patterns of the ArduinoCore-zephyr loader implementation.

## Memory Configuration

This values are board-specific, please refer to the variant config file for the real values. For example the memory parameters for Uno Q can be found at [arduino_uno_q_stm32u585xx.conf](./variants/arduino_uno_q_stm32u585xx/arduino_uno_q_stm32u585xx.conf).

| Setting |  Description |
|---------|-------------|
| `CONFIG_HEAP_MEM_POOL_SIZE`  | Main heap pool size |
| `CONFIG_MAIN_STACK_SIZE` |  Main/LLEXT thread stack size |
| `CONFIG_LLEXT_HEAP_SIZE` |  LLEXT internal heap |

## Memory Layout Diagram

```text
+-----------------------------------------------------------------------------+
|                              FLASH MEMORY                                   |
+-----------------------------------------------------------------------------+
|  +---------------------------------------------------------------------+    |
|  |                      Loader Firmware                                |    |
|  +---------------------------------------------------------------------+    |
|  +---------------------------------------------------------------------+    |
|  |                   user_sketch partition                             |    |
|  |              (64-128 KB, board-specific)                            |    |
|  |  +---------------+------------------------------------------+       |    |
|  |  | Header (16B)  |           Sketch ELF Data                |       |    |
|  |  +---------------+------------------------------------------+       |    |
|  +---------------------------------------------------------------------+    |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                               RAM MEMORY                                    |
+-----------------------------------------------------------------------------+
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                   CONFIG_HEAP_MEM_POOL_SIZE                         |    |
|  |                      (default: 100 KB)                              |    |
|  |  +-------------------------------------------------------------+    |    |
|  |  |   Main Heap Pool (k_malloc, etc.)                           |    |    |
|  |  |   Used by loader for:                                       |    |    |
|  |  |   - Sketch Buffer                                           |    |    |
|  |  |     Allocated via k_aligned_alloc(4096, sketch_len)         |    |    |
|  |  +-------------------------------------------------------------+    |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                    CONFIG_LLEXT_HEAP_SIZE                           |    |
|  |  +-------------------------------------------------------------+    |    |
|  |  |   LLEXT Internal Heap                                       |    |    |
|  |  |   - Symbol table allocations                                |    |    |
|  |  |   - Section metadata                                        |    |    |
|  |  |   - Relocation structures                                   |    |    |
|  |  |   - Sketch .text, .rodata, .data, .bss sections             |    |    |
|  |  |     (see CONFIG_LLEXT_RODATA_NO_RELOC below)                |    |    |
|  |  +-------------------------------------------------------------+    |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                    CONFIG_MAIN_STACK_SIZE                           |    |
|  |                      (default: 32 KB)                               |    |
|  |  +-------------------------------------------------------------+    |    |
|  |  |   LLEXT Thread Stack (llext_stack)                          |    |    |
|  |  |   - Sketch execution context                                |    |    |
|  |  |   - Local variables                                         |    |    |
|  |  |   - Function call frames                                    |    |    |
|  |  |   - Interrupt handling                                      |    |    |
|  |  +-------------------------------------------------------------+    |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                   Sketch RAM (remaining memory)                     |    |
|  |  +-------------------------------------------------------------+    |    |
|  |  |   Dynamic allocations from sketch (malloc/free)             |    |    |
|  |  |   - Sketch .data and .bss sections                          |    |    |
|  |  |   - Runtime heap allocations                                |    |    |
|  |  +-------------------------------------------------------------+    |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

```

## CONFIG_LLEXT_RODATA_NO_RELOC

When `CONFIG_LLEXT_RODATA_NO_RELOC=y`, `.llext.rodata.noreloc` sections stay in flash instead of being copied to the LLEXT heap. This reduces LLEXT heap usage at the cost of flash space.

When `CONFIG_LLEXT_RODATA_NO_RELOC=n` (or unset), all `.llext.*` sections are copied into the LLEXT heap and count against `CONFIG_LLEXT_HEAP_SIZE`.

The `maximum_data_size` in `boards.txt` is set to `LLEXT_HEAP_SIZE * 1024` for each board.
