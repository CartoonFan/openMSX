#ifndef MSXPRINTERPORTLOGGER_HH
#define MSXPRINTERPORTLOGGER_HH

#include "PrinterPortDevice.hh"
#include <memory>

namespace openmsx {

class CommandController;
class File;
class FilenameSetting;

class PrinterPortLogger final : public PrinterPortDevice
{
public:
	explicit PrinterPortLogger(CommandController& commandController);
	~PrinterPortLogger();

	// PrinterPortDevice
	bool getStatus(EmuTime::param time) override;
	void setStrobe(bool strobe, EmuTime::param time) override;
	void writeData(byte data, EmuTime::param time) override;

	// Pluggable
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<FilenameSetting> logFilenameSetting;
	std::unique_ptr<File> file;
	byte toPrint;
	bool prevStrobe;
};

} // namespace openmsx

#endif
