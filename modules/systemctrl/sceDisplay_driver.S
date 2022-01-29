	.set noreorder

#include "pspstub.s"

	STUB_START	"sceDisplay_driver",0x00090011,0x00030005
	STUB_FUNC	0x984C27E7,sceDisplayWaitVblankStart
	STUB_FUNC	0xDEA197D4,sceDisplayGetMode
	STUB_FUNC	0xEEDA2E54,sceDisplayGetFrameBuf
	STUB_END
