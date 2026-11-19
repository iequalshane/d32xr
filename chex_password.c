#include "doomdef.h"
#include "p_local.h"
#include "mars.h"

extern VINT showAllThings, showAllLines;
boolean P_GivePower(player_t *player, powertype_t power);

static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
static char entry[20];
static char result[40];
static unsigned letter, length;

void CQ_ResetPassword(void)
{
	letter = length = 0;
	entry[0] = result[0] = 0;
}

static void CQ_GiveEquipment(player_t *p, boolean keys)
{
	int i;
	p->armorpoints = 200;
	p->armortype = 2;
	for (i = 0; i < NUMAMMO; i++)
		p->ammo[i] = p->maxammo[i];
	for (i = 0; i < NUMWEAPONS; i++)
		p->weaponowned[i] = i != wp_supershotgun;
	if (keys)
		for (i = 0; i < NUMCARDS; i++)
			p->cards[i] = true;
}

static boolean CQ_ApplyPassword(player_t *p)
{
	static const struct {
		const char *name;
		powertype_t power;
	} powers[] = {
		{"andrewbenson", pw_invulnerability}, {"deanhyers", pw_strength},
		{"marybregi", pw_invisibility}, {"allen", pw_ironfeet},
		{"digitalcafe", pw_allmap}, {"joshuastorms", pw_infrared}
	};
	unsigned i;
	for (i = 0; i < sizeof(powers)/sizeof(powers[0]); i++) {
		powertype_t power = powers[i].power;
		if (D_strcasecmp(entry, powers[i].name))
			continue;
		if (p->powers[power])
			p->powers[power] = power == pw_strength || power == pw_allmap ? 0 : 1;
		else
			P_GivePower(p, power);
		return true;
	}
	if (!D_strcasecmp(entry, "davidbrus")) {
		p->cheats ^= CF_GODMODE;
		if (p->cheats & CF_GODMODE)
			p->health = p->mo->health = 100;
	} else if (!D_strcasecmp(entry, "charlesjacobi")) {
		p->cheats ^= CF_NOCLIP;
		p->mo->flags ^= MF_NOCLIP;
	} else if (!D_strcasecmp(entry, "joelkoenigs")) {
		p->weaponowned[wp_chainsaw] = true;
		p->powers[pw_invulnerability] = 1;
	} else if (!D_strcasecmp(entry, "scottholman") || !D_strcasecmp(entry, "mikekoenigs")) {
		CQ_GiveEquipment(p, entry[0] == 's');
	} else if (!D_strcasecmp(entry, "kimhyers")) {
		D_snprintf(result, sizeof(result), "X %d  Y %d  ANGLE %d",
			p->mo->x >> FRACBITS, p->mo->y >> FRACBITS, (int)(((unsigned)p->mo->angle >> 16) * 360 / 65536));
	} else if (!D_strcasecmp(entry, "sherrill")) {
		if (!showAllLines)
			showAllLines = 1;
		else if (!showAllThings)
			showAllThings = 1;
		else
			showAllThings = showAllLines = 0;
	} else if (length == 11 && !D_strncasecmp(entry, "leesnyder", 9)
		&& entry[9] >= '1' && entry[9] <= '4' && entry[10] >= '1' && entry[10] <= '5') {
		startmap = entry[10] - '0';
		startskill = gameskill;
		starttype = gt_single;
		startsplitscreen = false;
		startsave = -1;
		gameaction = ga_startnew;
	} else {
		return false;
	}
	return true;
}

boolean CQ_PasswordInput(player_t *p, int pressed)
{
	if (pressed & BT_LEFT) letter = (letter + 35) % 36;
	if (pressed & BT_RIGHT) letter = (letter + 1) % 36;
	if (pressed & BT_UP) letter = (letter + 27) % 36;
	if (pressed & BT_DOWN) letter = (letter + 9) % 36;
	if (pressed & BT_B) {
		if (!length) return false;
		entry[--length] = 0;
	}
	if ((pressed & BT_A) && length + 1 < sizeof(entry)) {
		entry[length++] = alphabet[letter];
		entry[length] = 0;
		result[0] = 0;
	}
	if (pressed & BT_C) {
		result[0] = 0;
		boolean valid = CQ_ApplyPassword(p);
		if (!result[0])
			D_snprintf(result, sizeof(result), "%s", valid ? "Code accepted" : "Unknown code");
		length = 0;
		entry[0] = 0;
	}
	return true;
}

void CQ_DrawPassword(void)
{
	unsigned i;
	char cell[2] = {0, 0};
	M_MenuText(48, 24, "Cheat codes");
	M_MenuText(48, 49, entry);
	for (i = 0; i < 36; i++) {
		int x = 49 + (i % 9) * 26;
		int y = 72 + (i / 9) * 20;
		cell[0] = alphabet[i];
		if (i == letter)
			M_MenuText(x - 10, y, ">");
		M_MenuText(x, y, cell);
	}
	M_MenuSmall(32, 19, result);
	M_MenuSmall(24, 21, "A: add   B: erase/back   C: enter");
}
