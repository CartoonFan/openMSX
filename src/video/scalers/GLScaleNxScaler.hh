#ifndef GLSCALENXSCALER_HH
#define GLSCALENXSCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLScaleNxScaler final : public GLScaler, private noncopyable
{
public:
	explicit GLScaleNxScaler(GLScaler& fallback);

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight) override;

private:
	GLScaler& fallback;
};

} // namespace openmsx

#endif // GLSCALENXSCALER_HH
