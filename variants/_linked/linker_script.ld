INCLUDE "provides.ld"

PROVIDE(TEXT_LENGTH = 0x100000);

MEMORY
{
    FLASH (rx) : ORIGIN = _sketch_start + 16, LENGTH = TEXT_LENGTH
    RAM (rwx) : ORIGIN = kheap_llext_heap, LENGTH = kheap_llext_heap_size
}

PROVIDE(__heap_start = kheap__system_heap);
PROVIDE(__heap_end = kheap__system_heap + kheap__system_heap_size);

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

        __static_thread_data_list_start = .;
        KEEP(*(SORT_BY_NAME(.__static_thread_data.static.*)));
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

    .got : {
        KEEP(*(.got .got.* .got.plt .got.plt*))
    }
}