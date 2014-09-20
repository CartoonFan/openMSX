#ifndef ROMZEMINA80IN1_HH
#define ROMZEMINA80IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina80in1 final : public Rom8kBBlocks
{
public:
	RomZemina80in1(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;
};

} // namespace openmsx

#endif
