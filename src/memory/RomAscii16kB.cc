// $Id$

// ASCII 16kB cartridges
//
// this type is used in a few cartridges.
// example of cartridges: Xevious, Fantasy Zone 2,
// Return of Ishitar, Androgynus, Gallforce ...
//
// The address to change banks:
//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

#include "RomAscii16kB.hh"


RomAscii16kB::RomAscii16kB(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	reset(time);
}

RomAscii16kB::~RomAscii16kB()
{
}

void RomAscii16kB::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setRom (1, 0);
	setRom (2, 0);
	setBank(3, unmapped);
}

void RomAscii16kB::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x6000 <= address) && (address < 0x7800) && !(address & 0x0800)) {
		byte region = ((address >> 12) & 1) + 1;
		setRom(region, value);
	}
}
