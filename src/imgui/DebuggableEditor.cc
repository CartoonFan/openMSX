#include "DebuggableEditor.hh"

#include "ImGuiCpp.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiManager.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiSettings.hh"
#include "ImGuiUtils.hh"
#include "Shortcuts.hh"

#include "CommandException.hh"
#include "Debuggable.hh"
#include "Debugger.hh"
#include "Display.hh"
#include "File.hh"
#include "Interpreter.hh"
#include "MSXMotherBoard.hh"
#include "SymbolManager.hh"
#include "TclObject.hh"
#include "VideoSystem.hh"

#include "enumerate.hh"
#include "narrow.hh"
#include "unreachable.hh"

#include "CustomFont.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <span>

namespace openmsx {

using namespace std::literals;

static constexpr int MidColsCount = 8; // extra spacing between every mid-cols.
static constexpr auto HighlightColor = IM_COL32(255, 255, 255, 50); // background color of highlighted bytes.
static constexpr auto HighlightSymbolColor = IM_COL32(148, 95, 35, 255); // background color of known symbol bytes.

DebuggableEditor::DebuggableEditor(ImGuiManager& manager_, std::string debuggableName_, size_t index)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
	, title(std::move(debuggableName_))
{
	debuggableNameSize = title.size();
	if (index) {
		strAppend(title, " (", index + 1, ')');
	}
}

void DebuggableEditor::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void DebuggableEditor::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
	parseSearchString(searchString);
}

void DebuggableEditor::loadEnd()
{
	updateAddr = true;
}

DebuggableEditor::Sizes DebuggableEditor::calcSizes(unsigned memSize) const
{
	Sizes s;
	const auto& style = ImGui::GetStyle();

	s.addrDigitsCount = 0;
	for (unsigned n = memSize - 1; n > 0; n >>= 4) {
		++s.addrDigitsCount;
	}

	s.lineHeight = ImGui::GetTextLineHeight();
	s.glyphWidth = ImGui::CalcTextSize("F").x + 1;            // We assume the font is mono-space
	s.hexCellWidth = truncf(s.glyphWidth * 2.5f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
	s.spacingBetweenMidCols = truncf(s.hexCellWidth * 0.25f); // Every 'MidColsCount' columns we add a bit of extra spacing
	s.posHexStart = float(s.addrDigitsCount + 2) * s.glyphWidth;
	auto posHexEnd = s.posHexStart + (s.hexCellWidth * float(columns));
	s.posAsciiStart = s.posAsciiEnd = posHexEnd;
	if (showAscii) {
		int numMacroColumns = (columns + MidColsCount - 1) / MidColsCount;
		s.posAsciiStart = posHexEnd + s.glyphWidth + float(numMacroColumns) * s.spacingBetweenMidCols;
		s.posAsciiEnd = s.posAsciiStart + float(columns) * s.glyphWidth;
	}
	s.windowWidth = s.posAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.glyphWidth;
	return s;
}

void DebuggableEditor::makeSnapshot(MSXMotherBoard& motherBoard)
{
	auto& debugger = motherBoard.getDebugger();
	if (auto* debuggable = debugger.findDebuggable(getDebuggableName())) {
		makeSnapshot(*debuggable);
	}
}
void DebuggableEditor::makeSnapshot(Debuggable& debuggable)
{
	snapshot.resize(debuggable.getSize());
	debuggable.readBlock(0, snapshot);
}

void DebuggableEditor::paint(MSXMotherBoard* motherBoard)
{
	if (!open || !motherBoard) return;
	auto& debugger = motherBoard->getDebugger();
	auto* debuggable = debugger.findDebuggable(getDebuggableName());
	if (!debuggable) return;

	unsigned memSize = debuggable->getSize();
	columns = std::min(columns, narrow<int>(memSize));
	auto s = calcSizes(memSize);

	if (showExportWindow) {
		drawExport(s, *debuggable);
	}

	im::ScopedFont sf(manager.fontMono);

	ImGui::SetNextWindowSize(ImVec2(s.windowWidth, s.windowWidth * 0.60f), ImGuiCond_FirstUseEver);

	im::Window(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar, [&]{
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
		    ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("context");
		}
		drawContents(s, *debuggable, memSize);
	});
}

void DebuggableEditor::drawExport(const Sizes& s, Debuggable& debuggable)
{
	auto& interp = manager.getInterpreter();
	const auto& style = ImGui::GetStyle();

	im::Window(tmpStrCat("Export ", title).c_str(), &showExportWindow, [&]{
		if (ImGui::IsWindowAppearing()) {
			exportBegin = strCat("0x", formatAddr(s, 0));
			exportEnd   = strCat("0x", formatAddr(s, debuggable.getSize() - 1));
		}

		std::optional<unsigned> begin, end;
		std::string formatError;

		ImGui::SeparatorText("Input");

		ImGui::RadioButton("All", &exportRange, RANGE_ALL);
		if (exportRange == RANGE_ALL) {
			begin = 0;
			end = debuggable.getSize() - 1;
		}

		ImGui::RadioButton("Range   ", &exportRange, RANGE_RANGE);
		auto handleAddress = [&](std::string_view label, std::string& text, int lowerBound) {
			std::optional<unsigned> addr;
			std::string error;
			try {
				auto i = TclObject(text).eval(interp).getInt(interp);
				if ((i < 0) || (i >= narrow<int>(debuggable.getSize()))) {
					error = "out of range";
				} else if (i < lowerBound) {
					error = "'end' smaller than 'begin'";
				} else {
					addr = narrow<unsigned>(i);
				}
			} catch (MSXException& e) {
				error = e.getMessage();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(label);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
			im::StyleColor(!addr, ImGuiCol_Text, getColor(imColor::ERROR), [&]{
				ImGui::InputText(tmpStrCat("##", label).c_str(), &text);
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					text = strCat("0x", formatAddr(s, currentAddr));
				}
				simpleToolTip([&]{
					return addr ? strCat("0x", formatAddr(s, *addr))
					            : error;
				});
			});
			return addr;
		};
		im::Disabled(exportRange != RANGE_RANGE, [&]{
			begin = handleAddress("begin:", exportBegin, 0);
			end =   handleAddress("end:", exportEnd, begin.value_or(0));
		});
		HelpMarker("Both 'begin' and 'end' are inclusive.\n"
		           "You can enter an expression line '0x100 + 7' or '[reg HL]'.\n"
		           "Right-click to enter the current cursor position.");

		ImGui::SeparatorText("Format");

		ImGui::RadioButton("Raw", &exportFormatted, EXPORT_RAW);
		HelpMarker("Produce binary (unformatted) output");

		ImGui::RadioButton("Formatted", &exportFormatted, EXPORT_FORMATTED);
		HelpMarker("Produce formatted ASCII output");
		im::DisabledIndent(exportFormatted != EXPORT_FORMATTED, [&]{
			ImGui::TextUnformatted("Line");
			HelpMarker("Formatting options per line");
			im::Indent([&]{
				ImGui::TextUnformatted("Prefix:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
				ImGui::InputText("##prefix", &exportPrefix);
				HelpMarker("Output a prefix at the start of each line.\n"
					"For example 'db ', note the space after 'db'.");

				ImGui::TextUnformatted("Columns:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(9.0f * s.glyphWidth + 2.0f * style.FramePadding.x);
				if (ImGui::InputInt("##columns", &exportColumns, 1, 0)) {
					exportColumns = std::clamp(exportColumns, 1, MAX_COLUMNS);
				}
				HelpMarker("Output N columns of comma-separated values.");

				ImGui::TextUnformatted("Suffix:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
				ImGui::InputText("##suffix", &exportSuffix);
				HelpMarker("Output a suffix at the end of each line.\n"
					"This might be useful if you want a trailing comma.");
			});
			ImGui::TextUnformatted("Value");
			HelpMarker("Formatting options per value (multiple per line)");
			im::Indent([&]{
				ImGui::RadioButton("Bin", &exportFormat, FORMAT_BIN);
				HelpMarker("8 digit binary with prefix '0b'\n"
					"corresponding to custom format '0b%08b'");
				ImGui::SameLine();
				ImGui::RadioButton("Dec", &exportFormat, FORMAT_DEC);
				HelpMarker("3 digit decimal right padded with spaces\n"
					"corresponding to custom format '%3d'");
				ImGui::SameLine();
				ImGui::RadioButton("Hex", &exportFormat, FORMAT_HEX);
				HelpMarker("2 digit hexadecimal\n"
					"corresponding to custom format '0x%02X'");

				try {
					if (exportFormatted == EXPORT_FORMATTED) {
						makeTclList("format", exportCustomFormat, 0).executeCommand(interp);
					}
				} catch (MSXException& e) {
					formatError = e.getMessage();
				}
				ImGui::RadioButton("Custom", &exportFormat, FORMAT_CUSTOM);
				ImGui::SameLine();
				im::Disabled(exportFormat != FORMAT_CUSTOM, [&]{
					im::StyleColor(!formatError.empty(), ImGuiCol_Text, getColor(imColor::ERROR), [&]{
						ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
						ImGui::InputText("##custom", &exportCustomFormat);
						if (!formatError.empty()) {
							simpleToolTip(formatError);
						}
					});
				});
				HelpMarker("format string passed to the Tcl 'format' command");
			});
		});
		std::string_view formatStr = (exportFormat == FORMAT_BIN) ? "0b%08b"sv
		                           : (exportFormat == FORMAT_DEC) ? "%3d"sv
		                           : (exportFormat == FORMAT_HEX) ? "0x%02X"sv
		                           : exportCustomFormat;

		ImGui::SeparatorText("Output");

		ImGui::RadioButton("Clipboard", &exportDestination, OUTPUT_CLIPBOARD);

		ImGui::RadioButton("File", &exportDestination, OUTPUT_FILE);
		ImGui::SameLine();
		im::Disabled(exportDestination != OUTPUT_FILE, [&]{
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18.0f);
			ImGui::InputText("##filename", &exportFilename);
			ImGui::SameLine();
			if (ImGui::Button(ICON_IGFD_FOLDER_OPEN)) {
				manager.openFile->selectNewFile(
					strCat("Select output file for ", title), "",
					[&](const auto& fn) { exportFilename = fn; });
			}
		});

		bool ok = begin && end &&
		          (exportFormat != FORMAT_CUSTOM || formatError.empty()) &&
			  (exportDestination != OUTPUT_FILE || !exportFilename.empty());
		im::Disabled(!ok, [&]{
			if (ImGui::Button("Export")) {
				auto fetch = [&](unsigned address) { return debuggable.read(address); };
				try {
					auto output = (exportFormatted == EXPORT_FORMATTED)
						? formatToString(fetch, *begin, *end, exportPrefix, exportColumns, exportSuffix,
								formatStr, interp)
						: rawToString(fetch, *begin, *end);
					if (exportDestination == OUTPUT_CLIPBOARD) {
						manager.getReactor().getDisplay().getVideoSystem().setClipboardText(output);
					} else {
						File file(exportFilename, File::OpenMode::TRUNCATE);
						file.write(std::span(std::bit_cast<const uint8_t*>(output.data()), output.size()));
					}
					exportStatus = "Export successful";
					exportStatusTimeout = 3.0f;
				} catch (MSXException& e) {
					exportStatus = e.getMessage();
					exportStatusTimeout = 5.0f;
					manager.printError(e.getMessage());
				}
			}
		});
		if (exportStatusTimeout > 0.0f) {
			const auto& io = ImGui::GetIO();
			exportStatusTimeout -= io.DeltaTime;

			ImGui::SameLine();
			ImGui::TextUnformatted(exportStatus);
		}

	});
}

[[nodiscard]] static unsigned DataTypeGetSize(ImGuiDataType dataType)
{
	std::array<unsigned, 8> sizes = { 1, 1, 2, 2, 4, 4, 8, 8 };
	assert(dataType >= 0 && dataType < 8);
	return sizes[dataType];
}

[[nodiscard]] static std::optional<int> parseHexDigit(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return std::nullopt;
}

[[nodiscard]] static std::optional<uint8_t> parseDataValue(std::string_view str)
{
	if (str.size() == 1) {
		return parseHexDigit(str[0]);
	} else if (str.size() == 2) {
		if (auto digit0 = parseHexDigit(str[0])) {
			if (auto digit1 = parseHexDigit(str[1])) {
				return 16 * *digit0 + *digit1;
			}
		}
	}
	return std::nullopt;
}

[[nodiscard]] static std::expected<unsigned, std::string> parseAddressExpr(
	std::string_view str, const SymbolManager& symbolManager, Interpreter& interp)
{
	if (str.empty()) return 0;

	// TODO linear search, probably OK for now, but can be improved if it turns out to be a problem
	// Note: limited to 16-bit, but larger values trigger an errors and are then handled below, so that's fine
	if (auto addr = symbolManager.parseSymbolOrValue(str)) {
		return *addr;
	}

	try {
		return TclObject(str).eval(interp).getInt(interp);
	} catch (CommandException& e) {
		return std::unexpected(e.getMessage());
	}
}

[[nodiscard]] static std::string formatData(uint8_t val)
{
	return strCat(hex_string<2, HexCase::upper>(val));
}

[[nodiscard]] static char formatAsciiData(uint8_t val)
{
	return (val < 32 || val >= 128) ? '.' : char(val);
}

[[nodiscard]] std::string DebuggableEditor::formatAddr(const Sizes& s, unsigned addr) const
{
	return strCat(hex_string<HexCase::upper>(Digits{size_t(s.addrDigitsCount)}, addr));
}
void DebuggableEditor::setStrings(const Sizes& s, Debuggable& debuggable)
{
	addrStr = strCat("0x", formatAddr(s, currentAddr));
	auto b = debuggable.read(currentAddr);
	if (dataEditingActive == HEX  ) dataInput = formatData(b);
	if (dataEditingActive == ASCII) dataInput = std::string(1, formatAsciiData(b));
}
bool DebuggableEditor::setAddr(const Sizes& s, Debuggable& debuggable, unsigned memSize, unsigned addr)
{
	addr = std::min(addr, memSize - 1);
	if (currentAddr == addr) return false;
	currentAddr = addr;
	setStrings(s, debuggable);
	return true;
}
void DebuggableEditor::scrollAddr(const Sizes& s, Debuggable& debuggable, unsigned memSize, unsigned addr, bool forceScroll)
{
	if (setAddr(s, debuggable, memSize, addr) || forceScroll) {
		im::Child("##scrolling", [&]{
			int row = narrow<int>(currentAddr) / columns;
			ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + float(row) * ImGui::GetTextLineHeight());
		});
	}
}

void DebuggableEditor::drawContents(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	const auto& style = ImGui::GetStyle();
	if (updateAddr) {
		updateAddr = false;
		scrollAddr(s, debuggable, memSize, currentAddr, true);
	} else {
		// still clip addr (for the unlikely case that 'memSize' got smaller)
		setAddr(s, debuggable, memSize, currentAddr);
	}

	float footerHeight = 0.0f;
	if (showAddress) {
		footerHeight += style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	}
	if (showSearch) {
		footerHeight += style.ItemSpacing.y + 2 * ImGui::GetFrameHeightWithSpacing();
	}
	if (showDataPreview) {
		footerHeight += style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 3 * ImGui::GetTextLineHeightWithSpacing();
	}
	if (showSymbolInfo) {
		footerHeight += style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	}
	// We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
	// This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.
	int cFlags = ImGuiWindowFlags_NoMove;
	// note: with ImGuiWindowFlags_NoNav it happens occasionally that (rapid) cursor-input is passed to the underlying MSX window
	//    without ImGuiWindowFlags_NoNav PgUp/PgDown work, but they are ALSO interpreted as openMSX hotkeys,
	//                                   though other windows have the same problem.
	//flags |= ImGuiWindowFlags_NoNav;
	cFlags |= ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginChild("##scrolling", ImVec2(0, -footerHeight), ImGuiChildFlags_None, cFlags);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	std::optional<unsigned> nextAddr;

	// Draw vertical separator
	auto* drawList = ImGui::GetWindowDrawList();
	ImVec2 windowPos = ImGui::GetWindowPos();
	if (showAscii) {
		drawList->AddLine(ImVec2(windowPos.x + s.posAsciiStart - s.glyphWidth, windowPos.y),
		                  ImVec2(windowPos.x + s.posAsciiStart - s.glyphWidth, windowPos.y + 9999),
		                  ImGui::GetColorU32(ImGuiCol_Border));
	}

	auto handleInput = [&](unsigned addr, int width, auto formatData, auto parseData, int extraFlags = 0) {
		// Display text input on current byte
		if (dataEditingTakeFocus) {
			ImGui::SetKeyboardFocusHere();
			setStrings(s, debuggable);
		}
		struct UserData {
			// TODO: We should have a way to retrieve the text edit cursor position more easily in the API, this is rather tedious. This is such a ugly mess we may be better off not using InputText() at all here.
			static int Callback(ImGuiInputTextCallbackData* data) {
				auto* userData = static_cast<UserData*>(data->UserData);
				if (userData->cursorPos != -1) { // reset cursor
					data->CursorPos = userData->cursorPos;
				}
				if (!data->HasSelection()) {
					userData->cursorPos = data->CursorPos;
				}
				if (data->SelectionStart == 0 && data->SelectionEnd == data->BufTextLen) {
					// When not editing a byte, always refresh its InputText content pulled from underlying memory data
					// (this is a bit tricky, since InputText technically "owns" the master copy of the buffer we edit it in there)
					data->DeleteChars(0, data->BufTextLen);
					userData->format(data);
					//data->InsertChars(0, ...);
					//data->SelectionEnd = width;
					data->SelectionStart = 0;
					data->CursorPos = 0;
				}
				return 0;
			}
			std::function<void(ImGuiInputTextCallbackData* data)> format;
			int cursorPos = -1; // Output
		};
		UserData userData;
		userData.format = formatData;
		if (resetCursor) {
			resetCursor = false;
			userData.cursorPos = 0;
		}
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
					  | ImGuiInputTextFlags_AutoSelectAll
					  | ImGuiInputTextFlags_NoHorizontalScroll
					  | ImGuiInputTextFlags_CallbackAlways
					  | ImGuiInputTextFlags_AlwaysOverwrite
					  | extraFlags;
		ImGui::SetNextItemWidth(s.glyphWidth * float(width));
		bool dataWrite = false;
		im::ID(int(addr), [&]{
			if (ImGui::InputText("##data", &dataInput, flags, UserData::Callback, &userData)) {
				dataWrite = true;
			} else if (!ImGui::IsItemActive()) {
				setStrings(s, debuggable);
			}
		});
		// Move cursor but only apply on next frame so scrolling will be synchronized (because currently we can't change the scrolling while the window is being rendered)
		if (addrMode == CURSOR && ImGui::IsItemActive()) {
			if (ImGui::IsKeyChordPressed(ImGuiKey_Home | ImGuiMod_Ctrl)) {
				nextAddr = 0;
				updateAddr = true; // force scroll
			} else if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
				nextAddr = currentAddr - (currentAddr % unsigned(columns));
			}
			if (ImGui::IsKeyChordPressed(ImGuiKey_End | ImGuiMod_Ctrl)) {
				nextAddr = memSize - 1;
				updateAddr = true; // force scroll
				// ImGui::InputText() already reacted to 'End' by placing the edit cursor at the end.
				// And normally that's a signal to write the value to memory. We don't want that.
				resetCursor = true;
			} else if (ImGui::IsKeyPressed(ImGuiKey_End)) {
				auto tmp = currentAddr - (currentAddr % unsigned(columns)) + columns - 1;
				nextAddr = std::min(tmp, memSize - 1);
				resetCursor = true;
			}
			if ((int(currentAddr) >= columns) &&
			    ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
				nextAddr = currentAddr - columns;
			}
			if ((int(currentAddr) < int(memSize - columns)) &&
			    ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
				nextAddr = currentAddr + columns;
			}
			if ((int(currentAddr) > 0) &&
			    ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
				nextAddr = currentAddr - 1;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
				// always set nextAddr, also when already on the very last address
				// this prevents doing a 'dataWrite'
				nextAddr = std::min(currentAddr + 1, memSize - 1);
				resetCursor = true; // see comment on 'End'
			}
			if (!switchedTab && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
				dataEditingActive = (dataEditingActive == HEX) ? ASCII : HEX;
				nextAddr = currentAddr;
				switchedTab = true; // to prevent switching back already in the next frame
			} else {
				switchedTab = false;
			}
		}
		dataEditingTakeFocus = false;
		dataWrite |= userData.cursorPos >= width;
		if (nextAddr) {
			dataWrite = false;
			dataEditingTakeFocus = true;
		}
		if (dataWrite) {
			if (auto value = parseData(dataInput)) {
				debuggable.write(addr, *value);
				assert(!nextAddr);
				nextAddr = currentAddr + 1;
			}
		}
	};

	auto& debugger = manager.debugger;
	if ((snapshot.size() != memSize) && debugger->needSnapshot()) {
		makeSnapshot(debuggable);
	}
	bool drawChanges = (snapshot.size() == memSize) && debugger->needDrawChanges();
	auto changedColor = debugger->getChangesColor();
	auto greyColor = getColor(imColor::TEXT_DISABLED);

	const auto totalLineCount = int((memSize + columns - 1) / columns);
	im::ListClipper(totalLineCount, -1, s.lineHeight, [&](int line) {
		auto addr = unsigned(line) * columns;
		ImGui::StrCat(formatAddr(s, addr), ':');

		auto previewDataTypeSize = DataTypeGetSize(previewDataType);
		auto inside = [](unsigned a, unsigned start, unsigned size) {
			return (start <= a) && (a < (start + size));
		};
		auto highLightDataPreview = [&](unsigned a) {
			return inside(a, currentAddr, previewDataTypeSize);
		};
		auto highLightSearch = [&](unsigned a) {
			if (!searchPattern) return false;
			auto len = narrow<unsigned>(searchPattern->size());
			if (searchHighlight == static_cast<int>(SearchHighlight::SINGLE)) {
				if (searchResult) {
					return inside(a, *searchResult, len);
				}
			} else if (searchHighlight == static_cast<int>(SearchHighlight::ALL)) {
				int start = std::max(0, int(a - len + 1));
				for (unsigned i = start; i <= a; ++i) {
					if (match(debuggable, memSize, i)) return true;
				}
			}
			return false;
		};
		auto highLight = [&](unsigned a) {
			return highLightDataPreview(a) || highLightSearch(a);
		};

		// Draw Hexadecimal
		for (int n = 0; n < columns && addr < memSize; ++n, ++addr) {
			int macroColumn = n / MidColsCount;
			float bytePosX = s.posHexStart + float(n) * s.hexCellWidth
					+ float(macroColumn) * s.spacingBetweenMidCols;
			ImGui::SameLine(bytePosX);

			// Draw highlight
			if (highLight(addr)) {
				ImVec2 pos = ImGui::GetCursorScreenPos();
				float highlightWidth = s.glyphWidth * 2;
				if (highLight(addr + 1)) {
					highlightWidth = s.hexCellWidth;
					if (n > 0 && (n + 1) < columns && ((n + 1) % MidColsCount) == 0) {
						highlightWidth += s.spacingBetweenMidCols;
					}
				}
				drawList->AddRectFilled(pos, ImVec2(pos.x + highlightWidth, pos.y + s.lineHeight), HighlightColor);
			}

			// Draw symbol highlight
			if (showSymbolInfo) {
				auto symbol = symbolManager.lookupValue(narrow_cast<uint16_t>(addr));
				if (!symbol.empty()) {
					float highlightWidth = s.glyphWidth * 2;
					ImVec2 pos = ImGui::GetCursorScreenPos();
					drawList->AddRectFilled(pos, ImVec2(pos.x + highlightWidth, pos.y + s.lineHeight), HighlightSymbolColor);
				}
			}

			if (currentAddr == addr && (dataEditingActive == HEX)) {
				handleInput(addr, 2,
					[&](ImGuiInputTextCallbackData* data) { // format
						auto valStr = formatData(debuggable.read(addr));
						data->InsertChars(0, valStr.data(), valStr.data() + valStr.size());
						data->SelectionEnd = 2;
					},
					[&](std::string_view data) { // parse
						return parseDataValue(data);
					},
					ImGuiInputTextFlags_CharsHexadecimal);
			} else {
				uint8_t b = debuggable.read(addr);
				bool changed = drawChanges && (b != snapshot[addr]);
				bool grey = (b == 0) && greyOutZeroes;
				im::StyleColor(changed || grey, ImGuiCol_Text, changed ? changedColor : greyColor, [&]{
					ImGui::StrCat(formatData(b), ' ');
				});
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
					dataEditingActive = HEX;
					dataEditingTakeFocus = true;
					nextAddr = addr;
				}
			}
		}

		if (showAscii) {
			// Draw ASCII values
			ImGui::SameLine(s.posAsciiStart);
			gl::vec2 pos = ImGui::GetCursorPos();
			gl::vec2 scrnPos = ImGui::GetCursorScreenPos();
			addr = unsigned(line) * columns;

			im::ID(line, [&]{
				// handle via a single full-width button, this ensures we don't miss
				// clicks because they fall in between two chars
				if (ImGui::InvisibleButton("ascii", ImVec2(s.posAsciiEnd - s.posAsciiStart, s.lineHeight))) {
					dataEditingActive = ASCII;
					dataEditingTakeFocus = true;
					nextAddr = addr + unsigned((ImGui::GetIO().MousePos.x - scrnPos.x) / s.glyphWidth);
				}
			});

			for (int n = 0; n < columns && addr < memSize; ++n, ++addr) {
				if (highLight(addr)) {
					auto start = scrnPos + gl::vec2(float(n) * s.glyphWidth, 0.0f);
					drawList->AddRectFilled(start, start + gl::vec2(s.glyphWidth, s.lineHeight), ImGui::GetColorU32(HighlightColor));
				}

				ImGui::SetCursorPos(pos);
				if (currentAddr == addr && (dataEditingActive == ASCII)) {
					handleInput(addr, 1,
						[&](ImGuiInputTextCallbackData* data) { // format
							char valChar = formatAsciiData(debuggable.read(addr));
							data->InsertChars(0, &valChar, &valChar + 1);
							data->SelectionEnd = 1;
						},
						[&](std::string_view data) -> std::optional<uint8_t> { // parse
							if (data.empty()) return {};
							uint8_t b = data[0];
							if (b < 32 || b >= 128) return {};
							return b;
						});
				} else {
					uint8_t c = debuggable.read(addr);
					bool changed = drawChanges && (c != snapshot[addr]);
					char display = formatAsciiData(c);
					bool grey = display != char(c);
					im::StyleColor(changed || grey, ImGuiCol_Text, changed ? changedColor : greyColor, [&]{
						ImGui::TextUnformatted(&display, &display + 1);
					});
				}
				pos.x += s.glyphWidth;
			}
		}
	});
	ImGui::PopStyleVar(2);
	ImGui::EndChild();

	if (nextAddr) {
		setAddr(s, debuggable, memSize, *nextAddr);
		dataEditingTakeFocus = true;
		addrMode = CURSOR;
	}

	if (showAddress) {
		bool forceScroll = ImGui::IsWindowAppearing();

		ImGui::Separator();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Address");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(2.0f * style.FramePadding.x + ImGui::CalcTextSize("Expression").x + ImGui::GetFrameHeight());
		if (ImGui::Combo("##mode", &addrMode, "Cursor\0Expression\0Link BC\0Link DE\0Link HL\0Link IX\0Link IY\0")) {
			dataEditingTakeFocus = true;
			forceScroll = true;
			if (addrMode >=2) {
				static constexpr std::array linkExpr = {
					"[reg bc]", "[reg de]", "[reg hl]", "[reg ix]", "[reg iy]"
				};
				addrExpr = linkExpr[addrMode - 2];
				addrMode = EXPRESSION;
			}
		}
		ImGui::SameLine();

		std::string* as = addrMode == CURSOR ? &addrStr : &addrExpr;
		auto addr = parseAddressExpr(*as, symbolManager, manager.getInterpreter());
		im::StyleColor(!addr, ImGuiCol_Text, getColor(imColor::ERROR), [&] {
			if (addrMode == EXPRESSION && addr) {
				scrollAddr(s, debuggable, memSize, *addr, forceScroll);
			}
			if (manager.getShortcuts().checkShortcut(Shortcuts::ID::HEX_GOTO_ADDR)) {
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(15.0f * ImGui::GetFontSize());
			if (ImGui::InputText("##addr", as, ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (auto addr2 = parseAddressExpr(addrStr, symbolManager, manager.getInterpreter())) {
					scrollAddr(s, debuggable, memSize, *addr2, forceScroll);
					dataEditingTakeFocus = true;
				}
			}
			simpleToolTip([&]{
				return addr ? strCat("0x", formatAddr(s, *addr))
				            : addr.error();
			});
		});
		im::Font(manager.fontProp, [&]{
			HelpMarker("Address-mode:\n"
				"  Cursor: view the cursor position\n"
				"  Expression: continuously re-evaluate an expression and view that address\n"
				"\n"
				"Addresses can be entered as:\n"
				"  Decimal or hexadecimal values (e.g. 0x1234)\n"
				"  A calculation like 0x1234 + 7*22\n"
				"  The name of a label (e.g. CHPUT)\n"
				"  A Tcl expression (e.g. [reg hl] to follow the content of register HL)\n"
				"\n"
				"Right-click to configure this view.");
		});
	}
	if (showSearch) {
		ImGui::Separator();
		drawSearch(s, debuggable, memSize);
	}
	if (showDataPreview) {
		ImGui::Separator();
		drawPreviewLine(s, debuggable, memSize);
	}
	if (showSymbolInfo) {
		ImGui::Separator();
		ImGui::AlignTextToFramePadding();
		auto symbol = symbolManager.lookupValue(narrow_cast<uint16_t>(currentAddr));
		if (!symbol.empty()) {
			ImGui::Text("Current symbol: %s", symbol[0]->name.c_str());
		} else {
			ImGui::TextUnformatted("No symbol for this address defined"sv);
		}
	}

	im::Popup("context", [&]{
		im::ScopedFont sf(manager.fontProp);
		ImGui::SetNextItemWidth(8.5f * s.glyphWidth + 2.0f * style.FramePadding.x);
		if (ImGui::InputInt("Columns", &columns, 1, 0)) {
			columns = std::clamp(columns, 1, MAX_COLUMNS);
		}
		ImGui::Checkbox("Show Address bar", &showAddress);
		ImGui::Checkbox("Show Search pane", &showSearch);
		ImGui::Checkbox("Show Data Preview", &showDataPreview);
		ImGui::Checkbox("Show Ascii", &showAscii);
		ImGui::Checkbox("Show Symbol info", &showSymbolInfo);
		ImGui::Checkbox("Grey out zeroes", &greyOutZeroes);
		im::Menu("Highlight changes", [&]{
			debugger->configureChangesMenu();
		});
		ImGui::Separator();
		if (ImGui::MenuItem("Export...")) {
			showExportWindow = true;
		}
	});
	im::Popup("NotFound", [&]{
		ImGui::TextUnformatted("Not found");
	});
}

[[nodiscard]] static const char* DataTypeGetDesc(ImGuiDataType dataType)
{
	std::array<const char*, 8> desc = {
		"Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64"
	};
	assert(dataType >= 0 && dataType < 8);
	return desc[dataType];
}

template<typename T>
[[nodiscard]] static T read(std::span<const uint8_t> buf)
{
	assert(buf.size() >= sizeof(T));
	T t = 0;
	memcpy(&t, buf.data(), sizeof(T));
	return t;
}

static void formatDec(std::span<const uint8_t> buf, ImGuiDataType dataType)
{
	switch (dataType) {
	case ImGuiDataType_S8:
		ImGui::StrCat(read<int8_t>(buf));
		break;
	case ImGuiDataType_U8:
		ImGui::StrCat(read<uint8_t>(buf));
		break;
	case ImGuiDataType_S16:
		ImGui::StrCat(read<int16_t>(buf));
		break;
	case ImGuiDataType_U16:
		ImGui::StrCat(read<uint16_t>(buf));
		break;
	case ImGuiDataType_S32:
		ImGui::StrCat(read<int32_t>(buf));
		break;
	case ImGuiDataType_U32:
		ImGui::StrCat(read<uint32_t>(buf));
		break;
	case ImGuiDataType_S64:
		ImGui::StrCat(read<int64_t>(buf));
		break;
	case ImGuiDataType_U64:
		ImGui::StrCat(read<uint64_t>(buf));
		break;
	default:
		UNREACHABLE;
	}
}

static void formatHex(std::span<const uint8_t> buf, ImGuiDataType data_type)
{
	switch (data_type) {
	case ImGuiDataType_S8:
	case ImGuiDataType_U8:
		ImGui::StrCat(hex_string<2>(read<uint8_t>(buf)));
		break;
	case ImGuiDataType_S16:
	case ImGuiDataType_U16:
		ImGui::StrCat(hex_string<4>(read<uint16_t>(buf)));
		break;
	case ImGuiDataType_S32:
	case ImGuiDataType_U32:
		ImGui::StrCat(hex_string<8>(read<uint32_t>(buf)));
		break;
	case ImGuiDataType_S64:
	case ImGuiDataType_U64:
		ImGui::StrCat(hex_string<16>(read<uint64_t>(buf)));
		break;
	default:
		UNREACHABLE;
	}
}

static void formatBin(std::span<const uint8_t> buf)
{
	for (int i = int(buf.size()) - 1; i >= 0; --i) {
		ImGui::StrCat(bin_string<8>(buf[i]));
		if (i != 0) ImGui::SameLine();
	}
}

void DebuggableEditor::parseSearchString(std::string_view str)
{
	searchPattern.reset();
	searchResult.reset();
	std::vector<uint8_t> result;

	if (searchType == static_cast<int>(SearchType::ASCII)) {
		const auto* begin = std::bit_cast<const uint8_t*>(str.data());
		const auto* end = begin + str.size();
		result.assign(begin, end);
	} else {
		assert(searchType == static_cast<int>(SearchType::HEX));
		std::optional<int> partial;
		for (char c : str) {
			if (c == ' ') continue; // ignore space characters
			auto digit = parseHexDigit(c);
			if (!digit) return; // error: invalid hex digit
			if (partial) {
				result.push_back(narrow<uint8_t>(16 * *partial + *digit));
				partial.reset();
			} else {
				partial = *digit;
			}
		}
		if (partial) return; // error: odd number of hex digits
	}

	searchPattern = std::move(result);
}

void DebuggableEditor::drawSearch(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	const auto& style = ImGui::GetStyle();

	bool doSearch = false;
	auto buttonSize = ImGui::CalcTextSize("Search").x + 2.0f * style.FramePadding.x;
	ImGui::SetNextItemWidth(-(buttonSize + style.WindowPadding.x));
	im::StyleColor(!searchPattern, ImGuiCol_Text, getColor(imColor::ERROR), [&] {
		auto callback = [](ImGuiInputTextCallbackData* data) {
			if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
				auto& self = *static_cast<DebuggableEditor*>(data->UserData);
				self.parseSearchString(std::string_view(data->Buf, data->BufTextLen));
			}
			return 0;
		};
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
		                          | ImGuiInputTextFlags_CallbackEdit;
		if (ImGui::InputText("##search_string", &searchString, flags, callback, this)) {
			doSearch = true; // pressed enter
		}
	});
	ImGui::SameLine();
	im::Disabled(!searchPattern, [&]{
		doSearch |= ImGui::Button("Search");
	});
	if (!searchPattern) {
		simpleToolTip("Must be an even number of hex digits, optionally separated by spaces");
	}
	if (searchPattern && doSearch) {
		search(s, debuggable, memSize);
	}

	auto arrowSize = ImGui::GetFrameHeight();
	auto extra = arrowSize + 2.0f * style.FramePadding.x;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Type");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::CalcTextSize("Ascii").x + extra);
	if (ImGui::Combo("##search_type", &searchType, "Hex\0Ascii\0\0")) {
		parseSearchString(searchString);
	}

	ImGui::SameLine(0.0f, 2 * ImGui::GetFontSize());
	ImGui::TextUnformatted("Direction");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::CalcTextSize("Backwards").x + extra);
	ImGui::Combo("##search_direction", &searchDirection, "Forwards\0Backwards\0\0");

	ImGui::SameLine(0.0f, 2 * ImGui::GetFontSize());
	ImGui::TextUnformatted("Highlight");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::CalcTextSize("Single").x + extra);
	ImGui::Combo("##search_highlight", &searchHighlight, "None\0Single\0All\0\0");
}

bool DebuggableEditor::match(Debuggable& debuggable, unsigned memSize, unsigned addr)
{
	assert(searchPattern);
	if ((addr + searchPattern->size()) > memSize) return false;
	for (auto [i, c] : enumerate(*searchPattern)) {
		if (debuggable.read(narrow<unsigned>(addr + i)) != c) return false;
	}
	return true;
}

void DebuggableEditor::search(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	std::optional<unsigned> found;
	auto test = [&](unsigned addr) {
		if (match(debuggable, memSize, addr)) {
			found = addr;
			return true;
		}
		return false;
	};
	if (searchDirection == static_cast<int>(SearchDirection::FWD)) {
		for (unsigned addr = currentAddr + 1; addr < memSize; ++addr) {
			if (test(addr)) break;
		}
		if (!found) {
			for (unsigned addr = 0; addr <= currentAddr; ++addr) {
				if (test(addr)) break;
			}
		}
	} else {
		for (int addr = currentAddr - 1; addr > 0; --addr) {
			if (test(unsigned(addr))) break;
		}
		if (!found) {
			for (int addr = memSize - 1; addr >= int(currentAddr); --addr) {
				if (test(unsigned(addr))) break;
			}
		}
	}
	if (found) {
		searchResult = *found;
		scrollAddr(s, debuggable, memSize, *found, false);
		dataEditingTakeFocus = true;
		addrMode = CURSOR;
	} else {
		searchResult.reset();
		ImGui::OpenPopup("NotFound");
	}
}

void DebuggableEditor::drawPreviewLine(const Sizes& s, Debuggable& debuggable, unsigned memSize)
{
	const auto& style = ImGui::GetStyle();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Preview as:"sv);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((s.glyphWidth * 10.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
	if (ImGui::BeginCombo("##combo_type", DataTypeGetDesc(previewDataType), ImGuiComboFlags_HeightLargest)) {
		for (ImGuiDataType n = 0; n < 8; ++n) {
			if (ImGui::Selectable(DataTypeGetDesc(n), previewDataType == n)) {
				previewDataType = n;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth((s.glyphWidth * 6.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
	ImGui::Combo("##combo_endianess", &previewEndianess, "LE\0BE\0\0");

	std::array<uint8_t, 8> dataBuf = {};
	auto elemSize = DataTypeGetSize(previewDataType);
	for (auto i : xrange(elemSize)) {
		auto addr = currentAddr + i;
		dataBuf[i] = (addr < memSize) ? debuggable.read(addr) : 0;
	}

	static constexpr bool nativeIsLittle = std::endian::native == std::endian::little;
	if (bool previewIsLittle = previewEndianess == LE;
	    nativeIsLittle != previewIsLittle) {
		std::reverse(dataBuf.begin(), dataBuf.begin() + elemSize);
	}

	ImGui::TextUnformatted("Dec "sv);
	ImGui::SameLine();
	formatDec(dataBuf, previewDataType);

	ImGui::TextUnformatted("Hex "sv);
	ImGui::SameLine();
	formatHex(dataBuf, previewDataType);

	ImGui::TextUnformatted("Bin "sv);
	ImGui::SameLine();
	formatBin(subspan(dataBuf, 0, elemSize));
}

} // namespace openmsx
