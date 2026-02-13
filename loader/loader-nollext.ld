#ifndef CONFIG_LLEXT
	SECTION_PROLOGUE(exported_sym, 0 (INFO),)
	{
		KEEP(*(.exported_sym*))
		KEEP(*(.exported_sym.*))
	}
#endif
