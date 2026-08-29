/* LineLimit probe: one source, built against both GDI+ implementations.
 *
 *   Windows GDI+ (native gdiplus.dll, also under Wine):
 *     zig cc -target x86-windows-gnu -o probe.exe probe.c
 *     WINEDLLOVERRIDES=gdiplus=n wine probe.exe <path to gdiplus.dll>
 *   libgdiplus:
 *     cc -o probe probe.c -ldl
 *     ./probe <path to libgdiplus.so.0>
 *
 * The function table is resolved at run time so that both builds share every
 * line of the measuring code. Rectangle heights are given as multiples (k) of
 * the font height reported by GdipGetFontHeight, so that the two sides line up
 * row by row. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#define API __stdcall
static void *open_lib (const char *p) { return LoadLibraryA (p); }
static void *sym (void *h, const char *n) { return (void *) GetProcAddress ((HMODULE) h, n); }
#else
#include <dlfcn.h>
#define API
static void *open_lib (const char *p) { return dlopen (p, RTLD_NOW); }
static void *sym (void *h, const char *n) { return dlsym (h, n); }
#endif

typedef uint16_t WCH;
typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { uint32_t Version; void *Callback; int NoThread; int NoCodecs; } StartupInput;

#define LINE_LIMIT 0x2000
#define PIXEL_FORMAT_32BPP_RGB 0x00022009
#define UNIT_POINT 3

static int (API *GdiplusStartup) (uintptr_t *, const StartupInput *, void *);
static int (API *GdipCreateBitmapFromScan0) (int, int, int, int, void *, void **);
static int (API *GdipGetImageGraphicsContext) (void *, void **);
static int (API *GdipGraphicsClear) (void *, uint32_t);
static int (API *GdipCreateSolidFill) (uint32_t, void **);
static int (API *GdipDrawString) (void *, const WCH *, int, void *, const RectF *, void *, void *);
static int (API *GdipMeasureString) (void *, const WCH *, int, void *, const RectF *, void *, RectF *, int *, int *);
static int (API *GdipDeleteGraphics) (void *);
static int (API *GdipDisposeImage) (void *);
static int (API *GdipDeleteBrush) (void *);
static int (API *GdipBitmapGetPixel) (void *, int, int, uint32_t *);
static int (API *GdipCreateStringFormat) (int, uint16_t, void **);
static int (API *GdipDeleteStringFormat) (void *);
static int (API *GdipSetStringFormatAlign) (void *, int);
static int (API *GdipSetStringFormatLineAlign) (void *, int);
static int (API *GdipSetStringFormatHotkeyPrefix) (void *, int);
static int (API *GdipCreateFontFamilyFromName) (const WCH *, void *, void **);
static int (API *GdipGetGenericFontFamilySansSerif) (void **);
static int (API *GdipCreateFont) (void *, float, int, int, void **);
static int (API *GdipGetFontHeight) (void *, void *, float *);
static int (API *GdipGetFamily) (void *, void **);
static int (API *GdipGetFamilyName) (void *, WCH *, uint16_t);

#define LOAD(f) if (!(*(void **) &f = sym (lib, #f))) { fprintf (stderr, "missing %s\n", #f); exit (1); }

static void *graphics;

/* Draws into a 100x80 white bitmap; returns the number of non-white pixels. */
static int ink (void *font, const WCH *s, int n, void *format, float h)
{
	void *img, *g, *brush;
	RectF r = { 4, 4, 78, h };
	int x, y, count = 0;
	uint32_t c;

	GdipCreateBitmapFromScan0 (100, 80, 0, PIXEL_FORMAT_32BPP_RGB, NULL, &img);
	GdipGetImageGraphicsContext (img, &g);
	GdipGraphicsClear (g, 0xffffffff);
	GdipCreateSolidFill (0xff000000, &brush);
	GdipDrawString (g, s, n, font, &r, format, brush);
	GdipDeleteGraphics (g);
	for (y = 0; y < 80; y++)
		for (x = 0; x < 100; x++)
			if (GdipBitmapGetPixel (img, x, y, &c) == 0 && (c & 0xffffff) != 0xffffff)
				count++;
	GdipDeleteBrush (brush);
	GdipDisposeImage (img);
	return count;
}

static void *make_format (int flags, int button)
{
	void *f;
	GdipCreateStringFormat (flags, 0, &f);
	if (button) {
		/* System.Windows.Forms.ButtonBase's text_format */
		GdipSetStringFormatAlign (f, 1);
		GdipSetStringFormatLineAlign (f, 1);
		GdipSetStringFormatHotkeyPrefix (f, 1);
	}
	return f;
}

static void row (void *font, const char *name, const WCH *s, int n, int button, const char *k, float h)
{
	void *f = make_format (LINE_LIMIT, button), *plain = make_format (0, button);
	RectF r = { 0, 0, 78, h }, bounds;
	int chars = -1, lines = -1;

	GdipMeasureString (graphics, s, n, font, &r, f, &bounds, &chars, &lines);
	printf ("%-10s %5s %7.2f %6d %6d %6d %6d\n", name, k, h, lines, chars,
		ink (font, s, n, f, h) > 0, ink (font, s, n, plain, h) > 0);
	GdipDeleteStringFormat (f);
	GdipDeleteStringFormat (plain);
}

static void run (void *family)
{
	static const double ks[] = { 0.50, 0.80, 0.90, 0.95, 0.99, 1.00, 1.01, 1.50, 1.99, 2.00, 2.01, 2.99, 3.01 };
	static const WCH ok[] = { 'O', 'K', 0 }, three[] = { 'A', '\n', 'B', '\n', 'C', 0 };
	static const WCH two[] = { 'A', '\n', 'B', 0 }, okNewline[] = { 'O', 'K', '\n', 0 };
	/* one paragraph that wraps into several lines in a 78 pixel wide rectangle */
	static const WCH wrap[] = { 'w','o','r','d','s',' ','t','h','a','t',' ','w','r','a','p',' ',
		'o','n','t','o',' ','m','o','r','e',' ','t','h','a','n',' ','o','n','e',' ','l','i','n','e', 0 };
	static const struct { const char *name; const WCH *s; int button; } cases[] = {
		{ "1/button", ok, 1 }, { "1/near", ok, 0 }, { "1nl/near", okNewline, 0 },
		{ "2/near", two, 0 }, { "3/button", three, 1 }, { "3/near", three, 0 }, { "wrap/near", wrap, 0 },
	};
	void *font, *fam;
	WCH wname[32];
	char name[32], kb[8];
	float fh;
	int c, i;

	GdipCreateFont (family, 8.25f, 0, UNIT_POINT, &font);
	GdipGetFontHeight (font, graphics, &fh);
	GdipGetFamily (font, &fam);
	GdipGetFamilyName (fam, wname, 0);
	for (i = 0; i < 31 && wname[i]; i++)
		name[i] = (char) wname[i];
	name[i] = 0;

	printf ("\nfont \"%s\" 8.25pt  GdipGetFontHeight=%.3f  Font.Height=%d\n", name, fh, (int) ceil (fh));
	printf ("%-10s %5s %7s %6s %6s %6s %6s\n", "case", "k", "h", "lines", "chars", "drawn", "noLL");
	for (c = 0; c < (int) (sizeof cases / sizeof cases[0]); c++) {
		const WCH *s = cases[c].s;
		int n = 0;
		while (s[n])
			n++;
		for (i = 0; i < (int) (sizeof ks / sizeof ks[0]); i++) {
			snprintf (kb, sizeof kb, "%.2f", ks[i]);
			row (font, cases[c].name, s, n, cases[c].button, kb, (float) (ks[i] * fh));
		}
		row (font, cases[c].name, s, n, cases[c].button, "F.Ht", (float) ceil (fh));
	}
}

int main (int argc, char **argv)
{
	static const WCH noto[] = { 'N','o','t','o',' ','S','a','n','s', 0 };
	StartupInput in = { 1, NULL, 0, 0 };
	uintptr_t token;
	void *lib, *img, *family;

	if (argc < 2 || !(lib = open_lib (argv[1]))) {
		fprintf (stderr, "usage: probe <gdiplus library>\n");
		return 1;
	}
	LOAD (GdiplusStartup) LOAD (GdipCreateBitmapFromScan0) LOAD (GdipGetImageGraphicsContext)
	LOAD (GdipGraphicsClear) LOAD (GdipCreateSolidFill) LOAD (GdipDrawString) LOAD (GdipMeasureString)
	LOAD (GdipDeleteGraphics) LOAD (GdipDisposeImage) LOAD (GdipDeleteBrush) LOAD (GdipBitmapGetPixel)
	LOAD (GdipCreateStringFormat) LOAD (GdipDeleteStringFormat) LOAD (GdipSetStringFormatAlign)
	LOAD (GdipSetStringFormatLineAlign) LOAD (GdipSetStringFormatHotkeyPrefix)
	LOAD (GdipCreateFontFamilyFromName) LOAD (GdipGetGenericFontFamilySansSerif) LOAD (GdipCreateFont)
	LOAD (GdipGetFontHeight) LOAD (GdipGetFamily) LOAD (GdipGetFamilyName)

	GdiplusStartup (&token, &in, NULL);
	GdipCreateBitmapFromScan0 (1, 1, 0, PIXEL_FORMAT_32BPP_RGB, NULL, &img);
	GdipGetImageGraphicsContext (img, &graphics);

	printf ("columns: lines/chars = GdipMeasureString with LineLimit; drawn = DrawString with LineLimit\n"
		"         painted anything; noLL = the same without LineLimit\n");
	if (GdipCreateFontFamilyFromName (noto, NULL, &family) == 0)
		run (family);
	else
		printf ("\n(Noto Sans not available)\n");
	if (GdipGetGenericFontFamilySansSerif (&family) == 0)
		run (family);
	return 0;
}
