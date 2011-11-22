/* read this: http://www.xgc.com/manuals/gcc-1750-ug/p5node5.html */

SECTIONS {
	.text : {
		*(.text)
	}
	.data : {
		*(.data)
	}
	.bss : {
		*(.bss)
		*(COMMON)
	}
}