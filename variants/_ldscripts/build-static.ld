/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

ENTRY(entry_point)
SECTIONS {
    /DISCARD/ : {
        *(.ARM.attributes)
        *(.ARM.attributes.*)
        *(.ARM.extidx)
        *(.ARM.extidx.*)
        *(.comment)
        *(.comment.*)
        *(.llvmbc)
        *(.llvmcmd)
        /* e.g. the build ID added by host toolchains: unused, and placed at
           the start of FLASH, where the entry point is expected */
        *(.note .note.*)
    }

    .text : ALIGN(4) {
        KEEP (*(.entry_point))
        *(.text)
        *(.stub)
        *(.text*)
        *(.text.*)
        *(.text._*)

        KEEP (*(.init))
        KEEP (*(.ctors))
        KEEP (*(.dtors))
        KEEP (*(.fini))
    } >FLASH

    .static_thread_data_area : {
        __static_thread_data_list_start = .;
        KEEP(*(.static_thread_data_area SORT_BY_NAME(.__static_thread_data.*)));
        __static_thread_data_list_end = .;
    } >FLASH

    .preinit_array : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } >FLASH

    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } >FLASH

    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >FLASH

    .rodata : {
        *(.rodata)
        *(.rodata1)
        *(.rodata.*)
    } >FLASH

    .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    _sidata = LOADADDR(.data);

    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.* .tdata*)
        *(._usbd_context.static.usbd_)
        *(.ramfunc)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss .bss.* .tbss* COMMON)
        *(.noinit .noinit.*)
        . = ALIGN(4);
        _ebss = .;
        . = ALIGN(1024);
    } >RAM

    .symtab : {
        KEEP(*(.symtab))
    }

    .strtab : {
        KEEP(*(.strtab))
    }

    .shstrtab : {
        KEEP(*(.shstrtab))
    }

    .rel : {
        KEEP(*(.rel .rel.*))
    }

    /* link-time constants in a static link: keep them with the code */
    .got : {
        KEEP(*(.got .got.* .got.plt .got.plt*))
    } >FLASH
}
