// $Id$

#include "SDLVisibleSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "OSDGUILayer.hh"
#include "build-info.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen)
{
#if PLATFORM_GP2X
	int flags = SDL_HWSURFACE;
#else
	int flags = SDL_SWSURFACE; // Why did we use a SW surface again?
#endif
	if (fullscreen) flags |= SDL_FULLSCREEN;

	createSurface(width, height, flags);
	SDL_Surface* surface = getSDLSurface();
	memcpy(&getSDLFormat(), surface->format, sizeof(SDL_PixelFormat));

	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

void SDLVisibleSurface::init()
{
	// nothing
}

void SDLVisibleSurface::drawFrameBuffer()
{
	// nothing
}

void SDLVisibleSurface::finish()
{
	unlock();
	SDL_Flip(getSDLSurface());
}

void SDLVisibleSurface::takeScreenShot(const std::string& filename)
{
	lock();
	try {
		ScreenShotSaver::save(getSDLSurface(), filename);
	} catch (CommandException& e) {
		throw;
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createSnowLayer()
{
	switch (getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
	case 2:
		return std::auto_ptr<Layer>(new SDLSnow<word>(*this));
#endif
#if HAVE_32BPP
	case 4:
		return std::auto_ptr<Layer>(new SDLSnow<unsigned>(*this));
#endif
	default:
		assert(false);
		return std::auto_ptr<Layer>(); // avoid warning
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor)
{
	return std::auto_ptr<Layer>(new SDLConsole(reactor, *this));
}

std::auto_ptr<Layer> SDLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::auto_ptr<Layer>(new SDLOSDGUILayer(gui));
}

} // namespace openmsx
