/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

FORCE_GROUP_ALLOCATION

SECTIONS {

    /DISCARD/ : {
        *(.ARM.attributes)
        *(.ARM.attributes.*)
        *(.ARM.exidx)
        *(.ARM.exidx.*)
        *(.ARM.extidx)
        *(.ARM.extidx.*)
        *(.ARM.extab)
        *(.ARM.extab.*)
        *(.comment)
        *(.comment.*)
        *(.llvmbc)
        *(.llvmcmd)
        *(.eh_frame)
    }

    .text 0x00000000 : ALIGN(4) {
        *(.text)
        *(.stub)
        *(.text*)
        *(.text.*)
        *(.text._*)

        KEEP (*(.init))
        KEEP (*(.ctors))
        KEEP (*(.dtors))
        KEEP (*(.fini))
    }

    .static_thread_data_area : {
        __static_thread_data_list_start = .;
        KEEP(*(.static_thread_data_area SORT_BY_NAME(.__static_thread_data.*)));
        __static_thread_data_list_end = .;
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.* COMMON)
        *(.noinit .noinit.*)
    }

    .exported_sym : {
        KEEP(*(.exported_sym))
        KEEP(*(.exported_sym.*))
    }

    .init_array : {
        KEEP(*(.init_array))
    }

    .fini_array : {
        KEEP(*(.fini_array))
    }

    .symtab : {
        KEEP(*(.symtab))
    }

    .strtab : {
        KEEP(*(.strtab))
    }

    .shstrtab : {
        KEEP(*(.shstrtab))
    }

    .got : {
        KEEP(*(.got .got.* .got.plt .got.plt*))
    }
}
