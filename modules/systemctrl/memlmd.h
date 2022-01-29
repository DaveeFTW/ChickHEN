#ifndef __MEMLMD_H__
#define __MEMLMD_H__

typedef struct
{
	u32 validate_addr;
	u32 decrypt_addr;
	u32 validate_1;
	u32 validate_2;
	u32 decrypt_1;
	u32 decrypt_2;
} memlmd_patches_struct;

memlmd_patches_struct memlmd_patches[2] = 
{
	{ 0x00000F10, 0x00000134, 0x000010D8, 0x0000112C, 0x00000E10, 0x00000E74 }, //phat
	{ 0x00000FA8, 0x00000134, 0x00001170, 0x000011C4, 0x00000EA8, 0x00000F0C }, //slim & 3k
};
 
int memlmd_1570BAB4(int val, u32 address);
int validateExecuteablePatched(u8 *buffer, void *a1, void *decrypt_info);

#endif
