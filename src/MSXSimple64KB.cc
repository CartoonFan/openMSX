// $Id$

//
// Empty , just created to have a device for the factory and a general file for new developers
//
#include "MSXSimple64KB.hh" 
#include <string>

MSXSimple64KB::MSXSimple64KB(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXSimple64KB object");
}

MSXSimple64KB::~MSXSimple64KB()
{
	PRT_DEBUG("Destructing an MSXSimple64KB object");
	delete [] memoryBank; // C++ can handle null-pointers
}

void MSXSimple64KB::start()
{
	MSXDevice::start();
}

void MSXSimple64KB::reset()
{
	MSXDevice::reset();
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(memoryBank, 0, 65536);
	}
}

void MSXSimple64KB::init()
{
	MSXDevice::init();
	
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	memoryBank = new byte[65536];
	if (memoryBank == NULL) 
		PRT_ERROR("Couldn't create 64KB memory bank !!!!!!");
	//Isn't completely true, but let's suppose that ram will 
	//always contain all zero if started
	memset(memoryBank, 0, 65536); // new doesn't fill with zero

	registerSlots();
}

byte MSXSimple64KB::readMem(word address, EmuTime &time)
{
	return memoryBank [address];
}

void MSXSimple64KB::writeMem(word address, byte value, EmuTime &time)
{
	memoryBank[address] = value;
}

