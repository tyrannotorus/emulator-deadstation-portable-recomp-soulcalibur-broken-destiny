/* Soulcalibur: Broken Destiny's own code beside the translation: what the module changes in the game (tools/patch-gen.py puts the
 * calls into the translated C) and its settings. */
#include "recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}

/* ---- logic at 60 ---- */
/* The game's frame pacer waits (interval + 1) vblanks a frame; the interval (a u16 at 0x08C66608) is set once at boot to 1, 30 steps
 * a second. Object updates step by dt x (interval + 1), so with the interval held at 0 the game steps 60 times a second at the PSP's
 * speed. DSP_LOGIC30=1 keeps the PSP's 30. */
uint32_t sc_frame_interval(uint32_t wanted)
{
	static int psp = -1;
	if (psp < 0) {
		const char *e = getenv("DSP_LOGIC30");
		psp = e && e[0] == '1';
		fprintf(stderr, "soulcalibur: logic %s\n", psp ? "at 30 (DSP_LOGIC30)" : "at 60");
	}
	return psp || wanted != 1u ? wanted : 0u;
}

/* ---- settings (from Deadstation Portable at the start and when one changes) ---- */
static int s_gui_hidden = -1;	/* Fight GUI: show / hide */
static int s_unlock = -1;	/* Unlock Everything: off / on */

void deadstation_module_setting(const char *key, const char *value)
{
	if (strcmp(key, "gui") == 0)
		s_gui_hidden = strcmp(value, "hide") == 0;
	else if (strcmp(key, "unlock") == 0)
		s_unlock = strcmp(value, "on") == 0;
}

static int gui_hidden(void)
{
	if (s_gui_hidden < 0) {
		const char *e = getenv("DSP_GUI");
		s_gui_hidden = e && strcmp(e, "hide") == 0;
	}
	return s_gui_hidden;
}

static int unlock_on(void)
{
	if (s_unlock < 0) {
		const char *e = getenv("DSP_UNLOCK");
		s_unlock = e && strcmp(e, "on") == 0;
	}
	return s_unlock;
}

/* ---- OPTIONS without INSTALL ---- */
/* INSTALL copies the disc's data to the memory stick; a recomp has nothing to install. The list is built from a table of row actions
 * (1-6 at 0x08B7C3E0, each naming its label, description and handler) up to a count of 6: CREDITS (6) goes fifth and the count
 * becomes 5. */
#define OPTIONS_ROW5 0x08B7C3F0u
uint32_t sc_options_rows(uint32_t rows)
{
	return rows == 6u && sr_r32(OPTIONS_ROW5) == 6u ? 5u : rows;
}

/* ---- once a frame, on the game's thread ---- */
static unsigned s_frame;	/* game frames so far */

/* (dev) DSP_FREEZE=FIRST,STEP,COUNT[,SECONDS[,PAIR]]: the game held for SECONDS (2) at game frames FIRST, FIRST + STEP, ... (and
 * PAIR frames after each), the picture on the screen meanwhile, /tmp/sc-frozen naming the frame while it is held (a run's script
 * takes its picture then). DSP_FRAMELOG=1: the frame count in the log each second. */
static void dev_freeze(void)
{
	static int init, n_left, secs = 2, pair, paired;
	static unsigned next, step, log_on;
	s_frame++;
	if (!init) {
		init = 1;
		const char *e = getenv("DSP_FREEZE");
		if (e && sscanf(e, "%u,%u,%d,%d,%d", &next, &step, &n_left, &secs, &pair) < 3)
			n_left = 0;
		log_on = getenv("DSP_FRAMELOG") != NULL;
	}
	if (log_on && s_frame % 60u == 0u)
		fprintf(stderr, "soulcalibur: frame %u at %llu ms\n", s_frame, (unsigned long long)now_ms());
	const unsigned frame = s_frame;
	if (n_left <= 0 || frame != next + (paired ? (unsigned)pair : 0u))
		return;
	FILE *f = fopen("/tmp/sc-frozen", "w");
	if (f) {
		fprintf(f, "%u\n", frame);
		fclose(f);
	}
	const uint64_t until = now_ms() + (uint64_t)secs * 1000u;
	while (now_ms() < until) {
		struct timespec t = { 0, 20000000 };
		nanosleep(&t, NULL);
	}
	remove("/tmp/sc-frozen");
	if (pair > 0 && !paired) {
		paired = 1;
		return;
	}
	paired = 0;
	n_left--;
	next += step;
}

/* Unlock Everything holds the game's unlock records open: the Gauntlet's chapters (a byte at 0x08C5F120, 0x22 all) and the equipment
 * (six words and a halfword from 0x08C5F130). Every fighter and stage is open from the start anyway. */
void sdk_module_frame(void)
{
	static int options_done;
	dev_freeze();
	if (!options_done++)
		sr_w32(OPTIONS_ROW5, 6u);
	if (!unlock_on())
		return;
	sr_w8(0x08C5F120u, 0x22u);
	for (uint32_t i = 0; i < 6u; i++)
		sr_w32(0x08C5F130u + 4u * i, 0xFFFFFFFFu);
	sr_w16(0x08C5F148u, 0xFFFFu);
}

/* ---- 4:3 ---- */
/* Vert+: the PSP's horizontal view with 44 rows more above and below (480 x 360). DSP_VPLUS=0: the PSP's 16:9, letterboxed. */
#define VPLUS_ROWS 44
static int vplus_rows(void)
{
	static int rows = -1;
	if (rows < 0) {
		const char *e = getenv("DSP_VPLUS");
		rows = e && e[0] == '0' ? 0 : VPLUS_ROWS;
	}
	return rows;
}
int sdk_vplus_rows(void) { return vplus_rows(); }

/* Each camera's culling planes (0x08A5F010, from its projection at camera+192 to planes at camera+800..876, read by the game's
 * visibility tests) are made for the PSP's 272 rows: a perspective camera's top and bottom planes from P11, an ortho camera's from
 * P11 and P31. Those terms scaled by 272 / (272 + 2 x rows), the planes as tall as the picture shown, so nothing wholly in the rows
 * Vert+ adds is dropped as off the screen; the projection the game draws with is unchanged. */
uint32_t sc_cull_y(uint32_t bits)
{
	const int rows = vplus_rows();
	if (!rows)
		return bits;
	float f;
	memcpy(&f, &bits, 4);
	f *= 272.0f / (float)(272 + 2 * rows);
	memcpy(&bits, &f, 4);
	return bits;
}

/* The lens flare (0x0885E82C) projects its sun through the main camera and is drawn only while the sun's screen y, 0 at the PSP
 * screen's top and 1 at its bottom, is within 0 to 1: with Vert+ the screen shown runs from -rows / 272 to 1 + rows / 272, mapped
 * here onto 0 to 1, so a sun in the rows above or below has its flare as anywhere else. */
uint32_t sc_screen_v(uint32_t bits)
{
	const int rows = vplus_rows();
	if (!rows)
		return bits;
	float v;
	memcpy(&v, &bits, 4);
	const float k = (float)rows / 272.0f;
	v = (v + k) / (1.0f + 2.0f * k);
	memcpy(&bits, &v, 4);
	return bits;
}

#define MESSAGE_DY 34.0f	/* a menu's message box: below the picture, above the button hints */
#define MENU_UP 22.0f		/* a menu with a title bar: its contents raised, the bar over the picture's top as on the PSP */

/* the SDK's description of a 2D draw (ge_gpu.c, ge.c): what the module may change in it is dy, skip, extend and zoom */
typedef struct { uint32_t tex_addr; int tex_enable; int through; float x0, y0, x1, y1; float dy; int skip;
				 float u0, v0, u1, v1; int tex_w, tex_h; int extend; int tnl; float zoom; } SdkSpriteDraw;

/* a 2D effect of a 3D scene (a lens flare's piece, a spark, smoke): a small texture the game keeps in VRAM, placed where its point
 * of the scene projects, on the screen or off it. Drawn into Vert+'s rows like the scene around it, never laid out as a menu's or
 * the HUD's piece (moved, it jumped 44 rows in and out of the rows above and below as it crossed a line) */
static int scene_effect(const SdkSpriteDraw *d)
{
	return d->tex_enable && (d->tex_addr & 0x0FE00000u) == 0x04000000u && d->tex_w <= 64 && d->tex_h <= 64;
}

/* the fight's HUD, by its textures: the health bars, the timer's frame, the soul gauges, the names' font, the timer's digits (the
 * big font's, not the round's banners: those fly in across the upper middle), and the message box at the bottom ("Acquired a new
 * honor.": its body, frame and text) */
static int hud_texture(const SdkSpriteDraw *d)
{
	switch (d->tex_addr) {
	case 0x0900cee0u: case 0x09011330u: case 0x09013780u: case 0x09013c10u: case 0x08fe9420u:
		return 1;
	case 0x08fc8320u:
		return d->y1 <= 48.0f;
	case 0x08bc9540u: case 0x0900a6a0u:
		return d->y0 >= 152.0f;
	}
	return 0;
}

/* menus and 2D screens */
static void menu_draw(SdkSpriteDraw *d, int rows)
{
	/* a menu with a title bar (a 480 x 32 strip at rows 2-34) seen recently: the screen raised. Some menus redraw only on a change,
	 * the bar after their contents, so the window is long */
	static uint64_t title_ms;
	const uint64_t t = now_ms();
	if (d->tex_enable && d->tex_h == 32 && d->x1 - d->x0 >= 470.0f && d->y0 >= 0.0f && d->y0 <= 4.0f && d->y1 >= 32.0f && d->y1 <= 36.0f)
		title_ms = t;
	const float up = title_ms && t - title_ms <= 2500u ? -MENU_UP : 0.0f;
	if (up == 0.0f && scene_effect(d)) {
		d->extend = 1;
		return;
	}

	/* a message box ("New weapon unlocked."): its dark body (0x08bc9540 at rows 220-258), then its frame and ornaments (0x0900a6a0)
	 * and its text (0x08fe9420) move together, so it covers nothing the PSP's screen showed */
	static float bx0, by0, bx1, by1;
	static uint64_t body_ms;
	if (d->tex_addr == 0x08bc9540u && d->tex_enable && d->x1 - d->x0 <= 320.0f && d->y0 >= 216.0f && d->y0 <= 224.0f &&
		d->y1 >= 254.0f && d->y1 <= 262.0f) {
		bx0 = d->x0, by0 = d->y0, bx1 = d->x1, by1 = d->y1;
		body_ms = t;
		d->dy = MESSAGE_DY;
		return;
	}
	if (body_ms && t - body_ms <= 8u &&
		((d->tex_addr == 0x0900a6a0u && d->x0 >= bx0 - 8.0f && d->x1 <= bx1 + 8.0f && d->y0 >= by0 - 8.0f && d->y1 <= by1 + 10.0f) ||
		 (d->tex_addr == 0x08fe9420u && d->x0 >= bx0 && d->x1 <= bx1 && d->y0 >= by0 && d->y1 <= by1))) {
		d->dy = MESSAGE_DY;
		return;
	}
	/* a movie's frames: as the PSP shows them, letterboxed */
	if (d->tex_addr == 0x099e42c0u || d->tex_addr == 0x0995c280u || d->tex_addr == 0x09a6c300u)
		return;
	/* the fade's texture: over the whole height a fade (the renderer stretches it), else a box's body */
	if (d->tex_addr == 0x08bc9540u) {
		if (!(d->y0 <= 0.0f && d->y1 >= 272.0f))
			d->dy = up;
		return;
	}
	/* a background picture (a whole-width piece, or a tile drawn 1:1 from a large texture): the whole picture zoomed about the
	 * screen's centre to the picture's height (a little of its sides cropped), so it fills the rows above and below with picture,
	 * not black, nor the texture's rows past the picture (another picture's, or garbage); a piece drawn from the same texture just
	 * after it is the same picture (some keep their bottom rows turned aside) */
	static uint32_t pic_tex;
	static uint64_t pic_ms;
	const float fill = (float)(272 + 2 * rows) / 272.0f;
	if ((d->x1 - d->x0 >= 480.0f && d->y1 - d->y0 >= 48.0f) ||
		(d->tex_enable && d->tex_w >= 256 && d->tex_h >= 256 && d->u0 > d->x0 - 1.0f && d->u0 < d->x0 + 1.0f &&
		 d->v0 > d->y0 - 1.0f && d->v0 < d->y0 + 1.0f)) {
		d->zoom = fill;
		if (d->tex_enable)
			pic_tex = d->tex_addr, pic_ms = t;
		return;
	}
	if (d->tex_enable && d->tex_addr == pic_tex && t - pic_ms <= 50u) {
		d->zoom = fill;
		return;
	}
	/* a bar along the top edge, or one wholly in the bottom 32 rows (button hints): to the screen's edge; the rest with the screen,
	 * but a layer over the whole height (a fill: the renderer stretches it) */
	const float h = d->y1 - d->y0;
	if (h < 136.0f && d->y0 <= 8.0f)
		d->dy = -(float)rows;
	else if (h < 136.0f && d->y1 >= 264.0f && d->y0 >= 240.0f)
		d->dy = (float)rows;
	else if (!(d->y0 <= 0.0f && d->y1 >= 272.0f))
		d->dy = up;
}

void sdk_sprite_draw(SdkSpriteDraw *d)
{
	/* in a fight: the HUD's health bars or timer frame drawn in the last 100 ms */
	static uint64_t hud_ms;
	if (d->through && d->tex_enable && (d->tex_addr == 0x0900cee0u || d->tex_addr == 0x09011330u))
		hud_ms = now_ms();
	const int fight = hud_ms && now_ms() - hud_ms <= 100u;
	const int rows = vplus_rows();
	/* a 2D T&L draw: in a fight, a small one within the health bars' rows from the HUD's textures is a bar's flash when its fighter
	 * is hit (0x092A6860) or the timer's (0x092530F0, 0x09257E60), HUD; any other is an effect placed in the scene (sparks, smoke)
	 * and stays */
	if (d->tnl) {
		if (!fight || d->y0 < -16.0f || d->y1 > 52.0f || d->y1 - d->y0 > 64.0f || d->x1 - d->x0 > 96.0f)
			return;
		if (d->tex_addr != 0x092a6860u && d->tex_addr != 0x092530f0u && d->tex_addr != 0x09257e60u)
			return;
		if (gui_hidden())
			d->skip = 1;
		else if (rows)
			d->dy = -(float)rows;
		return;
	}
	if (!d->through)
		return;
	if (!fight) {
		if (rows)
			menu_draw(d, rows);
		return;
	}
	if (scene_effect(d)) {
		d->extend = 1;
		return;
	}
	/* the fight's HUD (hud_texture), wholly in the top part (bars, names, timer, gauges) or the bottom part (messages): to the
	 * screen's edge, or hidden; any other 2D draw of the fight stays where the game puts it */
	const float h = d->y1 - d->y0;
	if (h >= 136.0f || (d->y0 <= 0.0f && d->y1 >= 272.0f))
		return;
	const int top = d->y1 <= 120.0f, bottom = d->y0 >= 152.0f;
	if (!top && !bottom)
		return;
	if (!hud_texture(d))
		return;
	if (gui_hidden())
		d->skip = 1;
	else if (rows)
		d->dy = top ? -(float)rows : (float)rows;
}

/* ---- benchmark aids ---- */
/* DSP_P1_CPU=1: fighter 1 is the CPU's too (the match set-up's two fighter records start with their control type, 2 the CPU's). */
uint32_t sc_fighter_control(uint32_t records, uint32_t cpu)
{
	static int p1_cpu = -1;
	if (p1_cpu < 0) {
		const char *e = getenv("DSP_P1_CPU");
		p1_cpu = e && e[0] == '1';
	}
	if (p1_cpu) {
		sr_w32(records + 0u, cpu);
		sr_w32(records + 4u, cpu);
	}
	return cpu;
}

/* DSP_SEED=n: the game's random numbers seeded with n, not the clock (the same opponents and stages every run). */
uint32_t sc_random_seed(uint32_t seed)
{
	const char *e = getenv("DSP_SEED");
	return e && *e ? (uint32_t)strtoul(e, NULL, 0) : seed;
}
