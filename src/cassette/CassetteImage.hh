#ifndef CASSETTEIMAGE_HH
#define CASSETTEIMAGE_HH

#include "EmuTime.hh"
#include "sha1.hh"

#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

class Filename;

class CassetteImage
{
public:
	enum FileType : uint8_t { ASCII, BINARY, BASIC, UNKNOWN };

	virtual ~CassetteImage() = default;
	[[nodiscard]] virtual int16_t getSampleAt(EmuTime time) const = 0;
	[[nodiscard]] virtual EmuTime getEndTime() const = 0;
	[[nodiscard]] virtual unsigned getFrequency() const = 0;
	virtual void fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const = 0;
	[[nodiscard]] virtual float getAmplificationFactorImpl() const = 0;

	[[nodiscard]] FileType getFirstFileType() const { return firstFileType; }
	[[nodiscard]] std::string getFirstFileTypeAsString() const;

	/** Get sha1sum for this image.
	 * This is based on the content of the file, not the logical meaning of
	 * the file. IOW: it's possible for different files (with different
	 * sha1sum) to represent the same logical cassette data (e.g. wav with
	 * different bits per sample). This method will give a different
	 * sha1sum to such files.
	 */
	[[nodiscard]] const Sha1Sum& getSha1Sum() const;

protected:
	CassetteImage() = default;
	// Please make sure this method is called from the constructor of each
	// subclass! (And only from there.)
	void setFirstFileType(FileType type, const Filename& fileName);
	void setSha1Sum(const Sha1Sum& sha1sum);

private:
	FileType firstFileType = UNKNOWN;
	Sha1Sum sha1sum;
};

} // namespace openmsx

#endif
