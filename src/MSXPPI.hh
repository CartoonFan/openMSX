// This class implements the PPI (8255)
//
//   PPI    MSX-I/O  Direction  MSX-Function
//  PortA    0xA8      Out     Memory primary slot register
//  PortB    0xA9      In      Keyboard column inputs
//  PortC    0xAA      Out     Keyboard row select / CAPS / CASo / CASm / SND
//  Control  0xAB     In/Out   Mode select for PPI
//
//  Direction indicates the direction normally used on MSX.
//  Reading from an output port returns the last written byte.
//  Writing to an input port has no immediate effect.
//
//  PortA combined with upper half of PortC form groupA
//  PortB               lower                    groupB
//  GroupA can be in programmed in 3 modes
//   - basic input/output
//   - strobed input/output
//   - bidirectional
//  GroupB can only use the first two modes.
//  Only the first mode is used on MSX, only this mode is implemented yet.
//
//  for more detail see
//    http://w3.qahwah.net/joost/openMSX/8255.pdf

#ifndef MSXPPI_HH
#define MSXPPI_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include <memory>

namespace openmsx {

class I8255;
class KeyClick;
class CassettePortInterface;
class RenShaTurbo;
class Keyboard;

class MSXPPI final : public MSXDevice, public I8255Interface
{
public:
	explicit MSXPPI(const DeviceConfig& config);
	~MSXPPI();

	void reset(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	byte readA(EmuTime::param time) override;
	byte readB(EmuTime::param time) override;
	nibble readC0(EmuTime::param time) override;
	nibble readC1(EmuTime::param time) override;
	byte peekA(EmuTime::param time) const override;
	byte peekB(EmuTime::param time) const override;
	nibble peekC0(EmuTime::param time) const override;
	nibble peekC1(EmuTime::param time) const override;
	void writeA(byte value, EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;
	void writeC0(nibble value, EmuTime::param time) override;
	void writeC1(nibble value, EmuTime::param time) override;

	CassettePortInterface& cassettePort;
	RenShaTurbo& renshaTurbo;
	const std::unique_ptr<I8255> i8255;
	const std::unique_ptr<KeyClick> click;
	const std::unique_ptr<Keyboard> keyboard;
	nibble prevBits;
	nibble selectedRow;
};

} // namespace openmsx

#endif
