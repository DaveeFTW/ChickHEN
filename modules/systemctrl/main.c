#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspmscm.h>
#include <pspsysmem_kernel.h>
#include <psploadexec_kernel.h>
#include <pspthreadman_kernel.h>

#include <string.h>

#include "pspinit.h"
#include "memlmd.h"
#include "systemctrl.h"
#include "nidresolver.h"

PSP_MODULE_INFO("SystemControl", 0x7007, 1, 1);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0ffffffc) >> 2), a)
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2)  & 0x03ffffff), a)

#define MIPS_J(f) (0x08000000 | (((u32)(f) & 0x0ffffffc) >> 2))
#define REDIRECT_FUNC(a, f) _sw(MIPS_J(f), a); _sw(0, a + 4)
#define REDIRECT_IMPORT_FUNCTION(a, p, f) ((u32 *)f)[0] = _lw(a); ((u32 *)f)[2] = _lw(a + 4); ((u32 *)f)[1] = MIPS_J(a + 8); REDIRECT_FUNC(a, p)

#define sceKernelApplicationType InitForKernel_7233B5BC
//prototypes
int sceKernelCheckExecFile(int *buffer, int *checks);

//Function pointers
int (* ProbeExec1)(int *buffer, int *checks) = NULL;
int (* ProbeExec2)(int *buffer, int *checks) = NULL;
int (* PartitionCheck)(int *buffer, int *checks) = NULL;
int (* sfoRead)(SfoHeader *header, void *a1, void *a2) = NULL;
int (* ValidateExecuteable)(void *buffer, void *a1, void *decrypt_info) = NULL;
int (* DecompressReboot)(u32 addr, u32 size, void *unk, void *unk2, void *unk3) = NULL;
int (* DecryptExecuteable)(void *buffer, void *a1, void *decrypt_info, void *a3) = NULL;
int (* sceKernelLinkLibraryEntries)(SceLibraryStubTable *stub, u32 stub_size, int value, int value2) = NULL;
int (* msgledDecryptExec)(void *a0, void *a1, void *a2, void *buffer, void *t0, void *decrypt_info, void *t2, void *t3) = NULL;

//NandGuard
int ndSessionAllow = 0, ndSessionDisallow = 0;

int psp_model = 0;
u32 dword_1A60[64];
void *rebootex = NULL;
u32 rebootex_size = 0;
int slim_partition_2 = 0;
int slim_partition_8 = 0;
int current_selection = 0;
int finished = 0, ms_checked = 0;
SceUID modstart_thid = -1, menu_thid = -1, evid = -1;
SceModule2 *modstart_mod = NULL;
MODSTART_HANDLER modstart_registered = NULL;

ChickHENConfig config;
extern unsigned char msx[];
u32 pressed_buttons = 0, last_buttons = PSP_CTRL_SELECT, blitting = 0, temp_block = 0, isconfigflash1 = 0;

void ClearCaches()
{
	sceKernelIcacheClearAll();
	sceKernelDcacheWritebackAll();
}

int checkElfHeader(Elf32_Ehdr *header)
{
	if (header->e_magic == 0x464C457F && header->e_type == 2) //ELF
	{
		return 1;
	}
	
	return 0;
}

u32 typhoonGetVersion()
{
	return 0xDE000102;
}

void sctrlHENPatchSyscall(u32 addr, void *newaddr)
{
	u32 **cfc0, *vectors, i;
	__asm__ volatile ("cfc0 %0, $12\n" "nop\n" : "=r" (cfc0));
	
	vectors = *cfc0;
	
	for (i = 0; i < 0x1000; i++)
	{
		if (vectors[i + 4] == addr)
			vectors[i + 4] = (u32)newaddr;
	}
}

int sctrlKernelExitVSH(struct SceKernelLoadExecVSHParam *param)
{
	int ret;
	sonySetK1();
	
	ret = sceKernelExitVSHVSH(param);
	
	sonyRestoreK1();
	return ret;
}

void sctrlFlushConfigurationF1(ChickHENConfig *config)
{
	SceUID fd = sceIoOpen("flash1:/henconfig.bin", PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);
	
	if (fd >= 0)
	{
		sceIoWrite(fd, config, sizeof(ChickHENConfig));
		sceIoClose(fd);
	}
}

void sctrlFlushConfigurationMs(ChickHENConfig *config)
{
	SceUID fd = sceIoOpen("ms0:/henconfig.bin", PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);
	
	if (fd >= 0)
	{
		sceIoWrite(fd, config, sizeof(ChickHENConfig));
		sceIoClose(fd);
	}
}

static u32 adjust_alpha(u32 col)
{
	u32 alpha = col >> 24;
	u8 mul;
	u32 c1, c2;

	if(alpha == 0)
		return col;
	if(alpha == 0xFF)
		return col;

	c1 = col & 0x00ff00ff;
	c2 = col & 0x0000ff00;
	mul = (u8)(255 - alpha);
	c1 = ((c1 * mul) >> 8) & 0x00ff00ff;
	c2 = ((c2 * mul) >> 8) & 0x0000ff00;
	return (alpha << 24) | c1 | c2;
}

void sctrlGfxPutChar32(int x, int y, u32 colour, u32 bgcolour, u8 ch, u32 *vram, u32 bufferwidth)
{
	int i, j;
	u8 *font = &msx[ch * 8];	
	
	for (i = 0; i < 8; i++)
	{
		u8 font_2 = font[i];
		u32 offset = ((y + i) * bufferwidth) + x;
		
		if (i == 7)
			font_2 = 0;	
		
		for (j = 0; j < 8; j++)
		{
			u32 cur_colour;		
			if (font_2 & 0x80) //Draw pixel
			{
				cur_colour = colour;
			}
			else //draw background
			{
				cur_colour = bgcolour;
			}
			
			u32 alpha = cur_colour >> 24;
			
			if (alpha == 0)
				vram[offset] = cur_colour;
			
			else if (alpha != 0xFF)
			{
				u32 colour_1, colour_2 = vram[offset];
				
				colour_1 = colour_2 & 0x00FF00FF;
				colour_2 &= 0x0000FF00;
				
				colour_1 = ((colour_1 * alpha) >> 8) & 0x00FF00FF;
				colour_2 = ((colour_2 * alpha) >> 8) & 0x0000FF00;
				
				vram[offset] = (cur_colour & 0xFFFFFF) + colour_1 + colour_2;
			}
			
			offset++;
			font_2 <<= 1;
		}
	}
}

int sctrlGfxBlitString(int x, int y, u32 colour, u32 bgcolour, u32 *vram, u32 width, u32 bufferwidth, char *msg)
{
	int sx, len = strlen(msg);
	colour = adjust_alpha(colour);
	bgcolour = adjust_alpha(bgcolour);
	
	for (sx = 0; sx < len; sx++)
	{
		sctrlGfxPutChar32(x + (sx * 8), y, colour, bgcolour, msg[sx] & 0x7F, vram, bufferwidth);
	}
	
	return sx;
}

void sctrlGfxDrawRectangle(int x, int y, int xlen, int ylen, u32 colour, u32 *vram, u32 bufferwidth)
{
	int i, j;
	colour = adjust_alpha(colour);
	
	for (i = 0; i < ylen; i++)
	{
		u32 offset = ((y + i) * bufferwidth) + x;
		
		for (j = 0; j < xlen; j++)
		{		
			u32 alpha = colour >> 24;
			
			if (alpha == 0)
				vram[offset] = colour;
			
			else if (alpha != 0xFF)
			{
				u32 colour_1, colour_2 = vram[offset];
				
				colour_1 = colour_2 & 0x00FF00FF;
				colour_2 &= 0x0000FF00;
				
				colour_1 = ((colour_1 * alpha) >> 8) & 0x00FF00FF;
				colour_2 = ((colour_2 * alpha) >> 8) & 0x0000FF00;
				
				vram[offset] = (colour & 0xFFFFFF) + colour_1 + colour_2;
			}
			
			offset++;
		}
	}
}

enum VshmenuPSPModels
{
	vshPSP_PHAT = 1,
	vshPSP_SLIM = 2,
	vshPSP_3000 = 4,
	vshPSP_ALL = 7,
};

typedef struct
{
	char *title;
	int require_reboot;
	u32 psp_model;
	u32 options_n;
	char **options;
	int value;
	int (* vshCrossHandler)(void);
} VshmenuStructure;

int sctrlRebootChickHEN()
{
	return sctrlKernelExitVSH(NULL);
}

int vshGfxEnd()
{
	temp_block = 1;
	blitting = 0;
	menu_thid = -1;
	
	return sceKernelExitDeleteThread(0);
}

char *vshDisableEnabled[] =
{
	"Disabled",
	"Enabled",
};

char *vshConfigLocation[] =
{ 
	"MemoryStick",
	"flash1:",
};

char *vshFirmwareVersions[] =
{
	"Disabled",
	"5.00",
	"5.02"
};

VshmenuStructure menu_entries[] =
{
	{ "Hide TIFF in PHOTO", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Hide PIC1 Homebrew Icons", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Spoof System Settings Version", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Spoof Mac Address", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Redirect Network Updater", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Spoof firmware Version (In-Game)", 0, vshPSP_ALL, 3, vshFirmwareVersions, 0, NULL },
	{ "Spoof 3k to 2k (In-Game)", 0, vshPSP_3000, 2, vshDisableEnabled, 0, NULL },
	{ "Use NandGuard Technology", 1, vshPSP_ALL, 2, vshDisableEnabled, 0, NULL },
	{ "Store ChickHEN Config", 0, vshPSP_ALL, 2, vshConfigLocation, 0, NULL },
	{ "Reboot PSP", 1, vshPSP_ALL, 0, NULL, 0, sctrlRebootChickHEN },
	{ "Exit VSHMenu", 0, vshPSP_ALL, 0, NULL, 0, vshGfxEnd },
};
#define MENU_N (sizeof(menu_entries) / sizeof(VshmenuStructure))

int vshGfxStart(SceSize args, void *argp)
{
	blitting = 1;
	int i, model = 0;
	sceKernelChangeThreadPriority(0, 8);
	
	if (psp_model == PSP_PHAT)
		model = vshPSP_PHAT;
	
	else if (psp_model == PSP_SLIM)
		model = vshPSP_SLIM;
	
	else if (psp_model == PSP_3000)
		model = vshPSP_3000;
	
	while (blitting)
	{
		int align_y = 50;
		int pmode = 0, pwidth = 0, pheight = 0;
		int *vram32 = NULL, bufferwidth = 0, pixelformat = 0, colour = 0;
		
		sceDisplayWaitVblankStart();
		sceDisplayGetMode(&pmode, &pwidth, &pheight);
		sceDisplayGetFrameBuf((void *)&vram32, &bufferwidth, &pixelformat, 1);
		
		if (bufferwidth == 0 || pixelformat != PSP_DISPLAY_PIXEL_FORMAT_8888)
			continue;
		
		if (pressed_buttons & PSP_CTRL_UP)
		{
			while (1)
			{
				current_selection--;
				
				if (current_selection < 0)
					current_selection = MENU_N - 1;
				
				if (menu_entries[current_selection].psp_model & model)
					break;
			}
		}
		
		else if (pressed_buttons & PSP_CTRL_DOWN)
		{
			while (1)
			{
				current_selection++;
				
				if (current_selection >= MENU_N)
					current_selection = 0;
				
				if (menu_entries[current_selection].psp_model & model)
					break;
			}
		}
		
		else if ((pressed_buttons & PSP_CTRL_LEFT) && menu_entries[current_selection].options_n)
		{
			menu_entries[current_selection].value--;
			
			if (menu_entries[current_selection].value < 0)
				menu_entries[current_selection].value = menu_entries[current_selection].options_n - 1;
		}
		
		else if ((pressed_buttons & PSP_CTRL_RIGHT) && menu_entries[current_selection].options_n)
		{
			menu_entries[current_selection].value++;
			
			if (menu_entries[current_selection].value >= menu_entries[current_selection].options_n)
				menu_entries[current_selection].value = 0;
		}
		
		else if (pressed_buttons & PSP_CTRL_CROSS)
		{
			if (menu_entries[current_selection].vshCrossHandler)
				menu_entries[current_selection].vshCrossHandler();
		}
		
		else if (pressed_buttons & PSP_CTRL_SELECT || pressed_buttons & PSP_CTRL_CIRCLE)
		{	
			break;
		}
		
		sctrlGfxBlitString(192, 34, 0x00FFFFFF, 0x44DC459E, (u32 *)vram32, (u32)pwidth, (u32)bufferwidth, "ChickHEN VSHMenu");
		
		for (i = 0; i < MENU_N; i++)
		{
			if (!(menu_entries[i].psp_model & model))
			{
				align_y -= 8;
				i++;
			}
			
			if (i == current_selection)
				colour = 0x44FF5500;
			else
				colour = 0xAA000000; //0xFFDC9E45
			
			sctrlGfxBlitString(60, align_y + (i * 8), 0x00FFFFFF, colour, (u32 *)vram32, (u32)pwidth, (u32)bufferwidth, menu_entries[i].title);
			
			if (menu_entries[i].options_n)
				sctrlGfxBlitString(320, align_y + (i * 8), 0x00FFFFFF, colour, (u32 *)vram32, (u32)pwidth, (u32)bufferwidth, menu_entries[i].options[menu_entries[i].value]);
			
			if (menu_entries[current_selection].require_reboot)
			{
				sctrlGfxBlitString(44, 200, 0x00FFFFFF, 0x44DC459E, (u32 *)vram32, (u32)pwidth, (u32)bufferwidth, "Changing this Option will Initiate Reboot on Exit");
			}
		}
	}
	
	temp_block = 1;
	blitting = 0;
	menu_thid = -1;
	
	return sceKernelExitDeleteThread(0);
}

int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count)
{
	int res = sceCtrlReadBufferPositive(pad_data, count);
	sonySetK1();
	
	if (temp_block) ///This is called when the vshmenu is terminated
	{	
		if (!(pad_data->Buttons & pressed_buttons))
		{
			int i, reboot = 0, changes = 0;
			
			for (i = 0; i < MENU_N; i++)
			{
				if (menu_entries[i].options_n)
				{
					if (menu_entries[i].value != ((u32 *)&config)[i])
					{
						changes = 1;
						
						if (menu_entries[i].require_reboot)
							reboot = 1;
						
						((u32 *)&config)[i] = menu_entries[i].value;
					}
				}
				
			}
			
			if (changes)
			{
				if (config.configflash1)
					sctrlFlushConfigurationF1(&config);
				
				else
					sctrlFlushConfigurationMs(&config);
			
				if (isconfigflash1 && !config.configflash1)
				{
					sceIoRemove("flash1:/henconfig.bin");
				}
				
				isconfigflash1 = config.configflash1;
			}
			
			temp_block = 0;
			
			if (reboot)
				sctrlRebootChickHEN();
		}
		
		else
			pad_data->Buttons &= ~0x0081F3F9;
	}
	
	if ((pad_data->Buttons & (PSP_CTRL_RTRIGGER | PSP_CTRL_LTRIGGER | PSP_CTRL_TRIANGLE)) == (PSP_CTRL_RTRIGGER | PSP_CTRL_LTRIGGER | PSP_CTRL_TRIANGLE))
	{
		SceUID fd = sceIoOpen("ms0:/systemex.bin", PSP_O_RDONLY, 0777);
		
		rebootex_size = sceIoRead(fd, (void *)0x08800000, 100000);
		sceIoClose(fd);
		
		rebootex = (void *)(0x08800000);
		sctrlRebootChickHEN();
	}
	
	if ((menu_thid < 0) && (pad_data->Buttons & PSP_CTRL_SELECT))
	{
		menu_thid = sceKernelCreateThread("VSHMenu", vshGfxStart, 0x10, 0x4000, 0, NULL);
		
		if (menu_thid >= 0)
		{
			int i;
			current_selection = 0;
			
			for (i = 0; i < MENU_N; i++)
			{
				if (menu_entries[i].options_n)
					menu_entries[i].value = ((u32 *)&config)[i];
			}
			
			sceKernelStartThread(menu_thid, 0, NULL);
		}
	}
	
	if (blitting)
	{
		int i;
		pressed_buttons = pad_data->Buttons & ~last_buttons;
		last_buttons = pad_data->Buttons;
		
		for (i = 0; i < count; i++)
		{
			pad_data[i].Buttons &= ~0x0081F3F9;
		}
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
		SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("SceUpdateDL_Library");
		
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

void *version_hen = NULL;
void *macaddress_spoof = NULL;
//wchar_t version[] = L"5.03 ChickHEN   ";
wchar_t version[] = L"5.03 ChickHEN    DEBUG";

void PatchSysconf(u32 text_addr)
{
	if (version_hen || macaddress_spoof)
	{
		if (version_hen)
		{
			_sw(0x3C020000 | ((u32)version_hen >> 16), text_addr + 0x15EE0);
			_sw(0x34420000 | ((u32)version_hen & 0xFFFF), text_addr + 0x15EE4);
		}
		
		if (macaddress_spoof)
		{
			_sw(0x3C060000 | ((u32)macaddress_spoof >> 16), text_addr + 0x15C10);
			_sw(0x24C60000 | ((u32)macaddress_spoof & 0xFFFF), text_addr + 0x15C14);
		}
		
		ClearCaches();
	}
}

u8 verify_buffer[52000];
u8 tiff_md5_phat[16] = { 0xA7, 0xEA, 0x9F, 0xBA, 0x70, 0x83, 0xC7, 0x24, 0xB4, 0x42, 0xB3, 0x00, 0x24, 0x04, 0xA3, 0xBC };
u8 tiff_md5_slim[16] = { 0xF7, 0x7F, 0xF4, 0x76, 0x68, 0x32, 0x5D, 0x65, 0x1E, 0x51, 0xC2, 0xE9, 0x8A, 0x26, 0x3B, 0x11 };

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

wchar_t *sctrlGetUnicodeString(const char *file)
{
	u16 unicode;
	SceIoStat stat;
	memset(&stat, 0, sizeof(SceIoStat));
	
	if (sceIoGetstat(file, &stat) < 0)
		return NULL;
	
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	
	if (fd < 0)
		return NULL;
	
	sceIoRead(fd, &unicode, sizeof(u16));
	
	if (unicode != 0xFEFF)
	{
		unicode = 0;
		sceIoLseek(fd, 0, PSP_SEEK_SET);
		
		stat.st_size += stat.st_size + 2;
	}
	
	SceUID block_id = sceKernelAllocPartitionMemory(2, "", PSP_SMEM_Low, stat.st_size, NULL);
	
	if (block_id < 0)
		return NULL;
	
	char *global = sceKernelGetBlockHeadAddr(block_id);
	memset(global, 0, stat.st_size);
	
	if (unicode)
		sceIoRead(fd, global, stat.st_size - 2);
	
	else
	{
		int i;
		for (i = 0; i < (stat.st_size - 2); i += 2)
		{
			sceIoRead(fd, &global[i], 1);
		}
	}
	
	sceIoClose(fd);
	return (wchar_t *)global;
}
void DumpModuleList()
{
	int modid[100], count;
	memset(modid, 0, sizeof(modid));
	
	sceKernelGetModuleIdList(modid, sizeof(modid), &count);
	
	log("---ModuleList---\n");
	log("%i module(s) found\n", count);
	
	int i;
	for(i = 0; i < count; i++)
	{
		SceModule2 *mod = sceKernelFindModuleByUID(modid[i]);
		if (mod)
		{
			log("Module ID 0x%08x\nName %s\n", modid[i], mod->modname);
		}
	}
	
	log("---END---\n\n");
}

void PatchVshmain(u32 text_addr)
{
	_sw(0, text_addr + 0xEE30);
	_sw(0, text_addr + 0xEE38);
	_sw(0x10000023, text_addr + 0xF0D8);
	
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceVshBridge_Driver");
	
	if (config.usehenupdater)
		MAKE_CALL(mod->text_addr + 0x10D4, sceKernelLoadModuleVSHPatched);
	
	MAKE_CALL(mod->text_addr + 0x0264, sceCtrlReadBufferPositivePatched);
	sctrlHENPatchSyscall(xfsFindFunction("sceController_Service", "sceCtrl", 0x1F803938, 0), sceCtrlReadBufferPositivePatched);
	
	if (config.hidetiff)
	{
		mod = (SceModule2 *)sceKernelFindModuleByName("sceIOFileManager");
		sctrlHENPatchSyscall(mod->text_addr + 0x3CD0, sceIoOpenPatched);
	}
	
	if (!config.spoofversion)
	{
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
	}
	else
	{
		version_hen = sctrlGetUnicodeString("ms0:/ChickHEN/version_spoof.txt");
	}
	
	if (config.spoofmac)
		macaddress_spoof = sctrlGetUnicodeString("ms0:/ChickHEN/mac_spoof.txt");
	
	DumpModuleList();
	ClearCaches();
}

char name[128];
char str[256];
int logStats = 0;

void logInfo(char *msg)
{
	logStats = 1;
	SceUID fd = sceIoOpen("ms0:/log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
	sceIoWrite(fd, msg, strlen(msg));
	
	sceIoClose(fd);
	logStats = 0;
}

int DecompressRebootPatched(u32 addr, u32 size, void *unk, void *unk2, void *unk3)
{
	memcpy((void *)0x88FB0000, rebootex, rebootex_size);
	memcpy((void *)0x88FC0020, (void *)&config, sizeof(ChickHENConfig));

	*(int *)0x88FC0000 = rebootex_size;
	*(int *)0x88FC0004 = psp_model;
	*(int *)0x88FC0008 = slim_partition_2;
	*(int *)0x88FC0010 = slim_partition_8;
	
	return DecompressReboot(addr, size, unk, unk2, unk3);
}

void PatchLoadExec(u32 text_addr)
{	
	DecompressReboot = (void *)text_addr;
	MAKE_CALL(text_addr + 0x27DC, DecompressRebootPatched);
	_sw(0x3C0188FB, text_addr + 0x00002820);
	
	_sw(0x1000000B, text_addr + 0x1EA8);
	_sw(0, text_addr + 0x1EE8);
	
	_sw(0x10000008, text_addr + 0x1494);
	_sw(0, text_addr + 0x14C8);
	
	ClearCaches();
}

int msgledDecryptExecPatched(void *a0, void *a1, void *a2, void *buffer, void *t0, void *decrypt_info, void *t2, void *t3)
{
	if (a0 && buffer && decrypt_info)
	{
		u32 oe_tag = ((u32 *)(buffer + 0x130))[0];
		
		if (oe_tag == 0x28796DAA || oe_tag == 0x7316308C || oe_tag == 0x3EAD0AEE || oe_tag == 0x8555ABF2)
		{
			if ((((u8 *)(buffer + 0x150))[0] == 0x1F) && (((u8 *)(buffer + 0x150))[1] == 0x8B))
			{
				*(int *)decrypt_info = ((u32 *)buffer)[0xB0/4];
				memmove(buffer, (void *)(buffer + 0x150), *(int *)decrypt_info);
				return 0;
			}
		}
	}
	
	int res = msgledDecryptExec(a0, a1, a2, buffer, t0, decrypt_info, t2, t3);
	
	if (res < 0)
	{
		if (validateExecuteablePatched(buffer, t0, t2) >= 0)
		{
			return msgledDecryptExec(a0, a1, a2, buffer, t0, decrypt_info, t2, t3);
		}
	}
	
	return res;
}

typedef struct
{
	u32 addr1;
	u32 addr2;
	u32 addr3;
	u32 addr4;
	u32 addr5;
} msgled_patches_struct;

msgled_patches_struct mesgled_patches[3] = 
{
	{ 0x00001878, 0x00001DAC, 0x00003418, 0x000037D4, 0x00001E7C }, //phat
	{ 0x00001900, 0x00001EC4, 0x00003920, 0x00003D6C, 0x00001F54 }, //slim
	{ 0x00001900, 0x00002274, 0x00003E00, 0x0000424C, 0x00001F54 }, //3000
};

void PatchMesgLed(u32 text_addr)
{
	msgledDecryptExec = (void *)(text_addr + 0xE0);
	msgled_patches_struct *patches = &mesgled_patches[psp_model];
		
	MAKE_CALL(text_addr + patches->addr1, msgledDecryptExecPatched);
	MAKE_CALL(text_addr + patches->addr2, msgledDecryptExecPatched);
	MAKE_CALL(text_addr + patches->addr3, msgledDecryptExecPatched);
	MAKE_CALL(text_addr + patches->addr4, msgledDecryptExecPatched);
	MAKE_CALL(text_addr + patches->addr5, msgledDecryptExecPatched);
	
	ClearCaches();
}

void PatchUmdCache(u32 text_addr)
{
	int i;
	
	if (sceKernelApplicationType() == 0x200 && sceKernelBootFrom() == 0x40)
	{
		_sw(0x03E00008, text_addr + 0x0B94);
		_sw(0x24020001, text_addr + 0x0B98);
		
		ClearCaches();
		
		for (i = 0xBC000040; i < 0xBC000080; i += 4)
		{
			((u32 *)i)[0] = 0xFFFFFFFF;
		}
	}
}

void PatchGamePlugin(u32 text_addr)
{
	_sw(0x03E00008, text_addr + 0x11764);
	_sw(0x00001021, text_addr + 0x11768);
	
	if (config.hidecorrupt)
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

int sctrlHENSetMemory(u32 p2, u32 p8)
{
	if (psp_model == PSP_PHAT)
		return -1;
	
	if (p2 == 0 || (p2 + p8) >= 53)
		return 0x80000107;
	
	sonySetK1();
	
	slim_partition_2 = p2;
	slim_partition_8 = p8;
	
	sonyRestoreK1();
	return 0;
}

void SystemCtrlForKernel_B86E36D1()
{
	int p2 = slim_partition_2;
	int p8 = slim_partition_8;
	
	if (p2 != 0 && (p2 + p8) < 53)
	{
		int *showvalue;
		int *(* SysmemFunc)(SceUID pid) = (void *)0x88003660;
		int *ret = SysmemFunc(2);
		
		int megabytes_p2 = p2 * 1024 * 1024;
		ret[2] = megabytes_p2;
		
		showvalue = (int *)ret[4];
		showvalue[5] = (megabytes_p2 << 1) | 0xFC;
		
		ret = SysmemFunc(8);
		showvalue = (int *)ret[4];
		
		int megabytes_p8 = p8 * 1024 * 1024;
		
		ret[1] = 0x88800000 + megabytes_p2;
		ret[2] = megabytes_p8;
		
		showvalue[5] = (megabytes_p8 << 1) | 0xFC;
	}
}

int sfoReadPatched(SfoHeader *header, void *a1, void *a2)
{
	int i, mempart = 0;
	SfoEntry *entry;
	
	if (slim_partition_2 == 0)
	{
		entry = (SfoEntry *)((u32)header + 20);
		
		for (i = 0; i < header->count; i++)
		{
			if (strcmp((char *)((u32)header + header->keyofs + (int)entry->nameofs), "MEMSIZE") == 0)
			{
				memcpy(&mempart, (void *)((u32)header + header->valofs + (int)entry->valofs), 4);
			}
			
			entry = (SfoEntry *)((u32)entry + 16);
		}
		
		if (mempart)
		{
			sctrlHENSetMemory(52, 0);
			SystemCtrlForKernel_B86E36D1();
		}
	}
	
	else
	{
		SystemCtrlForKernel_B86E36D1();
		slim_partition_2 = 0;
	}
	
	return sfoRead(header, a1, a2);
}

void PatchMediaSync(u32 text_addr)
{
	char *filename = sceKernelInitFileName();
	
	if (filename && strstr(filename, ".PBP"))
	{
		_sw(0x00008021, text_addr + 0x0628);
		_sw(0x00008021, text_addr + 0x0528);
		
		if (psp_model != PSP_PHAT)
		{
			sfoRead = (void *)(text_addr + 0x860);
			MAKE_CALL(text_addr + 0x061C, sfoReadPatched);
		}
		
		if (config.spoof_firmware)
		{
			u32 devkit = DEVKIT_STR_CONV(vshFirmwareVersions[config.spoof_firmware]);		
			u32 res = xfsFindFunction("sceSystemMemoryManager", "SysMemUserForUser", 0x3FC9AE6A, 0);
			
			if (res)
			{
				_sw(0x3C030000 | (devkit >> 16), res);
				_sw(0x34620000 | (devkit & 0xFFFF), res + 8);
			}
		}
		
		ClearCaches();
	}
}

void PatchWlan(u32 text_addr)
{
	_sw(0, text_addr + 0x25F4);
	ClearCaches();
}

char *nandguard_notifications[] =
{
	"Allow Access",
	"Disallow Access",
	"Allow Access this session",
	"Disallow Access this session",
};

int ndGetValidateResult()
{
	if (!ndSessionAllow)
	{
		if (ndSessionDisallow)
			return 0xDAEEDAEE;
		
		//int ret = sctrlActivateNotification("NandGuard: Software has attempted to write to NAND.", NOTIFICATION_CUSTOM, 4, nandguard_notifications, ngWriteVerify);
		
		//if (ret < 0)
		//	return ret;
	}
	
	return 0;
}

u32 fake_imports[15];

int (* sceNandWriteAccess)(u32 ppn, void *user, void *spare, u32 len, void *mode) = (void *)fake_imports;
int (* sceNandWriteBlock)(u32 ppn, void *user, void *spare) = (void *)&fake_imports[3];
int (* sceNandWriteBlockWithVerify)(u32 ppn, void *user, void *spare) = (void *)&fake_imports[6];
int (* sceNandEraseBlockWithRetry)(u32 ppn) = (void *)&fake_imports[9];
int (* sceNandEraseBlock)(u32 ppn) = (void *)&fake_imports[12];

int sceNandWriteAccessPatched(u32 ppn, void *user, void *spare, u32 len, void *mode)
{
	int ret = ndGetValidateResult();
	
	if (ret < 0)
		return ret;
	
	return sceNandWriteAccess(ppn, user, spare, len, mode);
}

int sceNandWriteBlockPatched(u32 ppn, void *user, void *spare)
{
	int ret = ndGetValidateResult();
	
	if (ret < 0)
		return ret;
	
	return sceNandWriteBlock(ppn, user, spare);
}

int sceNandWriteBlockWithVerifyPatched(u32 ppn, void *user, void *spare)
{
	int ret = ndGetValidateResult();
	
	if (ret < 0)
		return ret;
	
	return sceNandWriteBlockWithVerify(ppn, user, spare);
}

int sceNandEraseBlockWithRetryPatched(u32 ppn)
{
	int ret = ndGetValidateResult();
	
	if (ret < 0)
		return ret;
	
	return sceNandEraseBlockWithRetry(ppn);
}

int sceNandEraseBlockPatched(u32 ppn)
{
	int ret = ndGetValidateResult();
	
	if (ret < 0)
		return ret;
	
	return sceNandEraseBlock(ppn);
}

int (* lflashIoOpen)(PspIoDrvFileArg *arg, char *file, int flags, SceMode mode) = NULL;
int lflashIoOpenPatched(PspIoDrvFileArg *arg, char *file, int flags, SceMode mode)
{
	char buffer[256];
	
	if (arg->fs_num == 0 && (flags & PSP_O_WRONLY) && !ndSessionAllow)
	{
		if (ndSessionDisallow)
			return 0xDAEEDAEE;
		
		/*sprintf(buffer, "NandGuard: Software has attempted to open %s with write permissions (0x%08x).", file, flags);
		int ret = sctrlActivateNotification(buffer, NOTIFICATION_CUSTOM, 4, nandguard_notifications, ngWriteVerify);
		
		if (ret < 0)
			return ret;*/
	}
	
	return lflashIoOpen(arg, file, flags, mode);
}

void PatchLowIO(u32 text_addr)
{
	REDIRECT_IMPORT_FUNCTION(text_addr + 0x89BC, sceNandWriteAccessPatched, sceNandWriteAccess);
	REDIRECT_IMPORT_FUNCTION(text_addr + 0x9784, sceNandWriteBlockWithVerifyPatched, sceNandWriteBlockWithVerify);
	REDIRECT_IMPORT_FUNCTION(text_addr + 0x9A0C, sceNandEraseBlockWithRetryPatched, sceNandEraseBlockWithRetry);
	REDIRECT_IMPORT_FUNCTION(text_addr + 0x8B80, sceNandEraseBlockPatched, sceNandEraseBlock);
	
	PspIoDrv *lflashDriver = sctrlHENFindDriver("flashfat");
	lflashIoOpen = lflashDriver->funcs->IoOpen;
	
	int intr = sceKernelCpuSuspendIntr();

	lflashDriver->funcs->IoOpen = lflashIoOpenPatched;
	
	sceKernelCpuResumeIntr(intr);
	ClearCaches();
}

int OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;
	
	if (strcmp(modname, "sceLoadExec") == 0)
	{
		PatchLoadExec(text_addr);
	}
	
	else if (strcmp(modname, "sceMesgLed") == 0)
	{
		PatchMesgLed(text_addr);
	}
	
	else if (config.usenandguard && strcmp(modname, "sceLowIO_Driver") == 0)
	{
		//PatchLowIO(text_addr);
	}
	
	else if (strcmp(modname, "sceWlan_Driver") == 0)
	{
		PatchWlan(text_addr);
	}
	
	else if (strcmp(modname, "sceMediaSync") == 0)
	{
		PatchMediaSync(text_addr);
	}
	
	else if (strcmp(modname, "sceUmdCache_driver") == 0)
	{
		PatchUmdCache(text_addr);
	}
	
	else if (strcmp(modname, "sysconf_plugin_module") == 0)
	{
		PatchSysconf(text_addr);
	}
	
	else if (strcmp(modname, "vsh_module") == 0)
	{		
		PatchVshmain(text_addr);
	}
	
	else if (strcmp(mod->modname, "game_plugin_module") == 0)
	{
		PatchGamePlugin(text_addr);
	}
	
	else if (config.usehenupdater && strcmp(mod->modname, "update_plugin_module") == 0)
	{
		PatchUpdatePlugin(text_addr);
	}
	
	if (modstart_registered)
		return modstart_registered(mod);
	
	return 0;
}

MODSTART_HANDLER xfsRegisterModuleStartHandler(MODSTART_HANDLER handler)
{
	MODSTART_HANDLER old = modstart_registered;
	modstart_registered = (MODSTART_HANDLER)((u32)handler | 0x80000000);
	
	return old;
}

SceUID sceKernelCreateThreadPatched(const char *name, SceKernelThreadEntry entry, int initPriority, int stackSize, SceUInt attr, void *option)
{
	SceUID ret = sceKernelCreateThread(name, entry, initPriority, stackSize, attr, option);
	
	if (ret >= 0)
	{
		modstart_thid = ret;
		modstart_mod = sceKernelFindModuleByAddress((u32)entry);
	}
	
	return ret;
}

int sceKernelStartThreadPatched(SceUID thid, SceSize args, void *argp)
{
	if (thid == modstart_thid && modstart_mod)
	{
		OnModuleStart(modstart_mod);
	}
	
	return sceKernelStartThread(thid, args, argp);
}

int kuKernelGetModel() //0x24331850
{
	int model;
	sonySetK1();
	
	model = psp_model;
	
	if (config.spoof3k && model == PSP_3000)
		model--;
	
	sonyRestoreK1();
	return model;
}

int kuKernelSetDdrMemoryProtection(void *addr, int size, int prot) //0xC4AF12AB
{
	int ret;
	sonySetK1();
	
	ret = sceKernelSetDdrMemoryProtection(addr, size, prot);
	
	sonyRestoreK1();
	return ret;
}

int kuKernelGetUserLevel() //0xA2ABB6D3
{
	int ret;
	sonySetK1();
	
	ret = sceKernelGetUserLevel();
	
	sonyRestoreK1();
	return ret;
}

int kuKernelBootFrom() //0x60DDB4AE
{
	return sceKernelBootFrom();
}

char *KUBridge_1742445F(char *file) //0x1742445F
{
	char *ret;
	sonySetK1();
	
	ret = sceKernelInitFileName(NULL);
	strcpy(file, ret);
	
	sonyRestoreK1();
	return ret;
}

int kuKernelInitApitype() //0x8E5A4057
{
	return sceKernelInitApitype();
}

SceUID kuKernelLoadModule(const char *file, int setzero, SceKernelLMOption *options) //0x4C25EA72
{
	SceUID ret;
	sonySetK1();
	
	ret = sceKernelLoadModule(file, setzero, options);
	
	sonyRestoreK1();
	return ret;
}

int sctrlKernelSetUserLevel(int level)
{
	int prev_level;
	sonySetK1();
	
	prev_level = sceKernelGetUserLevel();
	
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceThreadManager");
	u32 *userlevel = (u32 *)_lw(mod->text_addr + 0x18D40);
	
	userlevel[5] = ((level ^ 0x8) << 28);
	sonyRestoreK1();
	return prev_level;
}

PspIoDrv *sctrlHENFindDriver(char *drvname)
{
	PspIoDrv *ret = NULL;
	sonySetK1();
	
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceIOFileManager");
	u32 *(* IoFileMgrGetDriver)(char *drvname) = (void *)(mod->text_addr + 0x2838);
	
	u32 *driver = IoFileMgrGetDriver(drvname);
	
	if (driver)
		ret = (PspIoDrv *)driver[1];
	
	sonyRestoreK1();
	return ret;
}

int PatchExec3(int *buffer, int *checks)
{
	int index = checks[19];
	
	if (index < 0)
		index += 3;
	
	u32 addr = (u32)(buffer + index);
	if (addr >= 0x88400000 && addr <= 0x88800000)
	{
		return 0;
	}
	
	checks[22] = buffer[index / 4] & 0xFFFF;
	return buffer[index / 4];
}

int PatchExec2(Elf32_Ehdr *buffer, int *checks, int isplain, int checkexec_ret)
{
	if (isplain)
	{
		if (checks[2] < 82)
		{	
			if (PatchExec3((int *)buffer, checks) & 0xFF00)
			{
				checks[17] = 1;
				return 0;
			}
		}
		
		else if (buffer->e_magic == 0x464C457F && buffer->e_type == 2)
		{
			checks[8] = 3;
		}
	}
	
	return checkexec_ret;
}

int PatchExec1(int *buffer, int *checks)
{
	if (buffer[0] != 0x464C457F)
		return -1;
	
	if (checks[2] >= 288)
	{
		if ((checks[2] != 288) &&
			(checks[2] != 321) &&
			(checks[2] != 322) &&
			(checks[2] != 323) &&
			(checks[2] != 320)
		)
		{
			return -1;
		}
		
		if (checks[4] == 0)
		{
			if (checks[17] == 0)
				return -1;
			
			checks[18] = 1;
		}
		else
		{
			checks[17] = 1;
			checks[18] = 1;
			
			PatchExec3(buffer, checks);
		}
		
		return 0;
	}
	
	else if (checks[2] >= 82)
		return -1;
	
	if (checks[17] == 0)
		return -2;
	
	checks[18] = 1;
	return 0;
}

int sceKernelCheckExecFilePatched(int *buffer, int *checks)
{
	int ret = PatchExec1(buffer, checks);
	
	if (!ret)
		return ret;
	
	return PatchExec2((Elf32_Ehdr *)buffer, checks, buffer[0] == 0x464C457F, sceKernelCheckExecFile(buffer, checks));
}

int ProbeExec1Patched(int *buffer, int *checks)
{
	int ret = ProbeExec1(buffer, checks);
	
	if (buffer[0] != 0x464C457F)
		return ret;
	
	u16 attribute = *(u16 *)((u32)buffer + checks[19]);
	
	if (attribute & 0x1E00)
	{
		if ((*(u16 *)((u32)checks + 88) & 0x1E00) != (attribute & 0x1E00))
		{
			*(u16 *)((u32)checks + 88) = attribute;
		}
	}
	
	if (checks[18] == 0)
		checks[18] = 1;
	
	return ret;
}

char *retrieveStrtab(Elf32_Ehdr *header)
{
	int i;
	char *data = (char *)((u32)header + header->e_shoff);

	for (i = 0; i < header->e_shnum; i++)
	{
		if (header->e_shstrndx == i)
		{
			Elf32_Shdr *section = (Elf32_Shdr *)data;
			
			if (section->sh_type == 3)
				return (char *)((u32)header + section->sh_offset);
		}
		
		data += header->e_shentsize;
	}

	return NULL;
}	
	
int ProbeExec2Patched(int *buffer, int *checks)
{
	int ret = ProbeExec2(buffer, checks);

	if (checkElfHeader((Elf32_Ehdr *)buffer))
	{
		if (checks[2] >= 0x140 && checks[2] < 0x145)
			checks[2] = 0x120;
		
		if (checks[19] == 0)
		{
			char *strtab = retrieveStrtab((Elf32_Ehdr *)buffer);
			
			if (strtab)
			{
				int i;
				Elf32_Ehdr *header = (Elf32_Ehdr *)buffer;
				char *data = (char *)((u32)buffer + header->e_shoff);

				for (i = 0; i < header->e_shnum; i++)
				{
					Elf32_Shdr *section = (Elf32_Shdr *)data;
					
					if (strcmp(strtab + section->sh_name, ".rodata.sceModuleInfo") == 0)
					{
						checks[19] = section->sh_offset;
						checks[22] = 0;
					}
					
					data += header->e_shentsize;
				}
			}
		}	
	}

	return ret;
}

int PartitionCheckPatched(int *buffer, int *checks)
{
	SceUID fd = (SceUID)buffer[13];
	
	if (fd < 0)
		return PartitionCheck(buffer, checks);

	u32 seek = sceIoLseek(fd, 0, PSP_SEEK_CUR);

	if (seek < 0)
		return PartitionCheck(buffer, checks);

	sceIoLseek(fd, 0, PSP_SEEK_SET);
	
	if (sceIoRead(fd, dword_1A60, sizeof(dword_1A60)) < sizeof(dword_1A60))
	{
		sceIoLseek(fd, seek, PSP_SEEK_SET);
		return PartitionCheck(buffer, checks);
	}

	if (dword_1A60[0] == 0x50425000) // PBP
	{
		sceIoLseek(fd, dword_1A60[8], PSP_SEEK_SET);
		sceIoRead(fd, dword_1A60, 0x14);

		if (dword_1A60[0] != 0x464C457F) // ELF 
		{
			sceIoLseek(fd, seek, PSP_SEEK_SET);
			return PartitionCheck(buffer, checks);
		}

		sceIoLseek(fd, dword_1A60[8] + checks[19], PSP_SEEK_SET);

		if (!checkElfHeader((Elf32_Ehdr *)dword_1A60))
		{
			checks[4] = dword_1A60[9] - dword_1A60[8];
		}
	}
	else if (dword_1A60[0] == 0x464C457F) // ELF 
	{
		sceIoLseek(fd, checks[19], PSP_SEEK_SET);
	}
	else
	{
		sceIoLseek(fd, seek, PSP_SEEK_SET);
		return PartitionCheck(buffer, checks);
	}

	u16 attributes;
	sceIoRead(fd, &attributes, sizeof(u16));

	if (checkElfHeader((Elf32_Ehdr *)dword_1A60))
	{
		checks[17] = 0;
	}
	else
	{
		if (attributes & 0x1000)
		{
			checks[17] = 1;
		}
		else
		{
			checks[17] = 0;
		}
	}

	sceIoLseek(fd, seek, PSP_SEEK_SET);
	return PartitionCheck(buffer, checks);
}

u32 xfsResolveNidtable(NidResolverLib *resolver, u32 nid)
{
	int i;
	
	for (i = 0; i < resolver->nids_n; i++)
	{
		if (resolver->nidtable[i].old_nid == nid)
			return resolver->nidtable[i].new_nid;
	}
	
	return nid;
}

NidResolverLib *xfsNidTable(const char *lib)
{
	int i;
	
	for (i = 0; i < RESOLVER_LIBRARIES_N; i++)
	{
		if (strcmp(RESOLVER_LIBRARIES[i].libname, lib) == 0)
		{
			return &RESOLVER_LIBRARIES[i];
		}
	}
	
	return NULL;
}

u32 xfsResolveNid(const char *lib, u32 nid)
{
	NidResolverLib *resolver = xfsNidTable(lib);
	
	if (resolver)
	{
		nid = xfsResolveNidtable(resolver, nid);
	}
	
	return nid;
}

u32 xfsFindFunction(const char *modname, const char *lib, u32 nid, u32 patchfunction)
{
	int i = 0, u;
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName(modname);
	
	if (!mod)
	{
		mod = sceKernelFindModuleByAddress((u32)modname);
		
		if (!mod)
			return (u32)mod;
	}
	
	if (lib)
		nid = xfsResolveNid(lib, nid);
	
	int entry_size = mod->ent_size;
	int entry_start = (int)mod->ent_top;
	
	while (i < entry_size)
	{
		SceLibraryEntryTable *entry = (SceLibraryEntryTable *)(entry_start + i);
		
		if (entry->libname && (strcmp(entry->libname, lib) == 0))
		{
			u32 *table = entry->entrytable;
			int total = entry->stubcount + entry->vstubcount;
			
			if (total > 0)
			{ 
				for (u = 0; u < total; u++)
				{ 
					if (table[u] == nid)
					{
						u32 func_addr = table[u + total];
						
						if (patchfunction)
							table[u + total] = patchfunction;
						
						return func_addr;
					}
				} 
			} 	
		}
		
		i += (entry->len * 4);
	}
	
	return 0;
}

u32 sctrlHENFindFunction(const char* szMod, const char* szLib, u32 nid)
{
	return xfsFindFunction(szMod, szLib, nid, 0);
}

int sceKernelLinkLibraryEntriesPatched(SceLibraryStubTable *stub_start, u32 stubsize)
{
	u16 version = 0;
	u32 i = 0, j = 0;
	u32 ret = xfsFindFunction((char *)stub_start, NULL, 0x11B97506, 0);
	
	if (ret)
	{
		version = *(u16 *)(ret + 2);
	}
	
	if (version != 0x503)
	{
		while (i < stubsize)
		{
			SceLibraryStubTable *stub = (SceLibraryStubTable *)((u32)stub_start + i);
			
			if (stub->libname)
			{
				u32 *table = (u32 *)stub->nidtable;
				
				if (stub->stubcount > 0)
				{
					NidResolverLib *nidresolver = xfsNidTable(stub->libname);
					
					if (nidresolver)
					{
						for (j = 0; j < stub->stubcount; j++)
						{
							table[j] = xfsResolveNidtable(nidresolver, table[j]);
						}
					}
				} 	
			}
			
			i += (stub->len * 4);
		}
		
		ClearCaches();
	}
	
	return sceKernelLinkLibraryEntries(stub_start, stubsize, 0, 0);
}

int dword_A878[9];
int MsCheck(int arg1, int arg2, void *arg)
{
	sceKernelSetEventFlag(evid, 1);
	ms_checked = arg2;
	return 0;
}

void LoadStartModule(const char *file)
{
	SceUID modid = sceKernelLoadModule(file, 0, NULL);
	
	if (modid >= 0)
	{
		sceKernelStartModule(modid, strlen(file)+1, (void *)file, NULL, NULL);
	}
}

/*void FixString(char *p)
{
	int len = strlen(p)-1;
	
	while (len > 0)
	{
		if (p[len] == 0x09 || p[len] == 0x20)
			p[len] = 0;
		else
			break;
	}
}*/

void FixString(char *str)
{
	int len;
	
	for (len = strlen(str) - 1; len >= 0; len--)
	{
		if (str[len] == 0x20 || str[len] == 0x09)
			str[len] = 0;
		
		else
			break;
	}
}

int GetModule(char *str, int size, char *module, int *active)
{
	int i, j, x;
	
	for (i = 0, j = 0; i < size; i++)
	{
		if (str[i] >= 0x20 || str[i] == 0x9)
		{
			module[j++] = str[i];
		}
		
		else if (j != 0)
			break;
	}
	
	FixString(module);
	active[0] = 0;
	
	if (i > 0)
	{
		char *s = strpbrk(module, "	 ");
		
		if (s)
		{
			x = 1;		
			while (s[x++] < 0);
				
			if (strcmp(&s[x], "1") == 0)
			{
				*active = 1;
			}
			
			s[0] = 0;
		}
	}
	
	return i;
}

/*int (char *p, int size, char *module, int *active)
{
	int i, x;
	
	for (i = 0 , x = 0; i < size; i++)
	{
		if (p[i] < 0x20 || p[i] == 0x09)
		{
			module[x++] = p[i];
		}
		else
		{
			if (x)	
				break;
		}
	}
	
	FixString(module);
	*active = 0;
	
	if (i > 0)
	{
		char *p = strpbrk(module, "	 "); // 1 tab & 1 space
		
		if (p)
		{
			for (x = 1; (p[x] < 0); x++);
		
			if (strcmp(&p[x], "1") == 0)
			{
				*active = 1;
			}
			
			p[0] = 0;
		}
	}
	
	return i;
}*/

char *getLine(char *buffer, int buffer_size)
{
	int i;
	
	for (i = 0; i < buffer_size; i++)
	{
		if (buffer[i] == 0x0A || buffer[i] == 0x0D)
		{
			buffer[i] = 0;
			
			if (buffer[i + 1] == 0x0A || buffer[i + 1] == 0x0D)
			{
				buffer[i + 1] = 0;
			}
			
			break;
		}
	}
	
	return buffer;
}

int sceKernelStartModulePatched(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option)
{
	void *buffer = NULL;
	SceUID fpl = -1;
	
	if (!finished)
	{
		int apitype = sceKernelApplicationType();
		
		if (sceKernelFindModuleByName("sceNp9660_driver"))
		{
			/* Similiar ms code is in np9960, so waste of time to do here */
			ms_checked = 1;
		}

		char *modname;
		
		if (apitype == PSP_INIT_KEYCONFIG_POPS)
			modname = "sceIoFilemgrDNAS";
		else
			modname = "sceMediaSync";
			
		if (sceKernelFindModuleByName(modname))
		{
			char *path = NULL;		
			finished = 1;
			
			if (apitype == PSP_INIT_KEYCONFIG_VSH)
			{
				if (sceKernelFindModuleByName("scePspNpDrm_Driver"))
				{
					path = "ms0:/seplugins/vsh.txt";
				}
			}
			else if (apitype == PSP_INIT_KEYCONFIG_GAME)
			{
				path = "ms0:/seplugins/game.txt";
			}
			else if (apitype == PSP_INIT_KEYCONFIG_POPS)
			{
				path = "ms0:/seplugins/pops.txt";
			}
			
			if (!ms_checked)
			{
				/* Uses macros from pspmscm.h to ease things up */
				if (MScmIsMediumInserted() == 1)
				{
					evid = sceKernelCreateEventFlag("", 0, 0, NULL);
					SceUID cbid = sceKernelCreateCallback("",  MsCheck, NULL);
					
					MScmRegisterMSInsertEjectCallback(cbid);
					sceKernelWaitEventFlagCB(evid, 1, 0x11, NULL, NULL);
					MScmUnregisterMSInsertEjectCallback(cbid);
					
					sceKernelDeleteCallback(cbid);
					sceKernelDeleteEventFlag(evid);
				}
			}
			
			if (ms_checked)
			{
				int i;
				SceUID fd;
				
				for (i = 0; i < 16; i++)
				{
					fd = sceIoOpen(path, PSP_O_RDONLY, 0);
				
					if (fd >= 0)
					{
						break;
					}
					
					sceKernelDelayThread(20000);
				}
				
				if (i != 16)
				{
					fpl = sceKernelCreateFpl("", 1, 0, 1025, 1, NULL);
					
					if (fpl >= 0)
					{
						sceKernelAllocateFpl(fpl, &buffer, NULL);
						int read = sceIoRead(fd, buffer, 1024);
						
						((u8 *)buffer)[1024] = 0;
						char *strbuf = buffer;
						while (read > 0)
						{
							char *module = getLine(strbuf, read);
							int len = strlen(module) - 1;
							
							strbuf += len + 3;
							read -= len + 3;
							
							int active = 0, x;
						
							for (x = len; x >= 0; x--)
							{
								if (module[x] <= 0x20)
									module[x] = 0;
							}
							
							for (x = strlen(module); x <= len; x++)
							{
								if (module[x] == '1')
									active = 1;
							}
							
							if (active)
								LoadStartModule(module);
						}
						
						sceIoClose(fd);
					}
				}
			}
		}
	}

	int res = sceKernelStartModule(modid, argsize, argp, status, option);
	
	if (buffer)
	{
		sceKernelFreeFpl(fpl, buffer);
		sceKernelDeleteFpl(fpl);
	}
	
	return res;
}

int InitModuleStartPatched(int (* init_module_start)(SceSize args, void *argp), void *argp)
{
	u32 text_addr = (u32)init_module_start - 0xD0;
	MAKE_JUMP(text_addr + 0x1B20, sceKernelStartModulePatched);
	
	//memset(dword_A878, 0, 36);		
		
	/*dword_A878[0] = text_addr;
	dword_A878[1] = (u32)argp;
	dword_A878[2] = text_addr + 0x22B0;
	dword_A878[3] = text_addr + 0x22D4;
	dword_A878[4] = text_addr + 0x2424;
	dword_A878[5] = text_addr + 0x2428;
	dword_A878[6] = text_addr + 0x154C;
	dword_A878[7] = text_addr + 0x164C;
	dword_A878[8] = text_addr + 0x13D4;*/
	
	ClearCaches();
	return init_module_start(4, argp);
}

void PatchLoadCore() //updated for 5.00 - confirmed
{
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceLoaderCore");
	u32 text_addr = mod->text_addr;
	
	sceKernelLinkLibraryEntries = (void *)(text_addr + 0x3468);	
	_sw((u32)sceKernelCheckExecFilePatched, text_addr + 0x84C4);
	
	MAKE_CALL(text_addr + 0x1628, sceKernelCheckExecFilePatched);
	MAKE_CALL(text_addr + 0x1678, sceKernelCheckExecFilePatched);
	MAKE_CALL(text_addr + 0x49D8, sceKernelCheckExecFilePatched);
	MAKE_CALL(text_addr + 0x1298, sceKernelLinkLibraryEntriesPatched);
	
	_sw(_lw(text_addr + 0x89C0), text_addr + 0x89DC);
	
	MAKE_CALL(text_addr + 0x46D0, ProbeExec1Patched);
	MAKE_CALL(text_addr + 0x486C, ProbeExec2Patched);
	
	ProbeExec1 = (void *)(text_addr + 0x61D8);
	ProbeExec2 = (void *)(text_addr + 0x60F4);
	
	_sw(0x3C080000, text_addr + 0x40C4);
	_sw(0x3C080000, text_addr + 0x40DC); //lui        $t0, 0x0
	_sw(0x00002021, text_addr + 0x7CF4); //~psp compressed
	
	_sw(0, text_addr + 0x6888);
	_sw(0, text_addr + 0x688C);
	_sw(0, text_addr + 0x6998); //allow usermode to load kernel
	_sw(0, text_addr + 0x699C);
	
	MAKE_CALL(text_addr + 0x1E5C, InitModuleStartPatched);
	_sw(0x02E02021, text_addr + 0x1E60);
	
	MAKE_CALL(text_addr + 0x41D0, (void *)(text_addr + 0x81F0));
	MAKE_CALL(text_addr + 0x68F8, (void *)(text_addr + 0x81F0));
	
	MAKE_CALL(text_addr + 0x691C, (void *)(text_addr + 0x81D0));
	MAKE_CALL(text_addr + 0x694C, (void *)(text_addr + 0x81D0));
	MAKE_CALL(text_addr + 0x69E4, (void *)(text_addr + 0x81D0));
}

void PatchModuleMgr() //updated for 5.00 - confimed
{
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceModuleManager");
	u32 text_addr = mod->text_addr;
	
	MAKE_JUMP(text_addr + 0x81C4, sceKernelStartThreadPatched);	
	MAKE_CALL(text_addr + 0x7C8C, sceKernelCreateThreadPatched);
	
	PartitionCheck = (void *)(text_addr + 0x78F4);
	
	_sw(0, text_addr + 0x760); //sceKernelLoadModule (user)
	_sw(0x24020000, text_addr + 0x7BC); //li         $v0, 0
	
	_sw(0, text_addr + 0x2BE4); //sceKernelLoadModuleVSH
	_sw(0, text_addr + 0x2C3C);
	_sw(0x10000009, text_addr + 0x2C68);
	
	_sw(0, text_addr + 0x2F90); //sceKernelLoadModule
	_sw(0, text_addr + 0x2FE4);
	_sw(0x10000010, text_addr + 0x3010);
	
	MAKE_CALL(text_addr + 0x5F3C, PartitionCheckPatched);
	MAKE_CALL(text_addr + 0x62B8, PartitionCheckPatched);
	
	_sw(0, text_addr + 0x3F6C); //sceKernelQueryModuleInfo
	_sw(0, text_addr + 0x3FB4);
	_sw(0, text_addr + 0x3FCC);
	
	MAKE_JUMP(text_addr + 0x8024, sceKernelCheckExecFilePatched);
}

int validateExecuteablePatched(u8 *buffer, void *a1, void *decrypt_info)
{
	int i;
	
	if (((u32 *)buffer)[0] != 0x5053507E)
		return 0;
	
	for (i = 0; i < 88; i++)
	{
		if (buffer[212 + i])
		{
			if (((u32 *)(buffer + 184))[0] != 0 || ((u32 *)(buffer + 188))[0] != 0)
				return ValidateExecuteable(buffer, a1, decrypt_info);
		}
	}
	
	return 0;
}

int DecryptExecuteablePatched(void *buffer, void *a1, void *decrypt_info, void *a3)
{
	if (buffer && decrypt_info)
	{
		if (((u32 *)buffer)[0x130/4] == 0xC6BA41D3 ||
			((u32 *)buffer)[0x130/4] == 0x55668D96)
		{
			if (((u8 *)buffer)[0x150] == 0x1F && (((u8 *)buffer)[0x151] == 0x8B))
			{
				// Gzip
				*(int *)decrypt_info = ((u32 *)buffer)[0xB0/4];
				memmove((void *)buffer, (void *)buffer+0x150, *(int *)decrypt_info);
				return 0;
			}
		}
	}
	
	int res = DecryptExecuteable(buffer, a1, decrypt_info, a3);
	
	if (res < 0)
	{
		// Decryption error, try to fix it before giving up
		if (validateExecuteablePatched(buffer, a1, decrypt_info) >= 0)
		{
			memlmd_1570BAB4(0, 0xBFC00200);
			return DecryptExecuteable(buffer, a1, decrypt_info, a3);
		}
	}
	
	return res;
}

void PatchMemlmd()
{
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceMemlmd");
	u32 text_addr = mod->text_addr;
	
	memlmd_patches_struct *patches = &memlmd_patches[(psp_model != 0)];
	DecryptExecuteable = (void *)(text_addr + patches->decrypt_addr);
	ValidateExecuteable = (void *)(text_addr + patches->validate_addr);
	
	MAKE_CALL(text_addr + patches->validate_1, validateExecuteablePatched);
	MAKE_CALL(text_addr + patches->validate_2, validateExecuteablePatched);
	
	MAKE_CALL(text_addr + patches->decrypt_1, DecryptExecuteablePatched);
	MAKE_CALL(text_addr + patches->decrypt_2, DecryptExecuteablePatched);
}

void PatchInterruptMgr() //updated for 5.00 - confirmed
{
	SceModule2 *mod = (SceModule2 *)sceKernelFindModuleByName("sceInterruptManager");
	u32 text_addr = mod->text_addr;

	_sw(0, text_addr + 0x145C);
	
	_sw(0, text_addr + 0x150C);
	_sw(0, text_addr + 0x1510);
}

int DisplayColour(u32 val)
{
	int i;
	
	for (i = 0x44000000; i < 0x44200000; i += 4)
	{
		((u32 *)i)[0] = val;
	}
}

int module_start(SceSize args, void *argp)
{
	DisplayColour(0x00FF00FF);
	psp_model = *(int *)0x88FC0004;
	
	PatchLoadCore();
	PatchModuleMgr();
	PatchMemlmd();
	PatchInterruptMgr();
	
	ClearCaches();
	
	SceUID block_id = sceKernelAllocPartitionMemory(1, "Systemex", PSP_SMEM_Low, rebootex_size, NULL);
	
	if (block_id >= 0)
	{
		rebootex = sceKernelGetBlockHeadAddr(block_id);
		memset(rebootex, 0, rebootex_size);
		memcpy(rebootex, (void *)0x88FB0000, rebootex_size);
	}
	
	rebootex_size = *(int *)0x88FC0000;
	slim_partition_2 = *(int *)0x88FC0008;
	slim_partition_8 = *(int *)0x88FC0010;
	
	memcpy((void *)&config, (void *)0x88FC0020, sizeof(ChickHENConfig));
	isconfigflash1 = config.configflash1;
	
	DisplayColour(0x000000FF);
	ClearCaches();
	return 0;
}
