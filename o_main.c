/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define STARTY		38
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define SLIDEWIDTH 90


extern void IN_DrawValue(int x,int y,int value);

typedef enum
{
	mi_audio,
	mi_video,
	mi_password,

	mi_soundvol,
	mi_music,
	mi_musicvol,
	mi_spcmpack,
#ifndef DISABLE_DMA_SOUND
	mi_sfxdriver,
#endif

	mi_resolution,
	mi_anamorphic,
	mi_detailmode,
	mi_lowres,
	mi_framerate,

	mi_controltype,
	mi_yabcdpad,
	mi_alwaysrun,
	mi_strafebtns,
	mi_help,

	mi_quityes,
	mi_quitno,

	NUMMENUITEMS
} menupos_t;

typedef enum
{
	si_resolution,
	si_sfxvolume,
	si_lod,
	si_musvolume,
	si_framerate,

	NUMMENUSLIDERS
} sliderid_t;

VINT cursorpos;

typedef struct
{
	char	curval;
	char	maxval;
} slider_t;

static slider_t sliders[NUMMENUSLIDERS];

typedef struct
{
	VINT	x;
	VINT	y;
	uint8_t	slider;
	uint8_t screen;
	char 	*name;
} menuitem_t;

static menuitem_t *menuitem;

static VINT cursordelay;
static VINT	movecount;



static VINT	o_slider, o_sliderml, o_slidermm, o_slidermr;

static VINT m_help;

VINT	o_musictype, o_sfxdriver;

static const char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
static const char buttonb[NUMCONTROLOPTIONS][8] =
		{"Fire","Use ","Speed","Use","Speed","Fire"};
static const char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

typedef struct
{
	VINT firstitem;
	VINT numitems;
	char *name;
} menuscreen_t;

typedef enum
{
	ms_none,
	ms_main,
	ms_audio,
	ms_video,
	ms_controls,
	ms_help,
	ms_quit,
	ms_password,

	NUMMENUSCREENS
} screenpos_t;

static menuscreen_t menuscreen[NUMMENUSCREENS];
static VINT screenpos;
static boolean settingsPlayerActive;

/* */
/* Draw control value */
/* */
void O_DrawControl(void)
{
	//EraseBlock(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 20, 90, 80);
	M_MenuText(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 20, buttona[controltype]);
	M_MenuText(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 40, buttonb[controltype]);
	M_MenuText(menuitem[mi_controltype].x + 40, menuitem[mi_controltype].y + 60, buttonc[controltype]);
/*	IN_DrawValue(30, 20, controltype); */
}

/*
===============
=
= O_Init
=
===============
*/

void CQ_ResetPassword(void);
boolean CQ_PasswordInput(player_t *player, int pressed);
void CQ_DrawPassword(void);

void O_Init (void)
{
	int cd_avail;

/* cache all needed graphics */
	o_slider = W_CheckNumForName("O_SLIDER");
	o_sliderml = W_CheckNumForName("O_STRAML");
	o_slidermm = W_CheckNumForName("O_STRAMM");
	o_slidermr = W_CheckNumForName("O_STRAMR");

	o_musictype = musictype;
	o_sfxdriver = sfxdriver;



	m_help = W_CheckNumForName("M_HELP");

/*	initialize variables */

	menuitem = Z_Malloc(sizeof(*menuitem)*NUMMENUITEMS, PU_STATIC);

	cursordelay = MOVEWAIT;
	cursorpos = 0;
	screenpos = ms_none;

	D_memset(menuitem, 0, sizeof(*menuitem)*NUMMENUITEMS);
	D_memset(sliders, 0, sizeof(sliders));

	menuitem[mi_audio] = (menuitem_t){ITEMX, CURSORY(0), 0, ms_audio, "Audio"};
	menuitem[mi_video] = (menuitem_t){ITEMX, CURSORY(1), 0, ms_video, "Video"};
	menuitem[mi_password] = (menuitem_t){ITEMX, CURSORY(2), 0, ms_password, "Cheat Codes"};
	menuitem[mi_help] = (menuitem_t){ITEMX, CURSORY(7), 0, ms_help, "Button guide"};

	menuitem[mi_soundvol].name = "Sfx volume";
	menuitem[mi_soundvol].x = ITEMX;
	menuitem[mi_soundvol].y = STARTY;
	menuitem[mi_soundvol].slider = si_sfxvolume+1;
	sliders[si_sfxvolume].maxval = 4;
	sliders[si_sfxvolume].curval = 4*sfxvolume/64;

	menuitem[mi_music].name = "Music";
	menuitem[mi_music].x = ITEMX;
	menuitem[mi_music].y = STARTY+ITEMSPACE*2;

	menuitem[mi_musicvol].name = "Music volume";
	menuitem[mi_musicvol].x = ITEMX;
	menuitem[mi_musicvol].y = STARTY + ITEMSPACE * 3;
	menuitem[mi_musicvol].slider = si_musvolume+1;
	sliders[si_musvolume].maxval = 8;
	sliders[si_musvolume].curval = 8*musicvolume/64;

	menuitem[mi_spcmpack].name = "SPCM";
	menuitem[mi_spcmpack].x = ITEMX;
	menuitem[mi_spcmpack].y = STARTY+ITEMSPACE*5;

#ifndef DISABLE_DMA_SOUND
	menuitem[mi_sfxdriver].name = "SFX driver";
	menuitem[mi_sfxdriver].x = ITEMX;
	menuitem[mi_sfxdriver].y = STARTY+ITEMSPACE*6;
#endif

	menuitem[mi_resolution].name = "Resolution";
	menuitem[mi_resolution].x = ITEMX;
	menuitem[mi_resolution].y = STARTY;
	menuitem[mi_resolution].slider = si_resolution+1;
	sliders[si_resolution].maxval = numViewports - 1;
	sliders[si_resolution].curval = viewportNum;

	menuitem[mi_anamorphic].name = "Widescreen";
	menuitem[mi_anamorphic].x = ITEMX;
	menuitem[mi_anamorphic].y = STARTY + ITEMSPACE * 2;

	menuitem[mi_detailmode].name = "Flats";
	menuitem[mi_detailmode].x = ITEMX;
	menuitem[mi_detailmode].y = STARTY + ITEMSPACE * 3;

	menuitem[mi_lowres].name = "Low detail";
	menuitem[mi_lowres].x = ITEMX;
	menuitem[mi_lowres].y = STARTY + ITEMSPACE * 4;

	menuitem[mi_framerate].name = "Framerate cap";
	menuitem[mi_framerate].x = ITEMX;
	menuitem[mi_framerate].y = STARTY + ITEMSPACE * 5;
	menuitem[mi_framerate].slider = si_framerate+1;
	sliders[si_framerate].maxval = MAXTICSPERFRAME - MINTICSPERFRAME;
	sliders[si_framerate].curval = MAXTICSPERFRAME - ticsperframe;

	menuitem[mi_controltype].name = "Gamepad";
	menuitem[mi_controltype].x = ITEMX;
	menuitem[mi_controltype].y = STARTY;

	menuitem[mi_yabcdpad].name = "YABC D-pad";
	menuitem[mi_yabcdpad].x = ITEMX;
	menuitem[mi_yabcdpad].y = STARTY+ITEMSPACE*4;

	menuitem[mi_alwaysrun].name = "Always run";
	menuitem[mi_alwaysrun].x = ITEMX;
	menuitem[mi_alwaysrun].y = STARTY+ITEMSPACE*5;

	menuitem[mi_strafebtns].name = "LR Strafe";
	menuitem[mi_strafebtns].x = ITEMX;
	menuitem[mi_strafebtns].y = STARTY+ITEMSPACE*6;

	menuscreen[ms_main].name = "Options";
	menuscreen[ms_main].firstitem = mi_audio;
	menuscreen[ms_main].numitems = mi_password - mi_audio + 1;

	menuscreen[ms_audio].name = "Audio";
	menuscreen[ms_audio].firstitem = mi_soundvol;
	menuscreen[ms_audio].numitems = mi_musicvol - mi_soundvol + 1;


	cd_avail = S_CDAvailable();
	if (cd_avail) /* CDA or MD+ */
	{
		menuscreen[ms_audio].numitems++; // SPCM
#ifndef DISABLE_DMA_SOUND
		if (cd_avail & 0x1) /* CD, not MD+ */
			menuscreen[ms_audio].numitems++;
#endif
	}

	menuscreen[ms_video].name = "Video";
	menuscreen[ms_video].firstitem = mi_resolution;
	menuscreen[ms_video].numitems = mi_framerate - mi_resolution + 1;

	menuscreen[ms_controls].name = "Controls";
	menuscreen[ms_controls].firstitem = mi_controltype;
	menuscreen[ms_controls].numitems = mi_help - mi_controltype + 1;

	menuscreen[ms_help].name = "Help";
	menuscreen[ms_help].firstitem = 0;
	menuscreen[ms_help].numitems = 0;

	menuscreen[ms_quit].firstitem = mi_quityes;
	menuscreen[ms_quit].numitems = mi_quitno - mi_quityes + 1;
}

/*
==================
=
= O_Control
=
= Button bits can be eaten by clearing them in ticbuttons[playernum]
==================
*/

void O_Control (player_t *player)
{
	int		buttons, oldbuttons;
	int playernum = player - players;
	int curplayer = consoleplayer;

	if (splitscreen && playernum != curplayer)
		return;

	buttons = player->ticbuttons & MENU_BTNMASK;
	oldbuttons = player->oldticbuttons & MENU_BTNMASK;
	
	if ( ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
#ifdef MARS
		|| ( (buttons & BT_START) && !(oldbuttons & BT_START) && !(buttons & BT_MODE) )
#endif
		)
	{
		player->automapflags ^= AF_OPTIONSACTIVE;

		if (playernum == curplayer)
		{
			if (netgame == gt_single)
				gamepaused ^= 1;
			o_musictype = musictype;
			o_sfxdriver = sfxdriver;
			movecount = 0;
			cursorpos = 0;
			screenpos = ms_main;
			if (player->automapflags & AF_OPTIONSACTIVE)
			{
				M_Start2(false);
				player->ticbuttons &= (BT_OPTION|BT_START);
				S_StartSound(NULL,  sfx_swtchn);
#ifndef MARS
				DoubleBufferSetup();
#endif
				return;
			}
			else
			{
				S_StartSound(NULL,  sfx_swtchx);
				WriteEEProm ();		/* save new settings */
			}
		}
	}

	if (!(player->automapflags & AF_OPTIONSACTIVE))
		return;

/* clear buttons so player isn't moving aroung */
	player->ticbuttons &= (BT_OPTION|BT_START);	/* leave option status alone */

	if (playernum != curplayer)
		return;

	int action = M_Ticker();
	if (action == ga_completed) {
		player->automapflags &= ~AF_OPTIONSACTIVE;
		if (netgame == gt_single)
			gamepaused = false;
		WriteEEProm();
		clearscreen = 2;
	} else if (action != ga_nothing) {
		gameaction = action;
	}
}

void O_OpenSettings(optionmenu_t menu)
{
	screenpos = menu == om_controls ? ms_controls : menu == om_quit ? ms_quit : ms_main;
	cursorpos = screenpos == ms_quit; /* Default to No. */
	cursordelay = MOVEWAIT;
	o_musictype = musictype;
	o_sfxdriver = sfxdriver;
}

boolean O_SettingsTicker(player_t *player)
{
	menuscreen_t *menuscr = &menuscreen[screenpos];
	boolean newcursor = false;
	int buttons, oldbuttons;

	settingsPlayerActive = player && netgame == gt_single;
	buttons = ticrealbuttons & MENU_BTNMASK;
	oldbuttons = oldticrealbuttons & MENU_BTNMASK;

	if (screenpos == ms_quit) {
		if ((buttons & BT_LEFT) && cursorpos == 1) buttons |= BT_UP;
		if ((buttons & BT_RIGHT) && cursorpos == 0) buttons |= BT_DOWN;
	}

	if (screenpos == ms_password) {
		if (settingsPlayerActive ? !CQ_PasswordInput(player, buttons & ~oldbuttons) :
			(buttons & ~oldbuttons & (BT_B | BT_RMBTN)))
			goto goback;
		if (buttons != oldbuttons)
			clearscreen = 2;
		return true;
	}

	if (buttons & (BT_A | BT_LMBTN) && !(oldbuttons & (BT_A | BT_LMBTN)))
	{
		int itemno = menuscr->firstitem + cursorpos;

		if (itemno == mi_quityes)
		{
			clearscreen = 2;

			S_StartSound(NULL, sfx_slop);

			// let the exit sound play
			int ticcount = I_GetTime();
			int lastticcount = ticcount;
			int ticwait = 150;
			do {
				ticcount = I_GetTime();
				S_UpdateSounds();
			} while (ticcount - lastticcount < ticwait);

			S_StopSong();

			gameaction = ga_quit;
			return true;
		}

		if (itemno == mi_quitno)
		{
			goto goback;
		}

		if (menuscr->numitems > 0 && menuitem[itemno].screen != ms_none)
		{
			movecount = 0;
			cursorpos = 0;
			screenpos = menuitem[itemno].screen;
			clearscreen = 2;

			if (screenpos == ms_password)
				CQ_ResetPassword();
			else
				S_StartSound(NULL, sfx_pistol);
			return true;
		}
	}

	if (buttons & (BT_B | BT_RMBTN) && !(oldbuttons & (BT_B | BT_RMBTN)))
	{
goback:
		if (screenpos == ms_main || screenpos == ms_quit || screenpos == ms_controls)
			return false;
		if (screenpos == ms_help) {
			screenpos = ms_controls;
			cursorpos = mi_help - mi_controltype;
		} else {
			cursorpos = screenpos == ms_audio ? 0 : screenpos == ms_video ? 1 : 2;
			screenpos = ms_main;
		}
		clearscreen = 2;
		S_StartSound(NULL, sfx_swtchx);
		return true;
	}

	if (buttons == 0)
	{
		cursordelay = 0;
		return true;
	}

	cursordelay -= vblsinframe;
	if (cursordelay > 0)
		return true;

	cursordelay = MOVEWAIT;

/* check for movement */
	if (! (buttons & (BT_UP| BT_DOWN| BT_LEFT| BT_RIGHT) ) )
		movecount = 0;		/* move immediately on next press */
	else
	{
		int sound = sfx_None;
		int itemno = menuscr->firstitem + cursorpos;
		slider_t*slider = menuitem[itemno].slider ? &sliders[menuitem[itemno].slider-1] : NULL;
		int oldcursorpos = cursorpos;

		if (slider && (buttons & (BT_RIGHT|BT_LEFT)))
		{
			boolean sliderchange = false;

			if (buttons & BT_RIGHT)
			{
				if (++slider->curval > slider->maxval)
					slider->curval = slider->maxval;
				else
					sliderchange = true;
			}
			if (buttons & BT_LEFT)
			{
				if (--slider->curval < 0)
					slider->curval = 0;
				else
					sliderchange = true;
			}

			if (sliderchange)
			{
				switch (itemno)
				{
				case mi_soundvol:
					sfxvolume = 64 * slider->curval / slider->maxval;
					break;
				case mi_musicvol:
					musicvolume = 64 * slider->curval / slider->maxval;
					break;
				case mi_resolution:
					R_SetViewportSize(slider->curval);
					break;
				case mi_framerate:
					ticsperframe = MAXTICSPERFRAME - slider->curval;
					break;
				default:
					break;
				}

				sound = sfx_stnmov;
			}
		}

		movecount = 0;		/* repeat move */

		if (++movecount == 1)
		{
			if (menuscr->numitems > 0)
			{
				if (buttons & BT_DOWN)
				{
					++cursorpos;
					if (screenpos == ms_audio && !gameinfo.spcmDirList[0][0])
					{
						if (menuscr->firstitem+cursorpos == mi_spcmpack)
							++cursorpos;
					}
					if (cursorpos >= menuscr->numitems)
						cursorpos = 0;
				}

				if (buttons & BT_UP)
				{
					--cursorpos;
					if (cursorpos == -1)
						cursorpos = menuscr->numitems-1;
					if (screenpos == ms_audio && !gameinfo.spcmDirList[0][0])
					{
						if (menuscr->firstitem+cursorpos == mi_spcmpack)
							--cursorpos;
					}
				}
			}

			if (screenpos == ms_controls)
			{
				int oldcontroltype = controltype;
				int oldalwaysrun = alwaysrun;
				int oldstrafebtns = strafebtns;
				int oldyabcdpad = yabcdpad;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_controltype:
						if (++controltype == NUMCONTROLOPTIONS)
							controltype = (NUMCONTROLOPTIONS - 1);
						break;
					case mi_alwaysrun:
						if (++alwaysrun > 1)
							alwaysrun = 1;
						break;
					case mi_strafebtns:
						if (++strafebtns > 3)
							strafebtns = 3;
						break;
					case mi_yabcdpad:
						if (++yabcdpad > 1)
							yabcdpad = 1;
						break;
					}
				}
				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_controltype:
						if (--controltype == -1)
							controltype = 0;
						break;
					case mi_alwaysrun:
						if (--alwaysrun < 0)
							alwaysrun = 0;
						break;
					case mi_strafebtns:
						if (--strafebtns < 0)
							strafebtns = 0;
						break;
					case mi_yabcdpad:
						if (--yabcdpad < 0)
							yabcdpad = 0;
						break;
					}
				}

				if (oldcontroltype != controltype ||
					oldalwaysrun != alwaysrun ||
					oldstrafebtns != strafebtns ||
					oldyabcdpad != yabcdpad)
					sound = sfx_stnmov;
			}

			if (screenpos == ms_audio)
			{
				int i;
				int oldmusictype = o_musictype;
				int oldsfxdriver = o_sfxdriver;
				int oldspcmpack, curpack, numpacks;

				curpack = -1;
				numpacks = 0;
				for (i = 0; i < MAX_SPCM_PACKS; i++) {
					if (!gameinfo.spcmDirList[i][0]) {
						numpacks = i;
						break;
					}
					if (!D_strcasecmp(gameinfo.spcmDirList[i], spcmDir)) {
						curpack = i;
					}
				}
				if (i == MAX_SPCM_PACKS)
					numpacks = MAX_SPCM_PACKS;
				oldspcmpack = curpack;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_music:
						++o_musictype;
						if (!gameinfo.spcmDirList[0][0])
						{
							if (o_musictype > mustype_cd)
								o_musictype = mustype_cd;
						}
						else
						{
							if (o_musictype > mustype_spcm)
								o_musictype = mustype_spcm;
						}
						if (o_musictype >= mustype_cd && !S_CDAvailable())
							o_musictype = mustype_fm;
						break;
					case mi_spcmpack:
						if (++curpack >= numpacks)
							curpack = numpacks-1;
						break;
#ifndef DISABLE_DMA_SOUND
					case mi_sfxdriver:
						if (++o_sfxdriver > sfxdriver_pwm)
							o_sfxdriver = sfxdriver_pwm;
						break;
#endif
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_music:
						if (--o_musictype < mustype_none)
							o_musictype = mustype_none;
						break;
					case mi_spcmpack:
						if (--curpack < 0)
							curpack = 0;
						break;
#ifndef DISABLE_DMA_SOUND
					case mi_sfxdriver:
						if (--o_sfxdriver < sfxdriver_auto)
							o_sfxdriver = sfxdriver_auto;
						break;
#endif
					}
				}

				if (oldmusictype != o_musictype)
				{
					S_SetMusicType(o_musictype);
					sound = sfx_stnmov;
				}

				if (oldsfxdriver != o_sfxdriver)
				{
					S_SetSoundDriver(o_sfxdriver);
					clearscreen = 2;					
				}

				if (oldspcmpack != curpack && curpack != -1)
				{
					S_SetSPCMDir(gameinfo.spcmDirList[curpack]);
					if (musictype == mustype_spcm)
						S_SetMusicType(mustype_spcmhack); // force refresh of the current dir
					clearscreen = 2;
				}
			}

			if (screenpos == ms_video)
			{
				int oldanamorphicview = anamorphicview;
				int olddetailmode = detailmode;

				if (buttons & BT_RIGHT)
				{
					switch (itemno) {
					case mi_anamorphic:
						if (++anamorphicview > 1)
							anamorphicview = 1;
						break;
					case mi_detailmode:
						if (++detailmode > detmode_normal)
							detailmode = detmode_normal;
						break;
					case mi_lowres:
						if (!lowres)
						{
							R_EnableLowResMode(true);
							R_SetViewportSize(viewportNum);
						}
						break;
					}
				}

				if (buttons & BT_LEFT)
				{
					switch (itemno) {
					case mi_anamorphic:
						if (--anamorphicview < 0)
							anamorphicview = 0;
						break;
					case mi_detailmode:
						if (--detailmode < detmode_potato)
							detailmode = detmode_potato;
						break;
					case mi_lowres:
						if (lowres)
						{
							R_EnableLowResMode(false);
							R_SetViewportSize(viewportNum);
						}
						break;
					}
				}

				if (olddetailmode != detailmode)
				{
					R_SetDrawFuncs(false);
					sound = sfx_stnmov;
				}
				if (oldanamorphicview != anamorphicview)
				{
					R_SetViewportSize(viewportNum);
					sound = sfx_stnmov;
				}
			}

			newcursor = cursorpos != oldcursorpos;
			if (newcursor)
				sound = sfx_pistol;
		}

		if (sound != sfx_None)
		{
			S_StartSound(NULL, sound);
			clearscreen = 2;
		}
	}

	if (newcursor)
		clearscreen = 2;
	return true;
}

void O_Drawer(void) { M_Drawer(); }

void O_DrawSettings(int cursorLump)
{
	int		i;
	int		offset;
	int		y;
	menuscreen_t* menuscr = &menuscreen[screenpos];
	menuitem_t* items = &menuitem[menuscr->firstitem];

	if (screenpos == ms_quit) {
		int center = SCREENWIDTH / 2;
		int y = SCREENHEIGHT / 2 - 12;
		M_MenuPrompt(center, y, "PLEASE DON'T LEAVE, WE");
		M_MenuPrompt(center, y + 9, "NEED YOUR HELP!");
		M_MenuPrompt(center, y + 28, "QUIT?");
		M_MenuPrompt(center - 20, y + 41, cursorpos == 0 ? "[YES]" : "YES");
		M_MenuPrompt(center + 20, y + 41, cursorpos == 1 ? "[NO]" : "NO");
		return;
	}
	if (screenpos == ms_password) {
		if (!settingsPlayerActive) {
			M_MenuText(62, 64, "Cheat codes");
			M_MenuText(38, 100, "Use during single player");
		} else CQ_DrawPassword();
		return;
	}
	if (screenpos != ms_help)
		DrawJagobjLump(cursorLump, CURSORX, items[cursorpos].y - 2, NULL, NULL);

/* Draw menu */

	y = 18;
	M_MenuText(ITEMX, y, menuscr->name);
	
	for (i = 0; i < menuscr->numitems; i++)
	{
		if (!gameinfo.spcmDirList[0][0])
		{
			if (menuscr->firstitem+i == mi_spcmpack)
				continue;
		}

		y = items[i].y;
		M_MenuText(items[i].x, y, items[i].name);

		if(items[i].slider)
		{
			int j;
			slider_t* slider = &sliders[items[i].slider-1];

			DrawJagobjLump(o_sliderml, items[i].x + 2, items[i].y + ITEMSPACE, NULL, NULL);
			for (j = 0; j < SLIDEWIDTH; j+=9)
				DrawJagobjLump(o_slidermm, items[i].x + 2 + 6 + j, items[i].y + ITEMSPACE, NULL, NULL);
			DrawJagobjLump(o_slidermr, items[i].x + 2 + 6 + j, items[i].y + ITEMSPACE, NULL, NULL);

			offset = (slider->curval * SLIDEWIDTH) / slider->maxval;
			DrawJagobjLump(o_slider, items[i].x + 5 + offset, items[i].y + ITEMSPACE, NULL, NULL);
/*			ST_Num(menuitem[i].x + o_slider->width + 10,	 */
/*			menuitem[i].y + 20,slider[i].curval);  */
		}
	}
	
/* Draw control info */
	if (screenpos == ms_controls)
	{
		const char* strabtnstr = "OFF";

		switch (strafebtns) {
		case 1:
			strabtnstr = "YZ";
			break;
		case 2:
			strabtnstr = "ZC";
			break;
		case 3:
			strabtnstr = "XZ";
			break;
		}

		M_MenuText(items[0].x + 10, items[0].y + ITEMSPACE, "A");
		M_MenuText(items[0].x + 10, items[0].y + ITEMSPACE*2, "B");
		M_MenuText(items[0].x + 10, items[0].y + ITEMSPACE*3, "C");

		O_DrawControl();

		M_MenuText(menuitem[mi_alwaysrun].x + 150, menuitem[mi_yabcdpad].y, yabcdpad ? "ON" : "OFF");
		M_MenuText(menuitem[mi_alwaysrun].x + 150, menuitem[mi_alwaysrun].y, alwaysrun ? "ON" : "OFF");
		M_MenuText(menuitem[mi_strafebtns].x + 150, menuitem[mi_strafebtns].y, strabtnstr);
	}

	if (screenpos == ms_audio)
	{
		switch (o_musictype) {
		case mustype_none:
			M_MenuText(menuitem[mi_music].x + 85, menuitem[mi_music].y, "off");
			break;
		case mustype_fm:
			M_MenuText(menuitem[mi_music].x + 85, menuitem[mi_music].y, "fm");
			break;
		case mustype_cd:
			M_MenuText(menuitem[mi_music].x + 85, menuitem[mi_music].y, "cda");
			break;
		case mustype_spcm:
			M_MenuText(menuitem[mi_music].x + 85, menuitem[mi_music].y, "spcm");
			break;
		}

		if (gameinfo.spcmDirList[0][0])
		{
			if (menuscreen[ms_audio].numitems > 3)
			{
				if (spcmDir[0] != '\0') {
					M_MenuText(menuitem[mi_spcmpack].x + 85, menuitem[mi_spcmpack].y, spcmDir);
				} else {
					M_MenuText(menuitem[mi_spcmpack].x + 85, menuitem[mi_spcmpack].y, "NONE");
				}
			}
		}

#ifndef DISABLE_DMA_SOUND
		if (menuscreen[ms_audio].numitems > 4)
		{
			switch (o_sfxdriver) {
			case sfxdriver_auto:
				M_MenuText(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "auto");
				break;
			case sfxdriver_mcd:
				M_MenuText(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "mcd");
				break;
			case sfxdriver_pwm:
				M_MenuText(menuitem[mi_sfxdriver].x + 150, menuitem[mi_sfxdriver].y, "pwm");
				break;
			}
		}
#endif
	}

	if (screenpos == ms_video)
	{
		char tmp[32];
		D_snprintf(tmp, sizeof(tmp), "%dx%d", viewportWidth, viewportHeight);
		M_MenuSmall(menuitem[mi_resolution].x + 114, (unsigned)menuitem[mi_resolution].y/8 + 3, tmp);

		switch (anamorphicview) {
		case 0:
			M_MenuText(menuitem[mi_anamorphic].x + 160, menuitem[mi_anamorphic].y, "off");
			break;
		case 1:
			M_MenuText(menuitem[mi_anamorphic].x + 150, menuitem[mi_anamorphic].y, "on");
			break;
		}

		switch (detailmode) {
		case detmode_potato:
			M_MenuText(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "simple");
			break;
		case detmode_lowres:
			M_MenuText(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "low-res");
			break;
		default:
			M_MenuText(menuitem[mi_detailmode].x + 90, menuitem[mi_detailmode].y, "high-res");
			break;
		}

		switch (lowres) {
		case true:
			M_MenuText(menuitem[mi_lowres].x + 160, menuitem[mi_lowres].y, "on");
			break;
		default:
			M_MenuText(menuitem[mi_lowres].x + 160, menuitem[mi_lowres].y, "off");
			break;
		}

		D_snprintf(tmp, sizeof(tmp), "%d fps", I_IsPAL() ? 50/ticsperframe : 60/ticsperframe);
		M_MenuSmall(menuitem[mi_framerate].x + 114, (unsigned)menuitem[mi_framerate].y/8 + 3, tmp);
	}

	if (screenpos == ms_help)
	{
		int x, x2, x3, l, l2;

		x = 10;
		y = CURSORY(0)-10;
		l = y/8;
		l += 1;

		x2 = 88;
		l2 = l;
		x3 = x2+7*8+4;
		M_MenuSmall(x, l, "Prev weap");
		M_MenuSmall(x2, l, "START+A");
		l++;
		M_MenuSmall(x, l, "Next weap");
		M_MenuSmall(x2, l, "START+B");
		l++;
		M_MenuSmall(x, l, "Automap");
		M_MenuSmall(x2, l, "START+C");
		if (strafebtns)
		{
			switch (strafebtns)
			{
			default:
			case 1:
			case 2:
				M_MenuSmall(x3, l2, "or");
				M_MenuSmall(x3+16+4, l2, "X");
				break;
			case 3:
				M_MenuSmall(x3, l2, "or");
				M_MenuSmall(x3+16+4, l2, "Y");
				break;
			}
		}
		else
		{
			M_MenuSmall(x3, l2, "or");
			M_MenuSmall(x3+16+4, l2, "X");
			M_MenuSmall(x3, l2+1, "or");
			M_MenuSmall(x3+16+4, l2+1, "Y");
			M_MenuSmall(x3, l2+2, "or");
			M_MenuSmall(x3+16+4, l2+2, "Z");
		}
		l++;

		l++;

		x2 = 138;
		M_MenuSmall(x, l++, "Hold MODE and press a");
		M_MenuSmall(x, l++, "button to switch to:");
		M_MenuSmall(x, l, "Spoon/Bootspork");
		M_MenuSmall(x2, l++, "START");
		M_MenuSmall(x, l, "Mini Zorcher");
		M_MenuSmall(x2, l++, "A");
		M_MenuSmall(x, l, "Large Zorcher");
		M_MenuSmall(x2, l++, "B");
		M_MenuSmall(x, l, "Rapid Zorcher");
		M_MenuSmall(x2, l++, "C");
		M_MenuSmall(x, l, "Zorch Propulsor");
		M_MenuSmall(x2, l++, "X");
		M_MenuSmall(x, l, "Phasing Zorcher");
		M_MenuSmall(x2, l++, "Y");
		M_MenuSmall(x, l, "LAZ Device");
		M_MenuSmall(x2, l++, "Z");

		l++;

		x2 = 62;
		M_MenuSmall(x, l++, "Automap");
		M_MenuSmall(x, l, "Scale");
		M_MenuSmall(x2, l++, "Hold B+UP/DOWN");
		M_MenuSmall(x, l, "Lock");
		M_MenuSmall(x2, l++, "Hold C");

		if (m_help >= 0)
		{
			x = 182;
			l = y/8;
			M_MenuSmall(x, l+1, "Online help:");
			DrawJagobjLump(m_help, x, y+16, NULL, NULL);
		}
	}
}

