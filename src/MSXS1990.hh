#ifndef MSXS1990_HH
#define MSXS1990_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class FirmwareSwitch;
class S1990Debuggable;

/**
 * This class implements the MSX-engine found in a MSX Turbo-R (S1990)
 *
 * TODO explanation
 */
class MSXS1990 final : public MSXDevice
{
public:
	explicit MSXS1990(const DeviceConfig& config);
	~MSXS1990();

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readRegister(byte reg) const;
	void writeRegister(byte reg, byte value);
	void setCPUStatus(byte value);

	const std::unique_ptr<FirmwareSwitch> firmwareSwitch;
	const std::unique_ptr<S1990Debuggable> debuggable;
	byte registerSelect;
	byte cpuStatus;

	friend class S1990Debuggable;
};

} // namespace openmsx

#endif
