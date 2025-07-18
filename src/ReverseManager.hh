#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "EmuTime.hh"

#include "MemBuffer.hh"
#include "DeltaBlock.hh"
#include "outer.hh"

#include <cstdint>
#include <deque>
#include <span>
#include <map>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

namespace openmsx {

class EventDelay;
class EventDistributor;
class Interpreter;
class MSXMotherBoard;
class StateChange;
class TclObject;

class ReverseManager final : private EventListener
{
public:
	static constexpr std::string_view REPLAY_DIR = "replays";
	static constexpr std::string_view REPLAY_EXTENSION = ".omr";

public:
	explicit ReverseManager(MSXMotherBoard& motherBoard);
	~ReverseManager();

	// To not loose any events we need to flush delayed events before
	// switching machine. See comments in goTo() for more info.
	void registerEventDelay(EventDelay& eventDelay_) {
		eventDelay = &eventDelay_;
	}

	// Should only be used by MSXMotherBoard to be able to transfer
	// reRecordCount to ReverseManager for version 2 of MSXMotherBoard
	// serializers.
	void setReRecordCount(unsigned count) {
		reRecordCount = count;
	}

	[[nodiscard]] bool isReplaying() const;
	void stopReplay(EmuTime time) noexcept;

	template<typename T, typename... Args>
	StateChange& record(EmuTime time, Args&& ...args) {
		assert(!isReplaying());
		++replayIndex;
		history.events.push_back(std::make_unique<T>(time, std::forward<Args>(args)...));
		return *history.events.back();
	}

	[[nodiscard]] bool isCollecting() const { return collecting; }
	[[nodiscard]] bool isViewOnlyMode() const;
	void setViewOnlyMode(bool value);
	[[nodiscard]] double getBegin() const;
	[[nodiscard]] double getEnd() const;
	[[nodiscard]] double getCurrent() const;
	[[nodiscard]] auto getSnapshotTimes() const {
		return std::views::transform(history.chunks, [](auto& p) {
			return (p.second.time - EmuTime::zero()).toDouble();
		});
	}

private:
	struct ReverseChunk {
		EmuTime time = EmuTime::zero();
		std::vector<std::shared_ptr<DeltaBlock>> deltaBlocks;
		MemBuffer<uint8_t> savestate;

		// Number of recorded events (or replay index) when this
		// snapshot was created. So when going back replay should
		// start at this index.
		unsigned eventCount;
	};
	using Chunks = std::map<unsigned, ReverseChunk>;
	using Events = std::deque<std::unique_ptr<StateChange>>;

	struct ReverseHistory {
		void swap(ReverseHistory& other) noexcept;
		void clear();
		[[nodiscard]] unsigned getNextSeqNum(EmuTime time) const;

		Chunks chunks;
		Events events;
		LastDeltaBlocks lastDeltaBlocks;
	};

	void start();
	void stop();
	void status(TclObject& result) const;
	void debugInfo(TclObject& result) const;
	void goBack(std::span<const TclObject> tokens);
	void goTo(std::span<const TclObject> tokens);
	void saveReplay(Interpreter& interp,
	                std::span<const TclObject> tokens, TclObject& result);
	void loadReplay(Interpreter& interp,
	                std::span<const TclObject> tokens, TclObject& result);

	void signalStopReplay(EmuTime time);
	[[nodiscard]] EmuTime getEndTime(const ReverseHistory& history) const;
	void goTo(EmuTime targetTime, bool noVideo);
	void goTo(EmuTime targetTime, bool noVideo,
	          ReverseHistory& history, bool sameTimeLine);
	void transferHistory(ReverseHistory& oldHistory,
	                     unsigned oldEventCount);
	void transferState(MSXMotherBoard& newBoard);
	void takeSnapshot(EmuTime time);
	void schedule(EmuTime time);
	void replayNextEvent();
	template<unsigned N> void dropOldSnapshots(unsigned count);

	// Schedulable
	struct SyncNewSnapshot final : Schedulable {
		friend class ReverseManager;
		explicit SyncNewSnapshot(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime /*time*/) override {
			auto& rm = OUTER(ReverseManager, syncNewSnapshot);
			rm.execNewSnapshot();
		}
	} syncNewSnapshot;
	struct SyncInputEvent final : Schedulable {
		friend class ReverseManager;
		explicit SyncInputEvent(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime /*time*/) override {
			auto& rm = OUTER(ReverseManager, syncInputEvent);
			rm.execInputEvent();
		}
	} syncInputEvent;

	void execNewSnapshot();
	void execInputEvent();
	[[nodiscard]] EmuTime getCurrentTime() const { return syncNewSnapshot.getCurrentTime(); }

	// EventListener
	bool signalEvent(const Event& event) override;

private:
	MSXMotherBoard& motherBoard;
	EventDistributor& eventDistributor;

	struct ReverseCmd final : Command {
		explicit ReverseCmd(CommandController& controller);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} reverseCmd;

	EventDelay* eventDelay = nullptr;
	ReverseHistory history;
	unsigned replayIndex = 0;
	bool collecting = false;
	bool pendingTakeSnapshot = false;

	unsigned reRecordCount = 0;

	friend struct Replay;
};

} // namespace openmsx

#endif
