#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specifc address.
 */
class DebugCondition final : public BreakPointBase
{
public:
	DebugCondition(TclObject command_, TclObject condition_, bool once_)
		: BreakPointBase(command_, condition_, once_)
		, id(++lastId) {}

	unsigned getId() const { return id; }

private:
	unsigned id;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
