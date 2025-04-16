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
        *stdexcept.o
        *eh_*.o
        *cow-stdexcept.o
        *functexcept.o
        *cow-string-inst.o
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

        __static_thread_data_list_start = .;
        KEEP(*(SORT_BY_NAME(.__static_thread_data.static.*)));
        __static_thread_data_list_end = .;
    }

    .rodata : {
        *(.rodata)
        *(.rodata1)
        *(.rodata.*)
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

    .rel : {
        KEEP(*(.rel .rel.*))
    }

    .got : {
        KEEP(*(.got .got.* .got.plt .got.plt*))
    }
}