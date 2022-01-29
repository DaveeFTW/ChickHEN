/*
	Rebootex.bin
	Reversed by Davee
*/
#include <pspkernel.h>
#include "systemctrl.h"

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2)  & 0x03ffffff), a)
#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0ffffffc) >> 2), a)

typedef struct BtcnfHeader
{
	u32 signature; // 0
	u32 devkit;		// 4
	u32 unknown[2];  // 8
	u32 modestart;  // 0x10
	int nmodes;  // 0x14
	u32 unknown2[2];  // 0x18
	u32 modulestart; // 0x20
	int nmodules;  // 0x24
	u32 unknown3[2]; // 0x28
	u32 modnamestart; // 0x30
	u32 modnameend; // 0x34
	u32 unknown4[2]; // 0x38
}  __attribute__((packed)) BtcnfHeader;

typedef struct ModeEntry
{
	u16 maxsearch;
	u16 searchstart; //
	int mode1;
	int mode2;
	int reserved[5];
} __attribute__((packed)) ModeEntry;

typedef struct ModuleEntry
{
	u32 stroffset;
	int reserved;
	u16 flags;
	u8 loadmode;
	u8 signcheck;
	int reserved2;
	u8  hash[0x10];
} __attribute__((packed)) ModuleEntry;

enum BootLoadFlags
{
	BOOTLOAD_VSH = 1,
	BOOTLOAD_GAME = 2,
	BOOTLOAD_UPDATER = 4,
	BOOTLOAD_POPS = 8,
	BOOTLOAD_UMDEMU = 64, /* for original NP9660 */
};

enum PSP_MODELS
{
	PSP_PHAT,
	PSP_SLIM,
	PSP_3000,
};

int sctrl;
void (* clearIcache)(void) = (void *)0x886007C0;
void (* clearDcache)(void) = (void *)0x8860022C;

int (* sceIoLfatClose)(void) = NULL;
int (* sceIoLfatOpen)(char *file) = NULL;
int (* DecryptPSP)(void *buf, u32 size) = NULL;
int (* sceIoLfatRead)(void *buf, u32 size) = NULL;
int (* executeRebootBin)(a0, a1, a2, a3) = (void *)0x88600000;

int (* validateExecuteable)(u8 *buffer, void *a1) = NULL;
int (* decryptExecutable)(void *buffer, void *a1, void *decrypt_info) = NULL;

void ClearCaches()
{
	clearIcache();
	clearDcache();
}

void memcpy(void *dest, const void *src, unsigned int len)
{
	int i;
	
	for (i = 0; i < len; i++)
	{
		((char *)dest)[i] = ((char *)src)[i];
	}
}

void *memmove(void *vdest, const void *vsrc, size_t count)
{
	const char *src = vsrc;
	char *dest = vdest;
	
	if (dest <= src)
	{
		while (count--)
		{
			*dest++ = *src++;
        }
    }
	else
	{
		src  += count - 1;
		dest += count - 1;
		
		while(count--)
		{
			*dest-- = *src--;
		}
	}
	
	return vdest;
}


int memcmp(void *addr, void *addr2, u32 len)
{
	int i;
	
	for (i = 0; i < len; i++)
	{
		if (((char *)addr)[i] != ((char *)addr2)[i])
			break;
	}
	
	return len - i;
}

int sceIoLfatOpenPatched(char *file)
{
	if (memcmp(file, "/kd/systemctrl.prx", 19) == 0)
	{
		sctrl = 1;
		return 0;
	}
	
	return sceIoLfatOpen(file);
}

int sceIoLfatReadPatched(void *buf, u32 size)
{
	if (sctrl)
	{
		memcpy(buf, systemctrl, sizeof(systemctrl));
		return sizeof(systemctrl);
	}
	
	return sceIoLfatRead(buf, size);
}

int sceIoLfatClosePatched()
{
	if (sctrl)
	{
		sctrl = 0;
		return 0;
	}
	
	return sceIoLfatClose();
}

int DecryptPSPPatched(void *buf, u32 size)
{
	int i, x;
	int res = DecryptPSP(buf, size);
	
	BtcnfHeader *header = (BtcnfHeader *)buf;
	
	if (header->signature == 0x0F803001)
	{
		ModeEntry *modes = (ModeEntry *)((u32)header + header->modestart);
		ModuleEntry *modules = (ModuleEntry *)((u32)header + header->modulestart);
		char *names = (char *)((u32)header + header->modnamestart);

		for (i = 0; i < header->nmodules; i++)
		{
			if (memcmp(names+modules[i].stroffset, "/kd/init.prx", 13) == 0)
			{
				memmove(modules+i+1, modules+i, res-header->modulestart-(i*sizeof(ModuleEntry)));
				
				header->modnamestart += sizeof(ModuleEntry);
				header->modnameend += sizeof(ModuleEntry);
				res += sizeof(ModuleEntry);
				header->nmodules++;
				
				modules[i].stroffset = header->modnameend-header->modnamestart;
				modules[i].flags = (BOOTLOAD_VSH | BOOTLOAD_GAME | BOOTLOAD_POPS | BOOTLOAD_UPDATER | BOOTLOAD_UMDEMU);
				memcpy((char *)header + header->modnameend, "/kd/systemctrl.prx", 19);
				
				res += 19; //res += strlen("/kd/systemctrl.prx")+1;
				header->modnameend += 19; //header->modnameend += strlen("/kd/systemctrl.prx")+1;
				
				for (x = 0; x < header->nmodes; x++)
				{
					modes[x].searchstart = 0;
					modes[x].maxsearch++;
				}
			}
		}
	}
	
	return res;
}

/*int sceKernelCheckExecFilePatched(u32 *buffer, u32 *checks) //Superceeded by Memlmd patches
{
	int plain_elf = (buffer[0] == 0x464C457F); // elf
	if (plain_elf) 
	{
		if (checks[17] != 0)
		{
			checks[18] = 1;
			return 0;
		}
	}	

	int ret = sceKernelCheckExecFile(buffer, checks);

	if (plain_elf)
	{
		int index = checks[19];

		if (checks[19] < 0)
		{
			index += 3;
		}			

		if ((checks[2] == 0x20) || 
			((checks[2] > 0x20) && (checks[2] < 0x52)))		
		{
			if ((buffer[index / 4] & 0x0000FF00))
			{			
				checks[17] = 1;
				checks[22] = buffer[index / 4] & 0xFFFF;
				return 0;
			}			
		}		
	}
	
	return ret;
}*/

int DecryptExecuteablePatched(void *buffer, void *a1, void *decrypt_info)
{
	if (((u32 *)buffer)[0x130 / 4] == 0x6C696166) //Failsome HEN tag...
	{
		u32 size = ((u32 *)buffer)[0xB0 / 4];
		((u32 *)decrypt_info)[0] = size;
		memmove(buffer, (void *)((u32)buffer + 0x150), size);
		
		return 0;
	}
	
	return decryptExecutable(buffer, a1, decrypt_info);
}

int validateExecuteablePatched(u8 *buffer, void *a1)
{
	int i;
	
	for (i = 0; i < 88; i++)
	{
		if (buffer[212 + i])
		{
			return validateExecuteable(buffer, a1);
		}
	}
	
	return 0;
}

int OnLoadCoreModuleStart(void *a0, void *a1, void *a2, int (* module_start)(void *, void *, void *)) //updated for 5.00
{
	u32 text_addr = (u32)module_start - 0xC74;

	decryptExecutable = (void *)(text_addr + 0x81F0);
	validateExecuteable = (void *)(text_addr + 0x81D0);
	
	MAKE_CALL(text_addr + 0x41D0, DecryptExecuteablePatched);
	MAKE_CALL(text_addr + 0x68F8, DecryptExecuteablePatched);
	
	MAKE_CALL(text_addr + 0x691C, validateExecuteablePatched);
	MAKE_CALL(text_addr + 0x694C, validateExecuteablePatched);
	MAKE_CALL(text_addr + 0x69E4, validateExecuteablePatched);
	
	ClearCaches();
	return module_start(a0, a1, a2);
}


int entry(void *a0, void *a1, void *a2, void *a3) __attribute__ ((section (".text.start")));
int entry(void *a0, void *a1, void *a2, void *a3) //Updated for 5.00, values needing checked - confirmed values updated
{
	sctrl = 0;
	int psp_model = *(int *)0x88FC0004;
	
	if (psp_model == PSP_PHAT) //DONE
	{
		MAKE_CALL(0x88601F58, sceIoLfatOpenPatched); //Open
		MAKE_CALL(0x88601FC8, sceIoLfatReadPatched); //Read
		MAKE_CALL(0x88601FF4, sceIoLfatClosePatched); //Close
		
		//_sw(0xAFA50000, 0x88604F68);
		//_sw(0x20A30000, 0x88604F6C);
		/* Patch DecryptPSP to change btcnf file after decryption */
		_sw(0x4C9416F0, systemctrl + 0xD0);
		MAKE_CALL(0x886069D0, DecryptPSPPatched);
		
		_sw(0x03E00008, 0x88603018);
		_sw(0x24020001, 0x8860301C);
		
		_sw(0, 0x88601F50);
		_sw(0, 0x88601FA4);
		_sw(0, 0x88601FBC);
		
		_sw(0x00113821, 0x88604E28);
		MAKE_JUMP(0x88604E2C, OnLoadCoreModuleStart);
		_sw(0x02A0E821, 0x88604E30);
		
		_sw(0, 0x88606C68);
		
		sceIoLfatOpen = (void *)0x88607B10;
		sceIoLfatRead = (void *)0x88607C84;
		sceIoLfatClose = (void *)0x88607C28;
		DecryptPSP = (void *)0x88604F34;
	}
	else
	{
		/* Patch lfatfs functions */
		MAKE_CALL(0x88602020, sceIoLfatOpenPatched);
		MAKE_CALL(0x88602090, sceIoLfatReadPatched);
		MAKE_CALL(0x886020BC, sceIoLfatClosePatched);
		
		/* Patch DecryptPSP to allow plain bootconfigs */
		/* Not needed anymore -> the btcnf is encrypted */
		//_sw(0xAFA50000, 0x88605030);
		//_sw(0x20A30000, 0x88605034);
		
		/* Patch DecryptPSP to change btcnf file after decryption */
		
		if (psp_model == PSP_3000)
		{
			_sw(0x4C941FF0, systemctrl + 0xD0);
			MAKE_CALL(0x88606AA0, DecryptPSPPatched);
		}
		else
		{
			_sw(0x4C9417F0, systemctrl + 0xD0);
			MAKE_CALL(0x88606A90, DecryptPSPPatched);
		}
		
		_sw(0x03E00008, 0x886030E0);
		_sw(0x24020001, 0x886030E4);
		
		_sw(0, 0x88602018);
		_sw(0, 0x8860206C);
		_sw(0, 0x88602084);
		
		_sw(0x00113821, 0x88604EF0);
		MAKE_JUMP(0x88604EF4, OnLoadCoreModuleStart);
		_sw(0x02A0E821, 0x88604EF8);
		
		_sw(0, 0x88606D38);
		
		sceIoLfatOpen = (void *)0x88607BE0;
		sceIoLfatRead = (void *)0x88607D54;
		sceIoLfatClose = (void *)0x88607CF8;
		DecryptPSP = (void *)0x88604FFC;
	}

	ClearCaches();
	return executeRebootBin(a0, a1, a2, a3);
}