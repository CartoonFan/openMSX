// $Id$

#ifndef __SDLHIRENDERER_HH__
#define __SDLHIRENDERER_HH__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "PixelRenderer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"

class VDP;
class VDPVRAM;
class SpriteChecker;
class SDLConsole;


/** Factory method to create SDLHiRenderer objects.
  * TODO: Add NTSC/PAL selection
  *   (immutable because it is colour encoding, not refresh frequency).
  */
Renderer *createSDLHiRenderer(VDP *vdp, bool fullScreen, const EmuTime &time);

/** Hi-res (640x480) renderer on SDL.
  */
template <class Pixel> class SDLHiRenderer : public PixelRenderer
{
public:
	/** Constructor.
	  * It is suggested to use the createSDLHiRenderer factory
	  * function instead, which automatically selects a colour depth.
	  */
	SDLHiRenderer(VDP *vdp, SDL_Surface *screen, bool fullScreen, const EmuTime &time);

	/** Destructor.
	  */
	virtual ~SDLHiRenderer();

	// Renderer interface:

	//void frameStart(const EmuTime &time);
	//void putImage(const EmuTime &time);
	void setFullScreen(bool);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	//void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(int mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	//void updateVRAM(int addr, byte data, const EmuTime &time);

protected:
	void finishFrame();
	void updateVRAMCache(int addr, byte data) {
		(this->*dirtyChecker)(addr, data);
	}
	void drawBorder(int fromX, int fromY, int limitX, int limitY);
	void drawDisplay(int fromX, int fromY, int limitX, int limitY);

private:
	typedef void (SDLHiRenderer::*DirtyChecker)
		(int addr, byte data);

	inline void renderBitmapLines(byte line, int count);
	inline void renderPlanarBitmapLines(byte line, int count);
	inline void renderCharacterLines(byte line, int count);

	/** Get width of the display area in pixels.
	  */
	inline int getDisplayWidth();

	/** Get a pointer to the start of a VRAM line in the cache.
	  * @param displayCache The display cache to use.
	  * @param line The VRAM line, range depends on display cache.
	  */
	inline Pixel *getLinePtr(SDL_Surface *displayCache, int line);

	/** Get the pixel colour of a graphics 7 colour index.
	  */
	inline Pixel graphic7Colour(byte index);

	/** Get the pixel colour of the border.
	  * SCREEN6 has separate even/odd pixels in the border.
	  * TODO: Implement the case that even_colour != odd_colour.
	  */
	inline Pixel getBorderColour();

	/** Dirty checking that does nothing (but is a valid method).
	  */
	void checkDirtyNull(int addr, byte data);

	/** Dirty checking for MSX1 display modes.
	  */
	void checkDirtyMSX1(int addr, byte data);

	/** Dirty checking for Text2 display mode.
	  */
	void checkDirtyText2(int addr, byte data);

	/** Dirty checking for bitmap modes.
	  */
	void checkDirtyBitmap(int addr, byte data);

	/** Draw sprites on this line over the background.
	  */
	void drawSprites(int screenLine, int leftBorder, int minX, int maxX);

	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	/** DirtyCheckers for each display mode.
	  */
	static DirtyChecker modeToDirtyChecker[];

	/** SDL colours corresponding to each VDP palette entry.
	  * palFg has entry 0 set to the current background colour,
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16], palBg[16];

	/** SDL colours corresponding to each Graphic 7 sprite colour.
	  */
	Pixel palGraphic7Sprites[16];

	/** SDL colours of current sprite palette.
	  * Points to either palBg or palGraphic7Sprites.
	  */
	Pixel *palSprites;

	/** SDL colours corresponding to each possible V9938 colour.
	  * Used by updatePalette to adjust palFg and palBg.
	  * Since SDL_MapRGB may be slow, this array stores precalculated
	  * SDL colours for all possible RGB values.
	  */
	Pixel V9938_COLOURS[8][8][8];

	/** SDL colours corresponding to the 256 colour palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	Pixel PALETTE256[256];

	/** Dirty checker: update dirty tables on VRAM write.
	  */
	DirtyChecker dirtyChecker;

	/** The surface which is visible to the user.
	  */
	SDL_Surface *screen;

	/** Cache for rendered VRAM in character modes.
	  * Cache line (N + scroll) corresponds to display line N.
	  * It holds a single page of 256 lines.
	  */
	SDL_Surface *charDisplayCache;

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	SDL_Surface *bitmapDisplayCache;

	/** Display mode the line is valid in.
	  * 0xFF means invalid in every mode.
	  */
	byte lineValidInMode[256 * 4];

	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  */
	bool anyDirtyColour, dirtyColour[1 << 10];
	bool anyDirtyPattern, dirtyPattern[1 << 10];
	bool anyDirtyName, dirtyName[1 << 12];
	// TODO: Introduce "allDirty" to speed up screen splits.

	/** Did foreground colour change since last screen update?
	  */
	bool dirtyForeground;

	/** Did background colour change since last screen update?
	  */
	bool dirtyBackground;

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel, Renderer::ZOOM_512> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel, Renderer::ZOOM_512> bitmapConverter;

	SDLConsole* console;
};

#endif //__SDLHIRENDERER_HH__

