// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "SDL/SDL_image.h"
#include "SDLConsole.hh"
#include "CommandController.hh"
#include "HotKey.hh"
#include "ConsoleManager.hh"
#include "FileOpener.hh"
#include "SDLFont.hh"
#include "EventDistributor.hh"
#include "MSXConfig.hh"


SDLConsole::SDLConsole(SDL_Surface *screen) :
	consoleCmd(this)
{
	isVisible = false;
	blink = false;
	lastBlinkTime = 0;
	
	outputScreen = screen;
	backgroundImage = NULL;
	consoleSurface  = NULL;
	inputBackground = NULL;
	consoleAlpha = SDL_ALPHA_OPAQUE;
	
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("Console");
	std::string fontName = config->getParameter("font");
	font = new SDLFont(FileOpener::findFileName(fontName));
	
	SDL_Rect rect;
	rect.x = (screen->w / 32);
	rect.w = (screen->w / 32) * 30;
	rect.y = (screen->h / 15) * 9;
	rect.h = (screen->h / 15) * 6;
	resize(rect);
	alpha(180);
	SDL_EnableUNICODE(1);

	try {
		std::string backgroundName = config->getParameter("background");
		background(FileOpener::findFileName(backgroundName), 0, 0);
	} catch(MSXException &e) {
		// no background or missing file
	}

	ConsoleManager::instance()->registerConsole(this);
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this);
	CommandController::instance()->registerCommand(consoleCmd, "console");
	HotKey::instance()->registerHotKeyCommand(Keys::K_F10, "console");
}

SDLConsole::~SDLConsole()
{
	PRT_DEBUG("Destroying SDLConsole");
	HotKey::instance()->unregisterHotKeyCommand(Keys::K_F10, "console");
	CommandController::instance()->unregisterCommand("console");
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this);
	ConsoleManager::instance()->unregisterConsole(this);
	delete font;
}


// Takes keys from the keyboard and inputs them to the console
bool SDLConsole::signalEvent(SDL_Event &event)
{
	if (!isVisible)
		return true;
	if (event.type == SDL_KEYUP)
		return false;	// don't pass event to MSX-Keyboard
	
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	switch (key) {
		case Keys::K_PAGEUP:
			scrollUp();
			break;
		case Keys::K_PAGEDOWN:
			scrollDown();
			break;
		case Keys::K_UP:
			prevCommand();
			break;
		case Keys::K_DOWN:
			nextCommand();
			break;
		case Keys::K_BACKSPACE:
			backspace();
			break;
		case Keys::K_TAB:
			tabCompletion();
			break;
		case Keys::K_RETURN:
			commandExecute();
			break;
		default:
			if (event.key.keysym.unicode)
				normalKey((char)event.key.keysym.unicode);
	}
	updateConsole();
	return false;	// don't pass event to MSX-Keyboard
}

// Updates the console buffer
void SDLConsole::updateConsole()
{
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));

	// draw the background image if there is one
	if (backgroundImage) {
		SDL_Rect destRect;
		destRect.x = backX;
		destRect.y = backY;
		destRect.w = backgroundImage->w;
		destRect.h = backgroundImage->h;
		SDL_BlitSurface(backgroundImage, NULL, consoleSurface, &destRect);
	}

	int screenlines = consoleSurface->h / font->height();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size())
			font->drawText(lines[num], consoleSurface, CHAR_BORDER,
			               consoleSurface->h - (1+loop)*font->height());
	}
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!isVisible) return;
	
	drawCursor();

	// Setup the rect the console is being blitted into based on the output screen
	SDL_Rect destRect;
	destRect.x = dispX;
	destRect.y = dispY;
	destRect.w = consoleSurface->w;
	destRect.h = consoleSurface->h;
	SDL_BlitSurface(consoleSurface, NULL, outputScreen, &destRect);
}



// Draws the command line the user is typing in to the screen
void SDLConsole::drawCursor()
{
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
		if (consoleScrollBack > 0)
			return;
		int cursorLocation = lines[0].length();
		if (blink) {
			// Print cursor if there is enough room
			font->drawText(std::string("_"), consoleSurface, 
				      CHAR_BORDER + cursorLocation * font->width(),
				      consoleSurface->h - font->height());
		} else {
			// Remove cursor
			SDL_Rect rect;
			rect.x = cursorLocation * font->width() + CHAR_BORDER;
			rect.y = consoleSurface->h - font->height();
			rect.w = font->width();
			rect.h = font->height();
			SDL_FillRect(consoleSurface, &rect, 
				     SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
			if (backgroundImage) {
				// draw the background image if applicable
				SDL_Rect rect2;
				rect2.x = cursorLocation * font->width() + CHAR_BORDER;
				rect.x = rect2.x - backX;
				rect2.y = consoleSurface->h - font->height();
				rect.y = rect2.y - backY;
				rect2.w = rect.w = font->width();
				rect2.h = rect.h = font->height();
				SDL_BlitSurface(backgroundImage, &rect, consoleSurface, &rect2);
			}
		}
	}
}

// Sets the alpha level of the console, 0 turns off alpha blending
void SDLConsole::alpha(unsigned char alpha)
{
	// store alpha as state!
	consoleAlpha = alpha;
	if (alpha == 0)
		SDL_SetAlpha(consoleSurface, 0,            alpha);
	else
		SDL_SetAlpha(consoleSurface, SDL_SRCALPHA, alpha);
	updateConsole();
}

// Adds  background image to the console
//  x and y are based on console x and y
void SDLConsole::background(const std::string &image, int x, int y)
{
	SDL_Surface *temp;
	if (!(temp = IMG_Load(image.c_str())))
		return;
	if (backgroundImage)
		SDL_FreeSurface(backgroundImage);
	backgroundImage = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	backX = x;
	backY = y;
	reloadBackground();
}

// resizes the console, has to reset alot of stuff
void SDLConsole::resize(SDL_Rect rect)
{
	// make sure that the size of the console is valid
	assert (!(rect.w > outputScreen->w || rect.w < font->width() * 32));
	assert (!(rect.h > outputScreen->h || rect.h < font->height()));

	SDL_Surface *temp1 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	SDL_Surface *temp2 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->height(), 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp1 == NULL || temp2 == NULL)
		return;
	if (consoleSurface)  SDL_FreeSurface(consoleSurface);
	if (inputBackground) SDL_FreeSurface(inputBackground);
	consoleSurface = SDL_DisplayFormat(temp1);
	inputBackground = SDL_DisplayFormat(temp2);
	SDL_FreeSurface(temp1);
	SDL_FreeSurface(temp2);
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
	
	consoleScrollBack = 0;	// dependent on previous size
	position(rect.x, rect.y);
	reloadBackground();
}

// takes a new x and y of the top left of the console window
void SDLConsole::position(int x, int y)
{
	assert (!(x<0 || x > outputScreen->w - consoleSurface->w));
	assert (!(y<0 || y > outputScreen->h - consoleSurface->h));
	dispX = x;
	dispY = y;
}

void SDLConsole::reloadBackground()
{
	SDL_FillRect(inputBackground, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
	if (backgroundImage) {
		SDL_Rect src;
			src.x = 0;
			src.y = consoleSurface->h - font->height() - backY;
			src.w = backgroundImage->w;
			src.h = inputBackground->h;
		SDL_Rect dest;
			dest.x = backX;
			dest.y = 0;
			dest.w = backgroundImage->w;
			dest.h = font->height();
		SDL_BlitSurface(backgroundImage, &src, inputBackground, &dest);
	}
}


// Console command
SDLConsole::ConsoleCmd::ConsoleCmd(SDLConsole *cons)
{
	console = cons;
}

void SDLConsole::ConsoleCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 1:
		console->isVisible = !console->isVisible;
		break;
	case 2:
		if (tokens[1] == "on") {
			console->isVisible = true;
			break;
		} 
		if (tokens[1] == "off") {
			console->isVisible = false;
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}
void SDLConsole::ConsoleCmd::help   (const std::vector<std::string> &tokens)
{
	print("This command turns console display on/off");
	print(" console:     toggle console display");
	print(" console on:  show console display");
	print(" console off: remove console display");
} 
