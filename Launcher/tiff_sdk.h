#ifndef __TIFF_SDK__
#define __TIFF_SDK__

#include <stdint.h>

#define NULL ((void *)0)
#define PSP_O_RDONLY	0x0001
#define PSP_O_WRONLY	0x0002
#define PSP_O_RDWR	(PSP_O_RDONLY | PSP_O_WRONLY)
#define PSP_O_NBLOCK	0x0004
#define PSP_O_DIROPEN	0x0008	// Internal use for dopen
#define PSP_O_APPEND	0x0100
#define PSP_O_CREAT	0x0200
#define PSP_O_TRUNC	0x0400
#define	PSP_O_EXCL	0x0800
#define PSP_O_NOWAIT	0x8000

#define PSP_SEEK_SET	0
#define PSP_SEEK_CUR	1
#define PSP_SEEK_END	2

typedef	uint8_t	u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t	s8;
typedef int16_t	s16;
typedef int32_t	s32;
typedef int64_t	s64;

typedef unsigned char SceUChar8;
typedef uint16_t SceUShort16;
typedef uint32_t SceUInt32;
typedef uint64_t SceUInt64;
typedef uint64_t SceULong64;

typedef char SceChar8;
typedef int16_t SceShort16;
typedef int32_t SceInt32;
typedef int64_t SceInt64;
typedef int64_t SceLong64;

typedef int SceUID;
typedef unsigned int SceSize;
typedef int SceSSize;
typedef unsigned char SceUChar;
typedef unsigned int SceUInt;
typedef int SceMode;
typedef SceInt64 SceOff;
typedef SceInt64 SceIores;

typedef struct _SceKernelUtilsMd5Context {
	unsigned int 	h[4];
	unsigned int 	pad;
	SceUShort16 	usRemains;
	SceUShort16 	usComputed;
	SceULong64 	ullTotalLen;
	unsigned char 	buf[64];
} SceKernelUtilsMd5Context;

enum PspCtrlButtons
{
	PSP_CTRL_SELECT     = 0x000001,
	PSP_CTRL_START      = 0x000008,
	PSP_CTRL_UP         = 0x000010,
	PSP_CTRL_RIGHT      = 0x000020,
	PSP_CTRL_DOWN      	= 0x000040,
	PSP_CTRL_LEFT      	= 0x000080,
	PSP_CTRL_LTRIGGER   = 0x000100,
	PSP_CTRL_RTRIGGER   = 0x000200,
	PSP_CTRL_TRIANGLE   = 0x001000,
	PSP_CTRL_CIRCLE     = 0x002000,
	PSP_CTRL_CROSS      = 0x004000,
	PSP_CTRL_SQUARE     = 0x008000,
	PSP_CTRL_HOME       = 0x010000,
	PSP_CTRL_HOLD       = 0x020000,
	PSP_CTRL_NOTE       = 0x800000,
	PSP_CTRL_SCREEN     = 0x400000,
	PSP_CTRL_VOLUP      = 0x100000,
	PSP_CTRL_VOLDOWN    = 0x200000,
	PSP_CTRL_WLAN_UP    = 0x040000,
	PSP_CTRL_REMOTE     = 0x080000,	
	PSP_CTRL_DISC       = 0x1000000,
	PSP_CTRL_MS         = 0x2000000,
};

typedef struct SceCtrlData {
	unsigned int 	TimeStamp;
	unsigned int 	Buttons;
	unsigned char 	Lx;
	unsigned char 	Ly;
	unsigned char 	Rsrv[6];
} SceCtrlData;

typedef struct ScePspDateTime {
	unsigned short	year;
	unsigned short 	month;
	unsigned short 	day;
	unsigned short 	hour;
	unsigned short 	minute;
	unsigned short 	second;
	unsigned int 	microsecond;
} ScePspDateTime;

enum IoAssignPerms
{
	/** Assign the device read/write */
	IOASSIGN_RDWR = 0,
	/** Assign the device read only */
	IOASSIGN_RDONLY = 1
};

typedef struct SceIoStat {
	SceMode 		st_mode;
	unsigned int 	st_attr;
	SceOff 			st_size;
	ScePspDateTime 	st_ctime;
	ScePspDateTime 	st_atime;
	ScePspDateTime 	st_mtime;
	unsigned int 	st_private[6];
} SceIoStat;

typedef struct SceKernelLMOption {
	SceSize                 size;
	SceUID                  mpidtext;
	SceUID                  mpiddata;
	unsigned int    flags;
	char                    position;
	char                    access;
	char                    creserved[2];
} SceKernelLMOption;

typedef struct SceKernelSMOption {
	SceSize 		size;
	SceUID 			mpidstack;
	SceSize 		stacksize;
	int 			priority;
	unsigned int 	attribute;
} SceKernelSMOption;

typedef struct SceModule2 
{
	struct SceModule	*next; // 0
	u16					attribute; // 4
	u8					version[2]; // 6
	char				modname[27]; // 8
	char				terminal; // 0x23
	char				mod_state;	// 0x24
    char				unk1;    // 0x25
	char				unk2[2]; // 0x26
	u32					unk3;	// 0x28
	SceUID				modid; // 0x2C
	u32					unk4; // 0x30
	SceUID				mem_id; // 0x34
	u32					mpid_text;	// 0x38
	u32					mpid_data; // 0x3C
	void *				ent_top; // 0x40
	unsigned int		ent_size; // 0x44
	void *				stub_top; // 0x48
	u32					stub_size; // 0x4C
	u32					entry_addr_; // 0x50
	u32					unk5[4]; // 0x54
	u32					entry_addr; // 0x64
	u32					gp_value; // 0x68
	u32					text_addr; // 0x6C
	u32					text_size; // 0x70
	u32					data_size;	// 0x74
	u32					bss_size; // 0x78
	u32					nsegment; // 0x7C
	u32					segmentaddr[4]; // 0x80
	u32					segmentsize[4]; // 0x90
} SceModule2;

typedef struct SceLibraryStubTable {
	/* The name of the library we're importing from. */
	const char *		libname;
	/** Minimum required version of the library we want to import. */
	unsigned char		version[2];
	/* Import attributes. */
	unsigned short		attribute;
	/** Length of this stub table in 32-bit WORDs. */
	unsigned char		len;
	/** The number of variables imported from the library. */
	unsigned char		vstubcount;
	/** The number of functions imported from the library. */
	unsigned short		stubcount;
	/** Pointer to an array of NIDs. */
	unsigned int *		nidtable;
	/** Pointer to the imported function stubs. */
	void *				stubtable;
	/** Pointer to the imported variable stubs. */
	void *				vstubtable;
} SceLibraryStubTable;

typedef struct SceLibraryEntryTable {
       const char *            libname;
        unsigned char           version[2];
      unsigned short          attribute;
        unsigned char           len;
        unsigned char           vstubcount;
  unsigned short          stubcount;
       void *                          entrytable;
 } SceLibraryEntryTable;


typedef struct
{
	int val;//0 //84
	int unk1[3]; //4 //0
	int val2; //16 //14
	char unk2[72]; //20 //0
} ExploitStruct;

enum SceUtilityOskState
{
	PSP_UTILITY_OSK_DIALOG_NONE = 0,	/**< No OSK is currently active */
	PSP_UTILITY_OSK_DIALOG_INITING,		/**< The OSK is currently being initialized */
	PSP_UTILITY_OSK_DIALOG_INITED,		/**< The OSK is initialised */
	PSP_UTILITY_OSK_DIALOG_VISIBLE,		/**< The OSK is visible and ready for use */
	PSP_UTILITY_OSK_DIALOG_QUIT,		/**< The OSK has been cancelled and should be shut down */
	PSP_UTILITY_OSK_DIALOG_FINISHED		/**< The OSK has successfully shut down */	
};

#endif
