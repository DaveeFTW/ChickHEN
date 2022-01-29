/*
	5.03 ChickHEN vshctrl
	By Davee
*/

#include <pspkernel.h>
#include <pspctrl.h>

PSP_MODULE_INFO("VshControl", 0x1007, 1, 0);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0ffffffc) >> 2), a); 
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2)  & 0x03ffffff), a); 

void *version_hen = NULL;
wchar_t version[] = L"5.03 ChickHEN   ";

u8 verify_buffer[52000];
u8 tiff_md5_phat[16] = { 0xA7, 0xEA, 0x9F, 0xBA, 0x70, 0x83, 0xC7, 0x24, 0xB4, 0x42, 0xB3, 0x00, 0x24, 0x04, 0xA3, 0xBC };
u8 tiff_md5_slim[16] = { 0xF7, 0x7F, 0xF4, 0x76, 0x68, 0x32, 0x5D, 0x65, 0x1E, 0x51, 0xC2, 0xE9, 0x8A, 0x26, 0x3B, 0x11 };

void ClearCaches()
{
	sceKernelIcacheClearAll();
	sceKernelDcacheWritebackAll();
}

int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count)
{
	int res = sceCtrlReadBufferPositive(pad_data, count);
	sonySetK1();
	
	if ((menu_thid < 0) && (pad_data->Buttons & PSP_CTRL_SELECT))
	{
		menu_thid = sceKernelCreateThread("VSHMenu", menuGraphicsStart, 0x10, 0x1000, 0, NULL);
		
		if (menu_thid >= 0)
		{
			sceKernelStartThread(menu_thid, 0, NULL);	
			pad_data->Buttons &= ~PSP_CTRL_SELECT;
		}
	}
	
	else if (menu_thid >= 0)
	{
		menuCtrlReadBufferPositive(pad_data, count);
	}
	
	sonyRestoreK1();
	return res;
}

u32 network_patches[] =
{
	0x2B28, 0x2B6C, 0x2BB0, 0x2BF4,
	0x2C3C, 0x2C84, 0x2CCC, 0x2D14,
	0x2D5C, 0x2DA4, 0x2DEC, 0x2E34,
	0x2E7C, 0x2EC4, 0x2F0C, 0x2F54,
};

SceUID sceKernelLoadModuleVSHPatched(const char *path, int flags, SceKernelLMOption *option)
{
	SceUID res = sceKernelLoadModuleVSH(path, flags, option);
	
	if (res >= 0)
	{
		sonySetK1();
		SceModule2 *mod = sceKernelFindModuleByName("SceUpdateDL_Library");
		
		if (mod)
		{
			int i;	
			for (i = 0; i < sizeof(network_patches)/sizeof(u32); i++)
			{
				strcpy((char *)mod->text_addr+network_patches[i], "http://davee.x-fusion.co.uk/updatehen.txt");
			}
			
			ClearCaches();
		}
		
		sonyRestoreK1();
	}
	
	return res;
}

int VerifyFile(SceUID fd, u8 *check)
{
	u8 digest[16];	
	SceKernelUtilsMd5Context ctx;
	sceKernelUtilsMd5BlockInit(&ctx);
	
	while (1)
	{
		int read = sceIoRead(fd, verify_buffer, sizeof(verify_buffer));
		
		if (read <= 0)
		{
			break;
		}
		
		sceKernelUtilsMd5BlockUpdate(&ctx, verify_buffer, read);
	}
	
	sceKernelUtilsMd5BlockResult(&ctx, digest);
	
	if (memcmp(digest, check, 16) != 0)
	{
		return 1;
	}
	
	return 0;
}

SceUID sceIoOpenPatched(char *file, int flags, SceMode mode)
{
	SceUID ret, tiff = 0;
	sonySetK1();
	
	char *ext = strrchr(file, '.');
	
	if (ext)
	{
		if (strcmp(ext, ".tiff") == 0)
		{
			tiff = 1;
		}
	}
	
	sonyRestoreK1();	
	ret = sceIoOpen(file, flags, mode);	
	sonySetK1AGAIN();
	
	if (ret >= 0 && tiff)
	{
		u8 *check;
		
		if (psp_model == PSP_PHAT)
			check = tiff_md5_phat;
		
		else
			check = tiff_md5_slim;
		
		if (VerifyFile(ret, check) == 0)
		{
			sceIoClose(ret);
			ret = 0xDAEEDAEE;
		}
	}
	
	sonyRestoreK1();
	return ret;
}

void PatchVshmain(u32 text_addr)
{
	_sw(0, text_addr + 0xEE30);
	_sw(0, text_addr + 0xEE38);
	_sw(0x10000023, text_addr + 0xF0D8);
	
	SceModule2 *mod = sceKernelFindModuleByName("sceVshBridge_Driver");
	
	MAKE_CALL(mod->text_addr+0x0264, sceCtrlReadBufferPositivePatched);
	sctrlHENPatchSyscall(xfsFindFunction("sceController_Service", "sceCtrl", 0x1F803938, 0), sceCtrlReadBufferPositivePatched);
	
	if (config->patchupdater)
		MAKE_CALL(mod->text_addr + 0x10D4, sceKernelLoadModuleVSHPatched);
	
	if (config->patchtiff)
	{
		mod = sceKernelFindModuleByName("sceIOFileManager");
		sctrlHENPatchSyscall(mod->text_addr + 0x3CD0, sceIoOpenPatched);
	}
	
	int ver = typhoonGetVersion() & 0xF;
	
	if (ver)
	{
		version[14] = 'R';
		version[15] = '1' + ver;
	}
	
	SceUID block_id = sceKernelAllocPartitionMemory(2, "ChickHENRev", PSP_SMEM_Low, sizeof(version), NULL);
	
	if (block_id >= 0)
	{
		version_hen = sceKernelGetBlockHeadAddr(block_id);
		memcpy(version_hen, (void *)version, sizeof(version));
	}
	
	ClearCaches();
}

void PatchSysconf(u32 text_addr)
{
	if (version_hen)
	{
		_sw(0x3C020000 | ((u32)version_hen >> 16), text_addr + 0x15EE0);
		_sw(0x34420000 | ((u32)version_hen & 0xFFFF), text_addr + 0x15EE4);
		
		ClearCaches();
	}
}

void PatchGamePlugin(u32 text_addr)
{
	_sw(0x03E00008, text_addr + 0x11764);
	_sw(0x00001021, text_addr + 0x11768);

	if (config->hidecorrupt)
	{
		_sw(0x00601021, text_addr + 0xF7A8);
		_sw(0x00601021, text_addr + 0xF7B4);
	}
	
	ClearCaches();
}

void PatchUpdatePlugin(u32 text_addr)
{
	u32 cfw_version = typhoonGetVersion();
	
	_sw(0x3C050000 | (cfw_version >> 16), text_addr + 0x1F44);
	_sw(0x34A40000 | (cfw_version & 0xFFFF), text_addr + 0x210C);
	
	ClearCaches();
}

int OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;
		
	if (strcmp(modname, "vsh_module") == 0)
	{		
		PatchVshmain(text_addr);
	}
	
	else if (strcmp(modname, "sysconf_plugin_module") == 0)
	{
		PatchSysconf(text_addr);
	}
	
	else if (strcmp(mod->modname, "game_plugin_module") == 0)
	{
		PatchGamePlugin(text_addr);
	}
	
	else if (config->patchupdater && strcmp(mod->modname, "update_plugin_module") == 0)
	{
		PatchUpdatePlugin(text_addr);
	}
	
	if (!previous)
		return 0;
	
	return previous(mod);
}

int module_start(SceSize args, void *argp)
{
	config = xfsGetMemoryConfiguration();
	previous = xfsRegisterModuleStartHandler(OnModuleStart);
	return 0;
}