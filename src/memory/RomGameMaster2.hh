#ifndef ROMGAMEMASTER2_HH
#define ROMGAMEMASTER2_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGameMaster2 final : public Rom4kBBlocks
{
public:
	RomGameMaster2(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomGameMaster2();

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	word sramOffset;
	bool sramEnabled;
};

} // namespace openmsx

#endif
