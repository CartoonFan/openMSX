#ifndef DUMMYAUDIOINPUTDEVICE_HH
#define DUMMYAUDIOINPUTDEVICE_HH

#include "AudioInputDevice.hh"

namespace openmsx {

class DummyAudioInputDevice final : public AudioInputDevice
{
public:
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	short readSample(EmuTime::param time) override;
};

} // namespace openmsx

#endif
