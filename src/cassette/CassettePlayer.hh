// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "EventListener.hh"
#include "CassetteDevice.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "Schedulable.hh"
#include "Filename.hh"
#include "EmuTime.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class CassetteImage;
class MSXMixer;
class Scheduler;
class CliComm;
class Wav8Writer;
class LoadingIndicator;
class ThrottleManager;
class BooleanSetting;
class TapeCommand;
class CommandController;
class MSXEventDistributor;
class EventDistributor;

class CassettePlayer : public CassetteDevice, public SoundDevice
                     , private Resample, private EventListener
                     , private Schedulable
{
public:
	CassettePlayer(CommandController& commandController,
	               MSXMixer& mixer, Scheduler& Scheduler,
	               MSXEventDistributor& msxEventDistributor,
	               EventDistributor& eventDistributor,
	               CliComm& cliComm,
	               EnumSetting<ResampleType>& resampleSetting,
	               ThrottleManager& throttleManager);
	virtual ~CassettePlayer();

	// CassetteDevice
	virtual void setMotor(bool status, EmuTime::param time);
	virtual short readSample(EmuTime::param time);
	virtual void setSignal(bool output, EmuTime::param time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		EmuTime::param time, EmuDuration::param sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	enum State { PLAY, RECORD, STOP }; // public for serialization

private:
	State getState() const;
	std::string getStateString() const;
	void setState(State newState, const Filename& newImage,
	              EmuTime::param time);
	void setImageName(const Filename& newImage);
	const Filename& getImageName() const;
	void checkInvariants() const;

	/** Insert a tape for use in PLAY mode.
	 */
	void playTape(const Filename& filename, EmuTime::param time);
	void insertTape(const Filename& filename);

	/** Removes tape (possibly stops recording). And go to STOP mode.
	 */
	void removeTape(EmuTime::param time);

	/** Goes to RECORD mode using the given filename as a new tape
	  * image. Finishes any old recording session.
	  */
	void recordTape(const Filename& filename, EmuTime::param time);

	/** Rewinds the tape. Also sets PLAY mode, because you can't record
	  * over an existing tape. (And it won't be useful to implement that
	  * anyway.)
	  */
	void rewind(EmuTime::param time);

	/** Enable or disable motor control.
	 */
	void setMotorControl(bool status, EmuTime::param time);

	/** True when the tape is rolling: not in STOP mode, AND [ motorcontrol
	  * is disabled OR motor is on ].
	  */
	bool isRolling() const;

	/** If motor, motorControl or state is changed, this method should
	  * be called to update the end-of-tape syncpoint and the loading
	  * indicator.
	  */
	void updateLoadingState(EmuTime::param time);

	/** Returns the position of the tape, in seconds from the
	  * beginning of the tape. */
	double getTapePos(EmuTime::param time);

	/** Returns the length of the tape in seconds.
	  * When no tape is inserted, this returns 0. While recording this
	  * returns the current position (so while recording, tape length grows
	  * continuously). */
	double getTapeLength(EmuTime::param time);

	void sync(EmuTime::param time);
	void updateTapePosition(EmuDuration::param duration, EmuTime::param time);
	void generateRecordOutput(EmuDuration::param duration);

	void fillBuf(size_t length, double x);
	void flushOutput();
	void autoRun();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	static const size_t BUF_SIZE = 1024;
	unsigned char buf[BUF_SIZE];

	double lastX; // last unfiltered output
	double lastY; // last filtered output
	double partialOut;
	double partialInterval;

	/** The time in the world of the tape. Zero at the start of the tape. */
	EmuTime tapePos;

	/** Last time the sync() method was called.
	  * Used to calculate EmuDuration since last sync. */
	EmuTime prevSyncTime;

	// SoundDevice
	unsigned audioPos;
	unsigned outputRate;
	Filename casImage;

	CommandController& commandController;
	CliComm& cliComm;
	EventDistributor& eventDistributor;

	const std::auto_ptr<TapeCommand> tapeCommand;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;
	const std::auto_ptr<BooleanSetting> autoRunSetting;
	std::auto_ptr<Wav8Writer> recordImage;
	std::auto_ptr<CassetteImage> playImage;

	size_t sampcnt;
	State state;
	bool lastOutput;
	bool motor, motorControl;
	bool syncScheduled;

	friend class TapeCommand;
};

} // namespace openmsx

#endif
