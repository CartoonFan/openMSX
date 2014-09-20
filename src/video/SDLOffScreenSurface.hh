#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

class SDLOffScreenSurface final : public OutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);
	~SDLOffScreenSurface();

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void clearScreen() override;

	SDLSurfacePtr surface;
	void* buffer;
};

} // namespace openmsx

#endif
