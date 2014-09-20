#ifndef HD_HH
#define HD_HH

#include "Filename.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "TigerTree.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class HDCommand;
class File;
class DeviceConfig;

class HD : public SectorAccessibleDisk, public DiskContainer
         , public TTData
{
public:
	explicit HD(const DeviceConfig& config);
	virtual ~HD();

	const std::string& getName() const { return name; }
	const Filename& getImageName() const { return filename; }
	void switchImage(const Filename& filename);

	std::string getTigerTreeHash();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

private:
	// SectorAccessibleDisk:
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	size_t getNbSectorsImpl() const override;
	bool isWriteProtectedImpl() const override;
	Sha1Sum getSha1Sum() override;

	// Diskcontainer:
	SectorAccessibleDisk* getSectorAccessibleDisk() override;
	const std::string& getContainerName() const override;
	bool diskChanged() override;
	int insertDisk(string_ref filename) override;

	// TTData
	uint8_t* getData(size_t offset, size_t size) override;
	bool isCacheStillValid(time_t& time) override;

	void openImage();

	MSXMotherBoard& motherBoard;
	std::string name;
	std::unique_ptr<HDCommand> hdCommand;
	std::unique_ptr<TigerTree> tigerTree;

	std::unique_ptr<File> file;
	Filename filename;
	size_t filesize;
	bool alreadyTried;
};

REGISTER_BASE_CLASS(HD, "HD");
SERIALIZE_CLASS_VERSION(HD, 2);

} // namespace openmsx

#endif
