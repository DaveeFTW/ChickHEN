	.set noreorder

#include "pspstub.s"

	STUB_START	"LoadExecForKernel",0x00090011,0x00010005
	STUB_FUNC	0xA3D5E142,sceKernelExitVSHVSH
	STUB_END
