// $Id$

#include "DummyMidiOutDevice.hh"


namespace openmsx {

void DummyMidiOutDevice::recvByte(byte value, const EmuTime& time)
{
	// ignore
}

const string& DummyMidiOutDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyMidiOutDevice::plugHelper(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyMidiOutDevice::unplugHelper(const EmuTime& time)
{
}

} // namespace openmsx
