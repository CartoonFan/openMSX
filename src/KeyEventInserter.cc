#include "KeyEventInserter.hh"
#include "MSXCPU.hh"

KeyEventInserter::KeyEventInserter()
{
}

KeyEventInserter &KeyEventInserter::operator<<(std::string &str)
{
	buffer += str;
	return *this;
}

KeyEventInserter &KeyEventInserter::operator<<(const char *cstr)
{
	std::string str(cstr);
	return operator<<(str);
}

void KeyEventInserter::flush(uint64 offset)
{
	SDL_Event event;
	EmuTime time(10);	// 10 Hz
	time = MSXCPU::instance()->getCurrentTime();
	for (unsigned i=0; i<buffer.length(); i++) {
		event.key.keysym.sym = (SDLKey)buffer[i];
		event.type = SDL_KEYDOWN;
		new SDLEventInserter(event, time);
		time++;
		event.type = SDL_KEYUP;
		new SDLEventInserter(event, time);
		time++;
	}
	buffer = "";
}
