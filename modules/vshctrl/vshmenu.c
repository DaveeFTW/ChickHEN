/*
	5.03 ChickHEN vshctrl
	By Davee
*/

int (* sceGuFinish)(void) = NULL;

int menuDrawInformationBannerRight(char *text, int y, u32 colour)
{

}

int menuCtrlReadBufferPositive(SceCtrlData *pad_data, int count)
{

}

void menuInitGraphicSystem()
{
	SceModule2 *mod = sceKernelFindModuleByName("scePaf_Module");
	u32 text_addr = mod->text_addr;
	
	(text_addr + 0x000E1DF8));
	MAKE_CALL(
}

int menuGraphicsStart(SceSize args, void *argp)
{
	if (!graphics_init)
		menuInitGraphicSystem();
	
	
}