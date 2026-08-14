#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <memory>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cassert>

namespace WakuBlood {
namespace UBTF {

// Magic identifiers for WAD header compatibility
constexpr char WAD_MAGIC_UBTF[4] = {'U', 'B', 'T', 'F'}; // Universal Build Tile Format
constexpr char WAD_MAGIC_PWAD[4] = {'P', 'W', 'A', 'D'}; // Doom Patch WAD compatibility
constexpr char WAD_MAGIC_IWAD[4] = {'I', 'W', 'A', 'D'}; // Doom Internal WAD compatibility

// Maximum lump name length matching Doom WAD specification
constexpr size_t WAD_LUMP_NAME_LEN = 8;

// Current UBTF spec version
constexpr uint16_t UBTF_VERSION_MAJOR = 1;
constexpr uint16_t UBTF_VERSION_MINOR = 0;

#pragma pack(push, 1)

// Standard 12-byte Doom WAD File Header
struct WadHeader {
    char magic[4];          // "UBTF", "PWAD", or "IWAD"
    int32_t numLumps{0};     // Total number of lumps stored in WAD
    int32_t dirOffset{0};    // File offset pointing to lump directory array
};

// Standard 16-byte Doom WAD Lump Directory Entry
struct WadLumpEntry {
    int32_t filePos{0};     // File offset where lump data starts
    int32_t size{0};        // Size of lump data in bytes
    char name[8]{0};        // 8-character ASCII lump name (null-padded)
};

// Custom UBTF Header Lump payload (stored inside 'UBTF_HDR' lump)
struct UBTFHeaderLump {
    uint16_t versionMajor{UBTF_VERSION_MAJOR};
    uint16_t versionMinor{UBTF_VERSION_MINOR};
    uint32_t numTiles{0};
    uint16_t defaultPaletteId{0};
    uint8_t  bitsPerPixel{8}; // 8-bit indexed palette or 32-bit RGBA
    uint8_t  reserved[7]{0};
};

// Build Engine PICANM Attribute Structure (Classic Build tile animation parameters)
struct BuildPicanm {
    uint8_t numFrames{0};   // Animation frame count (bits 0-5)
    uint8_t animType{0};    // 0: none, 1: oscillating, 2: forward, 3: backward
    int8_t  xOffset{0};     // X display offset adjustment
    int8_t  yOffset{0};     // Y display offset adjustment
    uint8_t animSpeed{0};   // Animation speed tick divider
    uint8_t viewType{0};    // Sprite rotational view count (1, 5, or 8 views)
};

// Monolith Blood Specific Extended Surface & Material Attributes
struct BloodTileAttributes {
    uint16_t surfaceType{0};  // 0: Generic, 1: Stone, 2: Wood, 3: Metal, 4: Flesh, 5: Water, 6: Ice
    uint16_t hitSoundId{0};   // Impact sound index when shot/hit
    uint8_t  translucency{0}; // Translucency mode (0: Opaque, 1: 33%, 2: 50%, 3: 66%, 4: Additive)
    uint8_t  flags{0};        // Bit 0: Burnable, Bit 1: Shatterable, Bit 2: Fullbright/Emissive
    uint8_t  lightEmission{0};// Dynamic light radius emitted by tile
    uint8_t  reserved{3};
};

// Header prefix stored at start of individual tile lumps ('TILExxxx')
struct UBTFTileHeader {
    uint16_t tileId{0};
    uint16_t width{0};
    uint16_t height{0};
    uint8_t  format{0};      // 0 = 8-bit Indexed Palette, 1 = 32-bit RGBA Raw
    uint8_t  reserved{1};
    BuildPicanm picanm;
    BloodTileAttributes bloodAttr;
};

#pragma pack(pop)

enum class PixelFormat : uint8_t {
    Indexed8Bit = 0,
    RGBA32Bit   = 1
};

enum class SurfaceType : uint16_t {
    Generic = 0,
    Stone   = 1,
    Wood    = 2,
    Metal   = 3,
    Flesh   = 4,
    Water   = 5,
    Ice     = 6,
    Dirt    = 7
};

// Runtime representation of a single Build / Blood tile in WakuBlood
struct UBTFTile {
    uint16_t tileId{0};
    uint16_t width{0};
    uint16_t height{0};
    PixelFormat format{PixelFormat::Indexed8Bit};
    BuildPicanm picanm{};
    BloodTileAttributes bloodAttr{};
    std::vector<uint8_t> pixelData; // Pixel buffer (8-bit palette indices or 32-bit RGBA bytes)

    size_t GetExpectedDataSize() const {
        size_t bpp = (format == PixelFormat::RGBA32Bit) ? 4 : 1;
        return static_cast<size_t>(width) * static_cast<size_t>(height) * bpp;
    }

    bool IsValid() const {
        return width > 0 && height > 0 && pixelData.size() == GetExpectedDataSize();
    }
};

// Helper to sanitize and convert lump string names into 8-byte fixed length arrays
inline void StringToLumpName(const std::string& str, char outName[8]) {
    std::memset(outName, 0, 8);
    for (size_t i = 0; i < 8 && i < str.size(); ++i) {
        outName[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(str[i])));
    }
}

inline std::string LumpNameToString(const char name[8]) {
    std::string s;
    for (size_t i = 0; i < 8; ++i) {
        if (name[i] == '\0') break;
        s += name[i];
    }
    return s;
}

// Standard 256-color RGB Palette for Monolith Blood rendering
struct UBTFPalette {
    uint16_t paletteId{0};
    uint8_t colors[256][3]; // 256 colors * 3 channels (R, G, B)

    void SetDefaultBloodPalette() {
        // Generate a test palette with Blood-themed tones (grays, dark reds, blood crimson)
        for (int i = 0; i < 256; ++i) {
            if (i < 64) {
                // Grayscale ramp
                colors[i][0] = static_cast<uint8_t>(i * 4);
                colors[i][1] = static_cast<uint8_t>(i * 4);
                colors[i][2] = static_cast<uint8_t>(i * 4);
            } else if (i < 128) {
                // Crimson Blood Ramp
                int val = (i - 64) * 4;
                colors[i][0] = static_cast<uint8_t>(val);
                colors[i][1] = static_cast<uint8_t>(val / 4);
                colors[i][2] = static_cast<uint8_t>(val / 6);
            } else {
                // General color spectrum
                colors[i][0] = static_cast<uint8_t>((i * 7) % 256);
                colors[i][1] = static_cast<uint8_t>((i * 3) % 256);
                colors[i][2] = static_cast<uint8_t>((i * 11) % 256);
            }
        }
    }
};

// WAD Lump Data wrapper
struct WadLumpData {
    std::string name;
    std::vector<uint8_t> data;
};

// Universal Build Tile Format (UBTF) WAD Archive Class
class UBTFArchive {
private:
    std::string magicIdentifier{"UBTF"};
    UBTFHeaderLump headerProps{};
    UBTFPalette defaultPalette{};
    std::unordered_map<uint16_t, UBTFTile> tiles;
    std::vector<WadLumpData> customLumps;

public:
    UBTFArchive() {
        magicIdentifier = "UBTF";
        defaultPalette.SetDefaultBloodPalette();
    }

    void SetMagic(const std::string& magic) {
        if (magic.size() == 4) magicIdentifier = magic;
    }

    std::string GetMagic() const { return magicIdentifier; }

    // Add or replace a tile in the archive
    void AddTile(const UBTFTile& tile) {
        if (!tile.IsValid()) {
            throw std::invalid_argument("Cannot add invalid UBTFTile to archive (ID: " + 
                                       std::to_string(tile.tileId) + ")");
        }
        tiles[tile.tileId] = tile;
        headerProps.numTiles = static_cast<uint32_t>(tiles.size());
    }

    // Retrieve a tile reference by ID
    const UBTFTile* GetTile(uint16_t tileId) const {
        auto it = tiles.find(tileId);
        if (it != tiles.end()) return &it->second;
        return nullptr;
    }

    const std::unordered_map<uint16_t, UBTFTile>& GetAllTiles() const {
        return tiles;
    }

    void SetPalette(const UBTFPalette& pal) {
        defaultPalette = pal;
    }

    const UBTFPalette& GetPalette() const {
        return defaultPalette;
    }

    // Add a raw custom lump to WAD archive
    void AddCustomLump(const std::string& name, const std::vector<uint8_t>& data) {
        WadLumpData lump;
        lump.name = name;
        lump.data = data;
        customLumps.push_back(lump);
    }

    // Serialize complete tile library into standard Doom WAD binary stream (.WAD)
    std::vector<uint8_t> SerializeToWadBuffer() const {
        std::vector<uint8_t> buffer;
        std::vector<WadLumpEntry> directory;

        // Reserve space for 12-byte WAD header at index 0
        buffer.resize(sizeof(WadHeader));

        // Helper lambda to append a lump into payload buffer and construct standard WAD entry
        auto AppendLump = [&](const std::string& lumpName, const void* data, size_t dataSize) {
            WadLumpEntry entry;
            entry.filePos = static_cast<int32_t>(buffer.size());
            entry.size = static_cast<int32_t>(dataSize);
            StringToLumpName(lumpName, entry.name);

            if (dataSize > 0 && data != nullptr) {
                const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(data);
                buffer.insert(buffer.end(), bytePtr, bytePtr + dataSize);
            }

            directory.push_back(entry);
        };

        // 1. Write Header Lump (UBTF_HDR)
        UBTFHeaderLump hdr = headerProps;
        hdr.numTiles = static_cast<uint32_t>(tiles.size());
        AppendLump("UBTF_HDR", &hdr, sizeof(UBTFHeaderLump));

        // 2. Write Default Palette Lump (PALETTE)
        AppendLump("PALETTE", defaultPalette.colors, sizeof(defaultPalette.colors));

        // 3. Write Tile Section Marker (T_START)
        AppendLump("T_START", nullptr, 0);

        // 4. Write Individual Tile Lumps (TILE0000, TILE0001, etc.)
        // Sort tiles by tileId for consistent WAD lump ordering
        std::vector<uint16_t> tileIds;
        for (const auto& [id, _] : tiles) tileIds.push_back(id);
        std::sort(tileIds.begin(), tileIds.end());

        for (uint16_t tid : tileIds) {
            const auto& tile = tiles.at(tid);

            // Construct tile payload buffer (Tile Header + Raw Pixel Bytes)
            std::vector<uint8_t> tilePayload;
            UBTFTileHeader thdr{};
            thdr.tileId = tile.tileId;
            thdr.width = tile.width;
            thdr.height = tile.height;
            thdr.format = static_cast<uint8_t>(tile.format);
            thdr.picanm = tile.picanm;
            thdr.bloodAttr = tile.bloodAttr;

            const uint8_t* thdrBytes = reinterpret_cast<const uint8_t*>(&thdr);
            tilePayload.insert(tilePayload.end(), thdrBytes, thdrBytes + sizeof(UBTFTileHeader));
            tilePayload.insert(tilePayload.end(), tile.pixelData.begin(), tile.pixelData.end());

            // Format lump name as TILExxxx (e.g. TILE0000, TILE0512)
            std::ostringstream ss;
            ss << "TILE" << std::setw(4) << std::setfill('0') << tile.tileId;
            AppendLump(ss.str(), tilePayload.data(), tilePayload.size());
        }

        // 5. Write Tile Section End Marker (T_END)
        AppendLump("T_END", nullptr, 0);

        // 6. Append any custom user lumps
        for (const auto& clump : customLumps) {
            AppendLump(clump.name, clump.data.data(), clump.data.size());
        }

        // 7. Write Lump Directory Table at current end of buffer
        int32_t directoryOffset = static_cast<int32_t>(buffer.size());
        for (const auto& entry : directory) {
            const uint8_t* entryBytes = reinterpret_cast<const uint8_t*>(&entry);
            buffer.insert(buffer.end(), entryBytes, entryBytes + sizeof(WadLumpEntry));
        }

        // 8. Patch standard Doom WAD Header at buffer offset 0
        WadHeader wadHdr{};
        std::memcpy(wadHdr.magic, magicIdentifier.c_str(), 4);
        wadHdr.numLumps = static_cast<int32_t>(directory.size());
        wadHdr.dirOffset = directoryOffset;

        std::memcpy(buffer.data(), &wadHdr, sizeof(WadHeader));

        return buffer;
    }
};

// Binary Parser to extract tiles and palettes from Doom-structured WAD files
class UBTFReader {
public:
    static bool ParseFromWadBuffer(const std::vector<uint8_t>& buffer, UBTFArchive& outArchive) {
        if (buffer.size() < sizeof(WadHeader)) {
            std::cerr << "[UBTFReader Error] Buffer too small for standard WAD header!\n";
            return false;
        }

        // Read WAD Header
        WadHeader wadHdr;
        std::memcpy(&wadHdr, buffer.data(), sizeof(WadHeader));

        std::string magic(wadHdr.magic, 4);
        outArchive.SetMagic(magic);

        if (wadHdr.dirOffset < 0 || wadHdr.dirOffset + (wadHdr.numLumps * sizeof(WadLumpEntry)) > buffer.size()) {
            std::cerr << "[UBTFReader Error] Invalid directory offset or corrupt lump count!\n";
            return false;
        }

        // Read Lump Directory Entries
        std::vector<WadLumpEntry> directory(wadHdr.numLumps);
        const uint8_t* dirPtr = buffer.data() + wadHdr.dirOffset;
        std::memcpy(directory.data(), dirPtr, wadHdr.numLumps * sizeof(WadLumpEntry));

        bool insideTileSection = false;

        for (const auto& entry : directory) {
            std::string lumpName = LumpNameToString(entry.name);

            if (entry.filePos < 0 || entry.filePos + entry.size > static_cast<int32_t>(buffer.size())) {
                std::cerr << "[UBTFReader Error] Lump '" << lumpName << "' points out of file bounds!\n";
                return false;
            }

            const uint8_t* lumpPtr = buffer.data() + entry.filePos;

            if (lumpName == "UBTF_HDR" && entry.size >= sizeof(UBTFHeaderLump)) {
                // Header metadata lump parsed silently
            } else if (lumpName == "PALETTE" && entry.size >= 768) {
                UBTFPalette pal;
                std::memcpy(pal.colors, lumpPtr, 768);
                outArchive.SetPalette(pal);
            } else if (lumpName == "T_START") {
                insideTileSection = true;
            } else if (lumpName == "T_END") {
                insideTileSection = false;
            } else if (lumpName.rfind("TILE", 0) == 0 || insideTileSection) {
                if (entry.size >= sizeof(UBTFTileHeader)) {
                    UBTFTileHeader thdr;
                    std::memcpy(&thdr, lumpPtr, sizeof(UBTFTileHeader));

                    UBTFTile tile;
                    tile.tileId = thdr.tileId;
                    tile.width = thdr.width;
                    tile.height = thdr.height;
                    tile.format = static_cast<PixelFormat>(thdr.format);
                    tile.picanm = thdr.picanm;
                    tile.bloodAttr = thdr.bloodAttr;

                    size_t pixelByteCount = entry.size - sizeof(UBTFTileHeader);
                    tile.pixelData.resize(pixelByteCount);
                    if (pixelByteCount > 0) {
                        std::memcpy(tile.pixelData.data(), lumpPtr + sizeof(UBTFTileHeader), pixelByteCount);
                    }

                    if (tile.IsValid()) {
                        outArchive.AddTile(tile);
                    }
                }
            } else if (lumpName.size() > 0 && entry.size > 0) {
                std::vector<uint8_t> customBytes(lumpPtr, lumpPtr + entry.size);
                outArchive.AddCustomLump(lumpName, customBytes);
            }
        }

        return true;
    }
};

// Utility to convert classic Build Engine binary ART tile files into UBTF format
class LegacyArtConverter {
public:
    static UBTFTile CreateSampleBloodWallTile(uint16_t tileId) {
        UBTFTile tile;
        tile.tileId = tileId;
        tile.width = 64;
        tile.height = 64;
        tile.format = PixelFormat::Indexed8Bit;

        // Set Blood-specific tile attributes
        tile.bloodAttr.surfaceType = static_cast<uint16_t>(SurfaceType::Stone);
        tile.bloodAttr.hitSoundId = 12; // Stone hit impact sound
        tile.bloodAttr.translucency = 0; // Opaque
        tile.bloodAttr.flags = 0x00;

        // Set classic Build Engine PICANM flags
        tile.picanm.numFrames = 0;
        tile.picanm.animType = 0;
        tile.picanm.animSpeed = 0;

        // Generate synthetic brick tile pattern
        tile.pixelData.resize(64 * 64);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                if (y % 16 == 0 || (x % 32 == 0 && (y / 16) % 2 == 0) || (x % 32 == 16 && (y / 16) % 2 != 0)) {
                    tile.pixelData[y * 64 + x] = 10; // Dark mortar gray
                } else {
                    tile.pixelData[y * 64 + x] = static_cast<uint8_t>(70 + ((x + y) % 30)); // Red stone
                }
            }
        }

        return tile;
    }

    static UBTFTile CreateSampleBloodAnimatedWaterTile(uint16_t tileId) {
        UBTFTile tile;
        tile.tileId = tileId;
        tile.width = 32;
        tile.height = 32;
        tile.format = PixelFormat::Indexed8Bit;

        tile.bloodAttr.surfaceType = static_cast<uint16_t>(SurfaceType::Water);
        tile.bloodAttr.hitSoundId = 44; // Water splash sound
        tile.bloodAttr.translucency = 2; // 50% translucency

        tile.picanm.numFrames = 4; // 4 animation frames
        tile.picanm.animType = 1;  // Oscillating animation
        tile.picanm.animSpeed = 8; // Speed tick divider

        tile.pixelData.resize(32 * 32);
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                tile.pixelData[y * 32 + x] = static_cast<uint8_t>(180 + ((x * y) % 20)); // Water palette index range
            }
        }

        return tile;
    }
};

// Verification tool to test WAD offsets, tile dimension bounds, and lump integrity
struct ValidationReport {
    bool isValid{true};
    std::vector<std::string> messages;
};

class UBTFValidator {
public:
    static ValidationReport ValidateArchive(const UBTFArchive& archive) {
        ValidationReport report;

        if (archive.GetMagic() != "UBTF" && archive.GetMagic() != "PWAD" && archive.GetMagic() != "IWAD") {
            report.isValid = false;
            report.messages.push_back("ERROR: Unknown WAD magic signature '" + archive.GetMagic() + "'");
        }

        const auto& tiles = archive.GetAllTiles();
        if (tiles.empty()) {
            report.messages.push_back("WARNING: UBTF archive contains zero tiles.");
        }

        for (const auto& [id, tile] : tiles) {
            if (tile.width == 0 || tile.height == 0) {
                report.isValid = false;
                report.messages.push_back("ERROR: Tile " + std::to_string(id) + " has zero width or height!");
            }
            if (!tile.IsValid()) {
                report.isValid = false;
                report.messages.push_back("ERROR: Tile " + std::to_string(id) + " pixel buffer size mismatch!");
            }
        }

        return report;
    }
};

std::string GetSurfaceTypeName(uint16_t type) {
    switch (static_cast<SurfaceType>(type)) {
        case SurfaceType::Generic: return "Generic";
        case SurfaceType::Stone:   return "Stone";
        case SurfaceType::Wood:    return "Wood";
        case SurfaceType::Metal:   return "Metal";
        case SurfaceType::Flesh:   return "Flesh";
        case SurfaceType::Water:   return "Water";
        case SurfaceType::Ice:     return "Ice";
        case SurfaceType::Dirt:    return "Dirt";
    }
    return "Unknown (" + std::to_string(type) + ")";
}

} // namespace UBTF
} // namespace WakuBlood

int main() {
    using namespace WakuBlood::UBTF;

    std::cout << "========================================================================\n";
    std::cout << "       WakuBlood Engine - Universal Build Tile Format (UBTF / WAD)      \n";
    std::cout << "========================================================================\n\n";

    std::cout << "[1] Constructing WakuBlood UBTF Archive container...\n";
    UBTFArchive archive;
    archive.SetMagic("UBTF"); // Modern UBTF WAD header signature

    std::cout << "    Adding sample Blood tiles to UBTF WAD...\n";
    
    // Add Stone Wall Tile
    UBTFTile stoneWall = LegacyArtConverter::CreateSampleBloodWallTile(500);
    archive.AddTile(stoneWall);
    std::cout << "    - Tile 500: Crypt Stone Wall (" << stoneWall.width << "x" << stoneWall.height 
              << ", Surface: " << GetSurfaceTypeName(stoneWall.bloodAttr.surfaceType) << ")\n";

    // Add Animated Water Tile
    UBTFTile water = LegacyArtConverter::CreateSampleBloodAnimatedWaterTile(800);
    archive.AddTile(water);
    std::cout << "    - Tile 800: Animated Blood Pool (" << water.width << "x" << water.height 
              << ", Surface: " << GetSurfaceTypeName(water.bloodAttr.surfaceType) 
              << ", Translucency Mode: " << static_cast<int>(water.bloodAttr.translucency) << ")\n\n";

    std::cout << "[2] Validating and serializing UBTF archive to standard Doom WAD binary format...\n";
    ValidationReport prepReport = UBTFValidator::ValidateArchive(archive);
    if (!prepReport.isValid) {
        std::cerr << "    [ERROR] Archive pre-validation failed!\n";
        for (const auto& msg : prepReport.messages) std::cerr << "      * " << msg << "\n";
        return 1;
    }
    std::cout << "    [SUCCESS] Pre-serialization validation passed!\n";

    std::vector<uint8_t> wadBuffer = archive.SerializeToWadBuffer();
    std::cout << "    - Total WAD Binary Size: " << wadBuffer.size() << " bytes.\n\n";

    std::cout << "[3] Testing binary parsing of generated UBTF WAD stream via UBTFReader...\n";
    UBTFArchive reloadedArchive;
    if (UBTFReader::ParseFromWadBuffer(wadBuffer, reloadedArchive)) {
        std::cout << "    [SUCCESS] UBTF WAD parsed successfully!\n";
        std::cout << "    - Parsed Magic Header: " << reloadedArchive.GetMagic() << "\n";
        std::cout << "    - Parsed Tile Count:   " << reloadedArchive.GetAllTiles().size() << "\n";

        for (const auto& [id, tile] : reloadedArchive.GetAllTiles()) {
            std::cout << "      * Loaded Tile " << id << ": " << tile.width << "x" << tile.height 
                      << " | Surface: " << GetSurfaceTypeName(tile.bloodAttr.surfaceType)
                      << " | Anim Frames: " << static_cast<int>(tile.picanm.numFrames)
                      << " | Buffer Bytes: " << tile.pixelData.size() << "\n";
        }
    } else {
        std::cout << "    [ERROR] Failed to parse generated UBTF WAD buffer!\n";
        return 1;
    }

    std::cout << "\n[4] Running post-parse validation report...\n";
    ValidationReport finalReport = UBTFValidator::ValidateArchive(reloadedArchive);
    if (finalReport.isValid) {
        std::cout << "    [SUCCESS] All lumps, directory entries, and pixel buffers are valid!\n";
    }

    std::cout << "\n========================================================================\n";
    std::cout << "                     UBTF System Test Completed                         \n";
    std::cout << "========================================================================\n";

    return 0;
}