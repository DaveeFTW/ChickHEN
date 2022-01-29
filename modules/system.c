/*
	ChickHEN vshGraphics Engine
	by Davee
*/

#include <pspkernel.h>
#include <pspge.h>

#define	PSP_LINE_SIZE 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define FRAMEBUFFER_SIZE (PSP_LINE_SIZE * SCREEN_HEIGHT)

char *menu_entries[] =
{
	"ChickHEN VSHMenu",
	"Hide TIFF in PHOTO",
	"Hide Corrupt Icons",
	"Spoof Version",
	"Spoof Mac Address"
	"Use ChickHEN Updater",
	"Spoof 3k to 2k (in game)"
	"Reboot PSP",
	"Exit VSHMenu",
};

#define ENTRIES_N 9

u32 current_selection, last_buttons = PSP_CTRL_HOME, menu_max = ENTRIES_N;

void HomeHandler()
{
	menu_thid = -1;
	sceKernelExitDeleteThread(0);
}

void vshGfxCtrlReadBufferPositive(SceCtrlData *pad_data, int count)
{
	for (i = 0; i < count; i++)
	{
		u32 buttons = pad_data[i].Button;
		if (buttons != last_buttons)
		{
			last_buttons = buttons;
			
			if (buttons & PSP_CTRL_UP)
			{
				current_selection--;
				
				if (current_selection < 0)
					current_selection = menu_max - 1;
			}
			
			else if (buttons & PSP_CTRL_DOWN)
			{
				current_selection++;
				
				if (current_selection >= menu_max)
					current_selection = 0;
			}
			
			else if (buttons & PSP_CTRL_CROSS)
			{
				if (vshGfxOptions[current_selection].AcceptHandler)
					vshGfxOptions[current_selection].AcceptHandler();
			}
			
			else if (buttons & PSP_CTRL_CIRCLE)
			{
				BackHandler();
			}
			
			else if (buttons & PSP_CTRL_SELECT)
			{
				HomeHandler();
			}
		}
		
		pad_data[i].Buttons &= ~0x0081F3F9;
	}
}

enum sctrlGfxPopupModes
{
	POPUP_ERROR = 1,
	POPUP_NOTIFY = 2,
	POPUP_WARNING = 4,
	POPUP_YESNO = 8,
	POPUP_CUSTOMOPTIONS = 16,
};

typedef struct
{
	int x;
	int y;
	int modetype; //One of the popup modes
	int customop_n;
	char **customops;
	void (* AcceptHandler)(void);
} sctrlGfxOptionsStruct;

SceUID popup_thid = -1;

char *popup_yesno_ops[] =
{
	"YES",
	"NO",
};

void DrawPopup(char *title, char *description, char **popup_ops, int popup_n, int x, int y, int boxsize, int colour, int selection_colour)
{
	int i, current_colour = 0, text_colour = 0;
	sceKernelChangeThreadPriority(0, 8);
	
	while (1)
	{
		sceDisplayWaitVblankStart();
		sceDisplayGetMode(&pmode, &pwidth, &pheight);
		sceDisplayGetFrameBuf((void *)&vram32, &bufferwidth, &pixelformat, 1);
		
		if (pixelformat != PSP_DISPLAY_PIXEL_FORMAT_8888)
			continue;
		
		sctrlGfxDrawRectangle(x, y, boxsize, selection_colour);
		
		blit_string(x, y, 0x00FFFFFF, selection_colour, title); //Draw Title
		blit_string(x, y + 30, 0x00FFFFFF, selection_colour, description);
		
		for (i = 0; i < popup_n; i++)
		{
			if (i == current_selection)
			{
				current_colour = selection_colour;
				text_colour = 0x00FFFF00;
			}
			else
			{
				current_colour = colour;
				text_colour = 0x00FFFFFF;
			}
			
			blit_string(x, y + 40 + (i * 8), text_colour, current_colour, popup_ops[i]);
		}
	}
}

int sctrlPopupStart(SceSize args, u32 *argp)
{
	char *string_description = (char *)argp[0];
	sctrlGfxOptionsStruct *sctrlGfxOptions = (sctrlGfxOptionsStruct *)argp[1];
	
	char *popup_title;
	char **popup_ops = NULL;
	int popup_n = 0;
	
	if (sctrlGfxOptions->modetype & POPUP_ERROR)
	{
		colour = 0xAA0000FF;
		selected = 0xFF459EDC;
		popup_title = "Systemctrl Popup: An Error has occured";
	}
	
	else if(sctrlGfxOptions->modetype & POPUP_NOTIFY)
	{
		colour = 0xAAFF5500;
		colour = 0xFFDC9E45;
		popup_title = "Systemctrl Popup: Notification";
	}
	
	else if (sctrlGfxOptions->modetype & POPUP_WARNING)
	{
		colour = 0xAA0055FF;
		selected = 0xFF459EDC;
		popup_title = "Systemctrl Popup: WARNING!";
	}
	
	if (sctrlGfxOptions->modetype & POPUP_YESNO)
	{
		popup_n = 2;
		popup_ops = popup_yesno_ops;
	}
	
	else if (sctrlGfxOptions->modetype & POPUP_YESNO)
	{
		popup_n = sctrlGfxOptions->customop_n;
		popup_ops = sctrlGfxOptions->customops;
	}
	
	DrawPopup(popup_title, string_description, popup_ops, popup_n, sctrlGfxOptions->x, sctrlGfxOptions->y, 100, colour, selected);

}

int sctrlGfxCreatePopup(char *string, sctrlGfxOptionsStruct *sctrlGfxOptions)
{
	if (popup_thid < 0)
	{
		popup_thid = sceKernelCreateThread("sctrlGfxPopup", sctrlPopupStart, 0x10, 0x1000, 0, NULL);
		
		if (popup_thid >= 0)
		{
			u32 argp[2];
			argp[0] = (u32 *)string;
			argp[1] = (u32 *)sctrlGfxOptions;
			
			sceKernelStartThread(popup_thid, 8, argp);
		}
		
		sceKernelWaitThreadEnd(popup_thid);
		return 0;
	}
	
	return -1;
}

int vshGfxStart(SceSize args, void *argp)
{

	
	sceKernelChangeThreadPriority(0, 8);
	
	while (1)
	{
		sceDisplayWaitVblankStart();
		sceDisplayGetMode(&pmode, &pwidth, &pheight);
		sceDisplayGetFrameBuf((void *)&vram32, &bufferwidth, &pixelformat, 1);
		
		if (pixelformat != PSP_DISPLAY_PIXEL_FORMAT_8888)
			continue;
		
		for (i = 0; i < ENTRIES_N; i++)
		{
			if (i - 1 == selection)
				colour = 0xAAFF5500;
			else
				colour = 0xFFDC9E45;
			
			blit_string(192, 50 + (i * 8), 0x00FFFFFF, colour, menu_entries[i]);
		}
	}
	
	return 0;
}

void sctrlGfxPutChar(char character, int x, int y, u32 colour, u32 bgcolour, u32 *vram)
{
	u32 cy, my, cs, mx, c1, c2, alpha;
	char *letter = font[character * 8];
	
	for (cy = 0; cy < 8; cy++)
	{
		for (my = 0; my < 1; my++)
		{
			u32 b = 0x80;
			u32 *vram32 = vram;
			
			for (cx = 0; cx < 8; cx++)
			{
				for (mx = 0; mx < 1; mx++)
				{
					if (letter[0] & b) //we need to draw a pixel here...
					{
						alpha = colour >> 24;
						
						if (alph
							*(unsigned long *)vram32 = color;
					}
					else //no pixel to be drawn
					{ 
						if (drawbg) 
						{
							*(unsigned long *)vram32=bgcolor; 
						}
					}
					
					alpha = cur_colour >> 24;
					
					if (!alpha)
					{
						vram32[0] = cur_colour;
					}
					
					else if (alpha != 0xFF)
					{
						c2 = vram32[0];
						c1 = c2 & 0x00ff00ff;
						c2 = c2 & 0x0000ff00;
						c1 = ((c1 * alpha) >> 8) & 0x00ff00ff;
						c2 = ((c2 * alpha) >> 8) & 0x0000ff00;
						
						vram32[0] = (col & 0xffffff) + c1 + c2;
					}
					
					vram32++;
				}
				
				b >>= 1; 
			}
			
			vram += LINESIZE * 4;		
		}
		
		letter++;
	}
}
