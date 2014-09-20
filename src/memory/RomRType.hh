#ifndef ROMRTYPE_HH
#define ROMRTYPE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRType final : public Rom16kBBlocks
{
public:
	RomRType(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;
};

} // namespace openmsx

#endif
