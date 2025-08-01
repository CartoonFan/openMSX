#ifndef VIDEOLAYER_HH
#define VIDEOLAYER_HH

#include "VideoSourceSetting.hh"
#include "Layer.hh"

#include "Observer.hh"
#include "MSXEventListener.hh"

#include <cstdint>
#include <string>
#include <optional>

namespace openmsx {

class MSXMotherBoard;
class Display;
class Setting;
class BooleanSetting;

class VideoLayer : public Layer, protected Observer<Setting>
                 , private MSXEventListener
{
public:
	VideoLayer(const VideoLayer&) = delete;
	VideoLayer(VideoLayer&&) = delete;
	VideoLayer& operator=(const VideoLayer&) = delete;
	VideoLayer& operator=(VideoLayer&&) = delete;

	/** Returns the ID for this VideoLayer.
	  * These IDs are globally unique. The 'videosource' setting uses
	  * these IDs as possible values.
	  */
	[[nodiscard]] int getVideoSource() const;
	[[nodiscard]] int getVideoSourceSetting() const;

	/** Create a raw (=non-post-processed) screenshot. The 'height'
	 * parameter should be either '240' or '480' if specified. If not
	 * specified, the height will be determined based on the available
	 * widths in the raw frame. The result will be scaled to either
	 * '320x240' or '640x480' and written to a png file.
	 */
	virtual void takeRawScreenShot(
		std::optional<unsigned> height, const std::string& filename) = 0;

	// We used to test whether a Layer is active by looking at the
	// Z-coordinate (Z_MSX_ACTIVE vs Z_MSX_PASSIVE). Though in case of
	// Video9000 it's possible the Video9000 layer is selected, but we
	// still need to render this layer (the v99x8 or v9990 layer).
	enum class Video9000Active : uint8_t { NO, FRONT, BACK };
	void setVideo9000Active(int video9000Source_, Video9000Active active) {
		video9000Source = video9000Source_;
		activeVideo9000 = active;
	}
	[[nodiscard]] bool needRender() const;
	[[nodiscard]] bool needRecord() const;

protected:
	VideoLayer(MSXMotherBoard& motherBoard,
	           const std::string& videoSource);
	~VideoLayer() override;

	// Observer<Setting> interface:
	void update(const Setting& setting) noexcept override;

private:
	/** Calculates the current Z coordinate of this layer. */
	void calcZ();
	/** Calculates the current coverage of this layer. */
	void calcCoverage();

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime time) noexcept override;

private:
	/** This layer belongs to a specific machine. */
	MSXMotherBoard& motherBoard;
	/** Settings shared between all renderers. */
	Display& display;
	/** Reference to "videosource" setting. */
	VideoSourceSetting& videoSourceSetting;
	/** Activate the videosource */
	VideoSourceActivator videoSourceActivator;
	/** Reference to "power" setting. */
	BooleanSetting& powerSetting;

	/** Video source ID of the Video9000 layer. */
	int video9000Source = 0;
	/** Active when Video9000 is shown. */
	Video9000Active activeVideo9000 = Video9000Active::NO;
};

} // namespace openmsx

#endif
