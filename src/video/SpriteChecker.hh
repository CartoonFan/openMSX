// $Id$

#ifndef SPRITECHECKER_HH
#define SPRITECHECKER_HH

#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VRAMObserver.hh"
#include "DisplayMode.hh"

namespace openmsx {

class RenderSettings;
class BooleanSetting;

class SpriteChecker: public VRAMObserver
{
public:
	/** Bitmap of length 32 describing a sprite pattern.
	  * Visible pixels are 1, transparent pixels are 0.
	  * If the sprite is less than 32 pixels wide,
	  * the lower bits are unused.
	  */
	typedef unsigned SpritePattern;

	/** Contains all the information to draw a line of a sprite.
	  */
	struct SpriteInfo {
		/** Pattern of this sprite line, corrected for magnification.
		  */
		SpritePattern pattern;
		/** X-coordinate of sprite, corrected for early clock.
		  */
		short int x;
		/** Bit 3..0 are index in palette.
		  * Bit 6 is 0 for sprite mode 1 like behaviour,
		  * or 1 for OR-ing of sprite colours.
		  * Other bits are undefined.
		  */
		byte colourAttrib;
	};

	/** Create a sprite checker.
	  * @param vdp The VDP this sprite checker is part of.
	  * @param renderSettings TODO
	  * @param time TODO
	  */
	SpriteChecker(VDP& vdp, RenderSettings& renderSettings,
	              const EmuTime& time);

	/** Puts the sprite checker in its initial state.
	  * @param time The moment in time this reset occurs.
	  */
	void reset(const EmuTime& time);

	/** Update sprite checking to specified time.
	  * This includes a VRAM sync.
	  * @param time The moment in emulated time to update to.
	  */
	inline void sync(const EmuTime& time) {
		if (mode0) return;
		// Debug:
		// This method is not re-entrant, so check explicitly that it is not
		// re-entered. This can disappear once the VDP-internal scheduling
		// has become stable.
		#ifdef DEBUG
		static bool syncInProgress = false;
		assert(!syncInProgress);
		syncInProgress = true;
		#endif
		vram.sync(time);
		checkUntil(time);
		#ifdef DEBUG
		syncInProgress = false;
		#endif
	}

	/** Clear status bits triggered by reading of S#0.
	  */
	inline void resetStatus() {
		// TODO: Used to be 0x5F, but that is contradicted by
		//       TMS9918.pdf. Check on real MSX.
		vdp.setSpriteStatus(vdp.getStatusReg0() & 0x1F);
	}

	/** Informs the sprite checker of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayMode(DisplayMode mode, const EmuTime& time) {
		sync(time);
		switch (mode.getSpriteMode()) {
		case 0:
			updateSpritesMethod = &SpriteChecker::updateSprites0;
			mode0 = true;
			return;
		case 1:
			updateSpritesMethod = &SpriteChecker::updateSprites1;
			break;
		case 2:
			updateSpritesMethod = &SpriteChecker::updateSprites2;
			break;
		default:
			assert(false);
		}
		if (mode0) {
			// switch from mode0 to some other mode
			mode0 = false;
			currentLine = frameStartTime.getTicksTill_fast(time)
			                 / VDP::TICKS_PER_LINE;
			// Every line in mode0 has 0 sprites, but none of the lines
			// are ever requested by the renderer, except for the last
			// line, because sprites are checked one line before they
			// are displayed.
			//   already done in frameStart()
			//   spriteCount[currentLine - 1] = 0;
		}
		planar = mode.isPlanar();
	}

	/** Informs the sprite checker of a VDP display enabled change.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayEnabled(bool enabled, const EmuTime& time) {
		enabled = enabled; // avoid warning
		sync(time);
		// TODO: Speed up sprite checking in display disabled case.
	}

	/** Informs the sprite checker of sprite enable changes.
	  * @param enabled The new sprite enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpritesEnabled(bool enabled, const EmuTime& time) {
		enabled = enabled; // avoid warning
		sync(time);
		// TODO: Speed up sprite checking in display disabled case.
	}

	/** Informs the sprite checker of sprite size or magnification changes.
	  * @param sizeMag The new size and magnification state.
	  *   Bit 0 is magnification: 0 = normal, 1 = doubled.
	  *   Bit 1 is size: 0 = 8x8, 1 = 16x16.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpriteSizeMag(byte sizeMag, const EmuTime& time) {
		sizeMag = sizeMag; // avoid warning
		sync(time);
		// TODO: Precalc something?
	}

	/** Informs the sprite checker of a vertical scroll change.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateVerticalScroll(int scroll, const EmuTime& time) {
		scroll = scroll; // avoid warning
		sync(time);
		// TODO: Precalc something?
	}

	/** Update sprite checking until specified line.
	  * VRAM must be up-to-date before this method is called.
	  * It is not allowed to call this method in a spriteless display mode.
	  * @param time The moment in emulated time to update to.
	  */
	inline void checkUntil(const EmuTime& time) {
		// TODO:
		// Currently the sprite checking is done atomically at the end of
		// the display line. In reality, sprite checking is probably done
		// during most of the line. Run tests on real MSX to make a more
		// accurate model of sprite checking.
		int limit = frameStartTime.getTicksTill_fast(time)
		               / VDP::TICKS_PER_LINE;
		if (currentLine < limit) {
			// Call the right update method for the current display mode.
			(this->*updateSpritesMethod)(limit);
		}
	}

	/** Get X coordinate of sprite collision.
	  */
	inline int getCollisionX(const EmuTime& time) {
		sync(time);
		return collisionX;
	}

	/** Get Y coordinate of sprite collision.
	  */
	inline int getCollisionY(const EmuTime& time) {
		sync(time);
		return collisionY;
	}

	/** Reset sprite collision coordinates.
	  * This happens directly after a read, so a timestamp for syncing is
	  * not necessary.
	  */
	inline void resetCollision() {
		collisionX = collisionY = 0;
	}

	/** Signals the start of a new frame.
	  * @param time Moment in emulated time the new frame starts.
	  */
	inline void frameStart(const EmuTime& time) {
		frameStartTime.advance_fast(time);
		currentLine = 0;
		for (int i = 0; i < 313; i++) spriteCount[i] = 0;
		// TODO: Reset anything else? Does the real VDP?
	}

	/** Signals the end of the current frame.
	  * @param time Moment in emulated time the current frame ends.
	  */
	inline void frameEnd(const EmuTime& time) {
		sync(time);
	}

	/** Get sprites for a display line.
	  * Returns the contents of the line the last time it was sprite checked;
	  * before getting the sprites, you should sync to a moment in time
	  * after the sprites are checked, or you'll get last frame's sprites.
	  * @param line The absolute line number for which sprites should
	  *   be returned. Range is [0..313) for PAL and [0..262) for NTSC.
	  * @param visibleSprites Output parameter in which the pointer to
	  *   a SpriteInfo array containing the sprites to be displayed is
	  *   returned.
	  *   The array's contents are valid until the next time the VDP
	  *   is scheduled.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int getSprites(int line, const SpriteInfo*& visibleSprites) const {
		// Compensate for the fact sprites are checked one line earlier
		// than they are displayed.
		line--;

		// TODO: Is there ever a sprite on absolute line 0?
		//       Maybe there is, but it is never displayed.
		if (line < 0) return 0;

		visibleSprites = spriteBuffer[line];
		return spriteCount[line];
	}

	/** Are there any sprites on the given display line?
	  * This method works similarly to getSprites, but returning only
	  * information about sprite presence instead of the actual sprites.
	  * @param line The absolute line number for which sprites should
	  *   be returned. Range is [0..313) for PAL and [0..262) for NTSC.
	  * @return true iff one or more sprites exist on the given line.
	  */
	inline int hasSprites(int line) const {
		// Compensate for the fact sprites are checked one line earlier
		// than they are displayed.
		line--;

		// TODO: Is there ever a sprite on absolute line 0?
		//       Maybe there is, but it is never displayed.
		if (line < 0) return 0;

		return spriteCount[line] != 0;
	}

	// VRAMObserver implementation:

	void updateVRAM(unsigned offset, const EmuTime& time) {
		offset = offset; // avoid warning
		checkUntil(time);
	}

	void updateWindow(bool enabled, const EmuTime& time) {
		enabled = enabled; // avoid warning
		sync(time);
	}

private:
	/** Do not calculate sprite patterns, because this mode is spriteless.
	  */
	void updateSprites0(int limit);

	/** Calculate sprite patterns for sprite mode 1.
	  */
	void updateSprites1(int limit);

	/** Calculate sprite patterns for sprite mode 2.
	  */
	void updateSprites2(int limit);

	/** Calculates a sprite pattern.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the leftmost bit of the sprite.
	  *   Unused bits are zero.
	  */
	inline SpritePattern calculatePatternNP(unsigned patternNr, unsigned y);
	inline SpritePattern calculatePatternPlanar(unsigned patternNr, unsigned y);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 1 (MSX1).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param minLine The first line number (inclusive) for which sprites
	  *                should be checked.
	  * @param maxLine The last line number (exclusive) for which sprites
	  *                should be checked.
	  * @effect Fills in the spriteBuffer and spriteCount arrays.
	  */
	inline void checkSprites1(int minLine, int maxLine);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 2 (MSX2).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param minLine The first line number (inclusive) for which sprites
	  *                should be checked.
	  * @param maxLine The last line number (exclusive) for which sprites
	  *                should be checked.
	  * @effect Fills in the spriteBuffer and spriteCount arrays.
	  */
	inline void checkSprites2(int minLine, int maxLine);

	typedef void (SpriteChecker::*UpdateSpritesMethod)(int limit);
	UpdateSpritesMethod updateSpritesMethod;

	/** The VDP this sprite checker is part of.
	  */
	VDP& vdp;

	/** The VRAM to get sprites data from.
	  */
	VDPVRAM& vram;

	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	BooleanSetting& limitSpritesSetting;

	/** The emulation time when this frame was started (vsync).
	  */
	Clock<VDP::TICKS_PER_SECOND> frameStartTime;

	/** Sprites are checked up to and excluding this display line.
	  */
	int currentLine;

	/** X coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
	  */
	int collisionX;

	/** Y coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
	  * Bit 9 contains EO, I guess that's a copy of the even/odd flag
	  * of the frame on which the collision occurred.
	  */
	int collisionY;

	/** Buffer containing the sprites that are visible on each
	  * display line.
	  */
	SpriteInfo spriteBuffer[313][32 + 1]; // +1 for sentinel

	/** Buffer containing the number of sprites that are visible
	  * on each display line.
	  * In other words, spriteCount[i] is the number of sprites
	  * in spriteBuffer[i].
	  */
	int spriteCount[313];

	/** Is the current display mode spriteless?
	  */
	bool mode0;

	/** Is current display mode planar or not?
	  * TODO: Introduce separate update methods for planar/nonplanar modes.
	  */
	bool planar;
};

} // namespace openmsx

#endif
