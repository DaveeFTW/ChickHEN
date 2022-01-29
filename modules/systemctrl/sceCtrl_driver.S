	.set noreorder

#include "pspstub.s"

	STUB_START	"sceCtrl_driver",0x00090011,0x00010005
	STUB_FUNC	0x1F803938,sceCtrlReadBufferPositive
	STUB_END
