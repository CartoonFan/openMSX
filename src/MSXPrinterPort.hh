#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"

namespace openmsx {

class PrinterPortDevice;

class MSXPrinterPort final : public MSXDevice, public Connector
{
public:
	explicit MSXPrinterPort(const DeviceConfig& config);
	~MSXPrinterPort();

	PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// Connector
	const std::string getDescription() const override;
	string_ref getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(byte newData, EmuTime::param time);

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
