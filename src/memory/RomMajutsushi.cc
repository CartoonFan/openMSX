// $Id$

// KONAMI4 8kB cartridges
// 
// this type is used by Konami cartridges that do not have an SCC and some others
// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
// The Maze of Galious, Aleste 1, 1942, Heaven, Mystery World, ...
//
// page at 4000 is fixed, other banks are switched
// by writting at 0x6000,0x8000 and 0xa000

#include "RomMajutsushi.hh"
#include "Device.hh"
#include "DACSound8U.hh"


namespace openmsx {

RomMajutsushi::RomMajutsushi(Device* config, const EmuTime& time, Rom* rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom)
{
	short volume = (short)config->getParameterAsInt("volume");
	dac = new DACSound8U(config->getId(), "Majutsushi DAC", volume, time);
	
	reset(time);
}

RomMajutsushi::~RomMajutsushi()
{
	delete dac;
}

void RomMajutsushi::reset(const EmuTime& time)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);

	dac->reset(time);
}

void RomMajutsushi::writeMem(word address, byte value, const EmuTime& time)
{
	if ((0x6000 <= address) && (address < 0xC000)) {
		setRom(address >> 13, value);
	} else if ((0x5000 <= address) && (address < 0x6000)) {
		dac->writeDAC(value, time);
	}
}

byte* RomMajutsushi::getWriteCacheLine(word address) const
{
	if ((0x5000 <= address) && (address < 0xC000)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
