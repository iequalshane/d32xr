/* Shared title and in-game menu. */
#include "r_local.h"

#define MOVEWAIT (I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define ITEMX 88
#define ROW 20

typedef enum {
	ms_main, ms_mode, ms_skill, ms_level, ms_load, ms_save, ms_settings
} screen_t;

enum { new_game, options, load_game, save_game, controls, join_quit };
static const char *const rootArt[] = {
	"M_NGAME", "M_OPTION", "M_LOADG", "M_SAVEG", NULL, "M_JOIN"
};
static const char *const modes[] = {"Single player", "Split-screen", "Multiplayer"};
static const char *const playModes[] = {"Single player", "Co-op", "Deathmatch"};
static screen_t screen;
static boolean startup;
static int cursor, mode, rules, skill, level, slot, savecount;
static int rootLabels[6], skull[2], logo, skillArt, skillTitle, font, smallFont;
static int repeatDelay, cursorFrame;
static VINT slotMap, slotSkill, slotMode;
static char wadName[32], mapName[32];
static boolean occupied, mismatch;

static void drawText(int x, int y, const char *text, int firstGlyph)
{
	while (*text) {
		int c = *text++, width;
		if (c == ' ') {
			x += firstGlyph == smallFont ? 8 : 5;
			continue;
		}
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		if (c < '!' || c > ']')
			c = '?';
		DrawJagobjLump(firstGlyph + c - '!', x, y, &width, NULL);
		x += firstGlyph == smallFont ? 8 : width + 1;
	}
}

void M_MenuText(int x, int y, const char *text)
{
	drawText(x, y, text, font);
}

void M_MenuSmall(int x, int row, const char *text)
{
	drawText(x, row * 8, text, smallFont);
}

/* The DOS quit prompt uses the compact HUD letters with proportional spacing. */
void M_MenuPrompt(int centerX, int y, const char *text)
{
	int width = 0;
	for (const char *p = text; *p; ++p) {
		if (*p == ' ') {
			width += 4;
		} else {
			const jagobj_t *glyph = W_POINTLUMPNUM(smallFont + *p - '!');
			width += BIGSHORT(glyph->width) + 1;
		}
	}
	int x = centerX - width / 2;
	while (*text) {
		int c = *text++;
		if (c == ' ') {
			x += 4;
		} else {
			int w;
			DrawJagobjLump(smallFont + c - '!', x, y, &w, NULL);
			x += w + 1;
		}
	}
}

static void openScreen(screen_t next, int selection)
{
	screen = next;
	cursor = selection;
	repeatDelay = MOVEWAIT;
	clearscreen = 2;
}

static void readSlot(void)
{
	occupied = GetSaveInfo(slot, &slotMap, &slotSkill, &slotMode, wadName, mapName);
	mismatch = occupied && D_strcasecmp(wadName, cd_pwad_name);
}

void M_Start2(boolean atTitle)
{
	startup = atTitle;
	openScreen(ms_main, new_game);
	for (int i = 0; i < 6; ++i) {
		if (i == controls)
			continue;
		rootLabels[i] = W_GetNumForName(i == join_quit && !startup ? "M_QUITG" : rootArt[i]);
	}
	skull[0] = W_GetNumForName("M_SKULL1");
	skull[1] = W_GetNumForName("M_SKULL2");
	logo = W_GetNumForName("M_DOOM");
	skillArt = W_GetNumForName("SKILL0");
	skillTitle = W_GetNumForName("M_SKILL");
	font = W_GetNumForName("SLIME033");
	smallFont = W_GetNumForName("MINI033");
	mode = level = 0;
	skill = startskill;
	rules = gt_coop;
}

void M_Start(void) { M_Start2(true); }
void M_Stop(void) {}

static int rowCount(void)
{
	switch (screen) {
	case ms_main: return 6;
	case ms_mode: return 3;
	case ms_skill: return sk_nightmare + 1;
	case ms_level: return gamemapcount;
	default: return 1;
	}
}

int M_Ticker(void)
{
	int buttons = ticrealbuttons & MENU_BTNMASK;
	int pressed = buttons & ~oldticrealbuttons;
	int frame = (I_GetTime() / (I_IsPAL() ? 10 : 12)) & 1;
	if (frame != cursorFrame)
		clearscreen = 2;
	cursorFrame = frame;

	if (screen == ms_settings) {
		if (!O_SettingsTicker(startup ? NULL : &players[consoleplayer])) {
			WriteEEProm();
			openScreen(ms_main, cursor);
		}
		return ga_nothing;
	}

	if (pressed & (BT_B | BT_RMBTN)) {
		switch (screen) {
		case ms_main: return startup ? ga_nothing : ga_completed;
		case ms_mode: openScreen(ms_main, new_game); break;
		case ms_skill: openScreen(ms_mode, mode); break;
		case ms_level: openScreen(ms_skill, skill); break;
		case ms_save: openScreen(ms_main, save_game); break;
		case ms_load: openScreen(ms_main, load_game); break;
		default: break;
		}
		S_StartSound(NULL, sfx_swtchx);
		return ga_nothing;
	}

	if (pressed & (BT_A | BT_LMBTN | BT_START)) {
		switch (screen) {
		case ms_main:
			switch (cursor) {
			case new_game: openScreen(ms_mode, mode); break;
			case load_game:
			case save_game:
				if (cursor == save_game && (startup || netgame == gt_deathmatch))
					return ga_nothing;
				slot = cursor == save_game;
				savecount = SaveCount();
				openScreen(slot ? ms_save : ms_load, 0);
				readSlot();
				break;
			case join_quit:
				if (startup) {
					consoleplayer = 1;
					I_NetSetup();
					clearscreen = 2;
					return starttype != gt_single ? ga_startnew : ga_nothing;
				}
				/* fall through */
			default:
				O_OpenSettings(cursor == controls ? om_controls :
					cursor == join_quit ? om_quit : om_options);
				openScreen(ms_settings, cursor);
				break;
			}
			break;
		case ms_mode: mode = cursor; openScreen(ms_skill, skill); break;
		case ms_skill: skill = cursor; openScreen(ms_level, level); break;
		case ms_level:
			level = cursor;
			consoleplayer = 0;
			startsave = -1;
			startmap = gamemaplist[level]->mapNumber;
			startskill = skill;
			starttype = mode ? rules : gt_single;
			startsplitscreen = mode == 1;
			if ((ticrealbuttons & (BT_Y|BT_MODE)) == (BT_Y|BT_MODE))
				demorecording = true;
			return ga_startnew;
		case ms_load:
			if (occupied && !mismatch) {
				startsplitscreen = slotMode != gt_single;
				startsave = slot;
				return ga_startnew;
			}
			return ga_nothing;
		case ms_save:
			SaveGame(slot);
			savecount = SaveCount();
			readSlot();
			break;
		default: break;
		}
		S_StartSound(NULL, sfx_pistol);
		return ga_nothing;
	}

	if (!(buttons & (BT_UP|BT_DOWN|BT_LEFT|BT_RIGHT))) {
		repeatDelay = 0;
		return ga_nothing;
	}
	if ((repeatDelay -= vblsinframe) > 0)
		return ga_nothing;
	repeatDelay = MOVEWAIT;
	if (buttons & (BT_DOWN|BT_UP)) {
		int count = rowCount();
		cursor = (cursor + count + (buttons & BT_DOWN ? 1 : -1)) % count;
	} else if (screen == ms_mode && cursor) {
		rules = rules == gt_coop ? gt_deathmatch : gt_coop;
	} else if (screen == ms_save || screen == ms_load) {
		int next = slot + (buttons & BT_RIGHT ? 1 : -1);
		if (next < (screen == ms_save) || next >= MaxSaveCount() ||
			next >= savecount + (screen == ms_save))
			return ga_nothing;
		slot = next;
		readSlot();
	} else return ga_nothing;
	S_StartSound(NULL, sfx_stnmov);
	clearscreen = 2;
	return ga_nothing;
}

static void drawSlot(void)
{
	char text[32];
	M_MenuText(ITEMX, 30, screen == ms_save ? "Save game" : "Load game");
	if (!savecount && screen == ms_load) {
		M_MenuText(48, 72, "Reach your first checkpoint");
		M_MenuText(48, 92, "after the first area.");
		return;
	}
	if (slot) {
		D_snprintf(text, sizeof(text), "Slot %d", slot);
		M_MenuText(ITEMX, 60, text);
	} else M_MenuText(ITEMX, 60, "Last autosave");
	if (!occupied) {
		M_MenuText(ITEMX, 90, "Empty");
		return;
	}
	D_snprintf(text, sizeof(text), "Level %d", slotMap);
	M_MenuText(ITEMX, 90, text);
	M_MenuText(ITEMX, 110, mapName);
	if (mismatch && screen == ms_load) {
		M_MenuText(ITEMX, 130, "WAD mismatch");
		M_MenuText(ITEMX, 150, wadName);
	} else {
		M_MenuText(ITEMX, 130, playModes[slotMode]);
		DrawJagobjLump(skillArt + slotSkill, ITEMX, 150, NULL, NULL);
	}
}

void M_Drawer(void)
{
	int x = screen == ms_main ? 120 : ITEMX;
	int y = 55;
	int spacing = screen == ms_main ? 17 : ROW;
	if (startup && !demoplayback)
		I_ClearFrameBuffer();
	if (screen == ms_settings) {
		O_DrawSettings(skull[cursorFrame]);
		return;
	}
	if (screen == ms_main) {
		jagobj_t *art = W_POINTLUMPNUM(logo);
		DrawJagobj(art, (320 - BIGSHORT(art->width))/2, 16);
		y = BIGSHORT(art->height) + 19;
	} else if (screen == ms_mode)
		M_MenuText(ITEMX, 23, "Pick mode");
	else if (screen == ms_skill)
		DrawJagobjLump(skillTitle, ITEMX, 23, NULL, NULL);
	else if (screen == ms_level)
		M_MenuText(ITEMX, 23, "Starting level");
	else {
		drawSlot();
		y = 60;
	}
	DrawJagobjLump(skull[cursorFrame], x - 30, y + cursor*spacing - 4, NULL, NULL);
	for (int i = 0; i < rowCount(); ++i) {
		switch (screen) {
		case ms_main:
			if (i == controls)
				M_MenuText(x, y, "Controls");
			else
				DrawJagobjLump(rootLabels[i], x, y, NULL, NULL);
			break;
		case ms_mode: M_MenuText(ITEMX, y, modes[i]); break;
		case ms_skill: DrawJagobjLump(skillArt+i, ITEMX, y, NULL, NULL); break;
		case ms_level: M_MenuText(ITEMX, y, G_MapNameForMapNum(gamemaplist[i]->mapNumber)); break;
		default: break;
		}
		y += spacing;
	}
	if (screen == ms_mode && cursor) {
		M_MenuText(ITEMX, y + 10, "Left/right");
		M_MenuText(ITEMX, y + 30, playModes[rules]);
	}
}
