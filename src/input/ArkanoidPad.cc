#include "ArkanoidPad.hh"

#include "Event.hh"
#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include <algorithm>

// Implemented mostly according to the info here: http://www.msx.org/forumtopic7661.html
// This is absolutely not accurate, but good enough to make the pad work in the
// Arkanoid games.

// The hardware works with a 556 dual timer that's unemulated here, it requires
// short delays at each shift, and a long delay at latching. Too short delays
// will cause garbage reads, and too long delays at shifting will eventually
// cause the shift register bits to return to 0.

namespace openmsx {

static constexpr int POS_MIN = 55; // measured by hap
static constexpr int POS_MAX = 325; // measured by hap
static constexpr int POS_CENTER = 236; // approx. middle used by games
static constexpr int SCALE = 2;


class ArkanoidState final : public StateChange
{
public:
	ArkanoidState() = default; // for serialize
	ArkanoidState(EmuTime time_, int delta_, bool press_, bool release_)
		: StateChange(time_)
		, delta(delta_), press(press_), release(release_) {}
	[[nodiscard]] int  getDelta()   const { return delta; }
	[[nodiscard]] bool getPress()   const { return press; }
	[[nodiscard]] bool getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("delta",   delta,
		             "press",   press,
		             "release", release);
	}
private:
	int delta;
	bool press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, ArkanoidState, "ArkanoidState");

ArkanoidPad::ArkanoidPad(MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, dialPos(POS_CENTER)
{
}

ArkanoidPad::~ArkanoidPad()
{
	if (isPluggedIn()) {
		ArkanoidPad::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
std::string_view ArkanoidPad::getName() const
{
	return "arkanoidpad";
}

std::string_view ArkanoidPad::getDescription() const
{
	return "Arkanoid pad";
}

void ArkanoidPad::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void ArkanoidPad::unplugHelper(EmuTime /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
uint8_t ArkanoidPad::read(EmuTime /*time*/)
{
	return buttonStatus | ((shiftReg & 0x100) >> 8);
}

void ArkanoidPad::write(uint8_t value, EmuTime /*time*/)
{
	uint8_t diff = lastValue ^ value;
	lastValue = value;

	if (diff & value & 0x4) {
		// pin 8 from low to high: copy dial position into shift reg
		shiftReg = dialPos;
	}
	if (diff & value & 0x1) {
		// pin 6 from low to high: shift the shift reg
		shiftReg = (shiftReg << 1) | (shiftReg & 1); // bit 0's value is 'shifted in'
	}
}

// MSXEventListener
void ArkanoidPad::signalMSXEvent(const Event& event,
                                 EmuTime time) noexcept
{
	visit(overloaded{
		[&](const MouseMotionEvent& e) {
			int newPos = std::min(POS_MAX,
					std::max(POS_MIN,
						dialPos + e.getX() / SCALE));
			int delta = newPos - dialPos;
			if (delta != 0) {
				stateChangeDistributor.distributeNew<ArkanoidState>(
					time, delta, false, false);
			}
		},
		[&](const MouseButtonDownEvent& /*e*/) {
			// any button will press the Arkanoid Pad button
			if (buttonStatus & 2) {
				stateChangeDistributor.distributeNew<ArkanoidState>(
					time, 0, true, false);
			}
		},
		[&](const MouseButtonUpEvent& /*e*/) {
			// any button will unpress the Arkanoid Pad button
			if (!(buttonStatus & 2)) {
				stateChangeDistributor.distributeNew<ArkanoidState>(
					time, 0, false, true);
			}
		},
		[](const EventBase&) { /*ignore */}
	}, event);
}

// StateChangeListener
void ArkanoidPad::signalStateChange(const StateChange& event)
{
	const auto* as = dynamic_cast<const ArkanoidState*>(&event);
	if (!as) return;

	dialPos += as->getDelta();
	if (as->getPress())   buttonStatus &= ~2;
	if (as->getRelease()) buttonStatus |=  2;
}

void ArkanoidPad::stopReplay(EmuTime time) noexcept
{
	// TODO Get actual mouse button(s) state. Is it worth the trouble?
	int delta = POS_CENTER - dialPos;
	bool release = (buttonStatus & 2) == 0;
	if ((delta != 0) || release) {
		stateChangeDistributor.distributeNew<ArkanoidState>(
			time, delta, false, release);
	}
}


// version 1: Initial version, the variables dialPos and buttonStatus were not
//            serialized.
// version 2: Also serialize the above variables, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void ArkanoidPad::serialize(Archive& ar, unsigned version)
{
	ar.serialize("shiftreg",  shiftReg,
	             "lastValue", lastValue);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("dialpos",      dialPos,
		             "buttonStatus", buttonStatus);
	}
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(ArkanoidPad);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, ArkanoidPad, "ArkanoidPad");

} // namespace openmsx
