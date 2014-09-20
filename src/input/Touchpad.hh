#ifndef TOUCHPAD_HH
#define TOUCHPAD_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include <memory>

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class CommandController;
class StringSetting;
class TclObject;
class Interpreter;

class Touchpad final : public JoystickDevice, private MSXEventListener
                     , private StateChangeListener
{
public:
	Touchpad(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         CommandController& commandController);
	~Touchpad();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTouchpadStateChange(EmuTime::param time,
		byte x, byte y, bool touch, bool button);
	void parseTransformMatrix(Interpreter& interp, const TclObject& value);
	void transformCoords(int& x, int& y);

	// Pluggable
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	std::unique_ptr<StringSetting> transformSetting;
	double m[2][3]; // transformation matrix

	EmuTime start; // last time when CS switched 0->1
	int hostX, hostY; // host state
	byte hostButtons; //
	byte x, y;          // msx state (different from host state
	bool touch, button; //    during replay)
	byte shift; // shift register to both transmit and receive data
	byte channel; // [0..3]   0->x, 3->y, 1,2->not used
	byte last; // last written data, to detect transitions
};

} // namespace openmsx

#endif
