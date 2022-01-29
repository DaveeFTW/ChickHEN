#ifndef __SYSTEMCTRL_H__
#define __SYSTEMCTRL_H__

#define sonySetK1() \
	u32 k1; __asm__ volatile ("move %0, $k1\n" "srl $k1, $k1, 16\n" : "=r" (k1))

#define sonySetK1AGAIN() \
	__asm__ volatile ("move %0, $k1\n" "srl $k1, $k1, 16\n" : "=r" (k1))
	
#define sonyRestoreK1() \
	__asm__ volatile ("move $k1, %0\n" :: "r" (k1))

#define DEVKIT_STR_CONV(str) (((str[0] - '0') << 24) | ((str[2] - '0') << 16) | ((str[3] - '0') << 8) | 0x10)

typedef struct {
	u32		e_magic; //0
	u8		e_class; //4
	u8		e_data; //5
	u8		e_idver; //6
	u8		e_pad[9]; //7
	u16		e_type;  //16
	u16		e_machine; //18
	u32		e_version; //20
	u32		e_entry; //24
	u32		e_phoff; //28
	u32		e_shoff; 
	u32		e_flags; 
	u16		e_ehsize; 
	u16		e_phentsize; 
	u16		e_phnum;
	u16		e_shentsize; 
	u16		e_shnum; 
	u16		e_shstrndx; 
} __attribute__((packed)) Elf32_Ehdr;

/* ELF section header */
typedef struct { 
	u32		sh_name; //0
	u32		sh_type; //4
	u32		sh_flags; //8
	u32		sh_addr;//12
	u32		sh_offset; //16
	u32		sh_size; 
	u32		sh_link;
	u32		sh_info;
	u32		sh_addralign;
	u32		sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

typedef struct __attribute__((packed))
{
	int magic; // 0
	int version; // 4
	u32 keyofs; // 8
	u32 valofs; // 12
	int count; // 16
} SfoHeader;

typedef struct __attribute__((packed))
{
	u16 nameofs; // 0
	char alignment; // 2
	char type; // 3
	int valsize; // 4
	int totalsize; // 8
	u16 valofs; // 12
	short unknown; // 16
} SfoEntry;

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

enum PSP_MODELS
{
	PSP_PHAT,
	PSP_SLIM,
	PSP_3000,
};

#define log(...)	while(logStats)\
						sceKernelDelayThread(50000);\
					sprintf(str, __VA_ARGS__ );\
					logInfo(str)

extern int logStats;
extern char str[256];
extern void logInfo(char *msg);

PspIoDrv *sctrlHENFindDriver(char *drvname);
u32 xfsFindFunction(const char *modname, const char *lib, u32 nid, u32 patchfunction);

typedef struct SceModule2 {
    struct SceModule2   *next;
    unsigned short      attribute;
    unsigned char       version[2];
    char                modname[27];
    char                terminal;
    unsigned int        unknown1;
    unsigned int        unknown2;
    SceUID              modid;
    unsigned int        unknown3[2];
    u32         mpid_text;  // 0x38
    u32         mpid_data; // 0x3C
    void *              ent_top;
    unsigned int        ent_size;
    void *              stub_top;
    unsigned int        stub_size;
    unsigned int        unknown4[5];
    unsigned int        entry_addr;
    unsigned int        gp_value;
    unsigned int        text_addr;
    unsigned int        text_size;
    unsigned int        data_size;
    unsigned int        bss_size;
    unsigned int        nsegment;
    unsigned int        segmentaddr[4];
    unsigned int        segmentsize[4];
} SceModule2;

typedef int (* MODSTART_HANDLER)(SceModule2 *);

#endif
