#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <memory>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cctype>

namespace WakuBlood {
namespace UBMF {

// Generic property value store for flexible key-value metadata (UDMF-style for Build/Blood)
enum class ValueType { String, Integer, Float, Boolean };

struct AttributeValue {
    ValueType type{ValueType::String};
    std::string strVal;
    int64_t intVal{0};
    double floatVal{0.0};
    bool boolVal{false};

    static AttributeValue MakeString(const std::string& v) {
        AttributeValue val; val.type = ValueType::String; val.strVal = v; return val;
    }
    static AttributeValue MakeInt(int64_t v) {
        AttributeValue val; val.type = ValueType::Integer; val.intVal = v; return val;
    }
    static AttributeValue MakeFloat(double v) {
        AttributeValue val; val.type = ValueType::Float; val.floatVal = v; return val;
    }
    static AttributeValue MakeBool(bool v) {
        AttributeValue val; val.type = ValueType::Boolean; val.boolVal = v; return val;
    }

    std::string ToString() const {
        switch (type) {
            case ValueType::String:  return "\"" + strVal + "\"";
            case ValueType::Integer: return std::to_string(intVal);
            case ValueType::Float:   return std::to_string(floatVal);
            case ValueType::Boolean: return boolVal ? "true" : "false";
        }
        return "nil";
    }
};

using ExtraDataMap = std::unordered_map<std::string, AttributeValue>;

struct Vector2D {
    double x{0.0};
    double y{0.0};

    Vector2D() = default;
    Vector2D(double _x, double _y) : x(_x), y(_y) {}

    bool operator==(const Vector2D& o) const { return x == o.x && y == o.y; }
    double Distance(const Vector2D& o) const {
        double dx = x - o.x;
        double dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

struct Vector3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vector3D() = default;
    Vector3D(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
};

struct UBMFVertex {
    int id{-1};
    Vector2D position{0.0, 0.0};
    ExtraDataMap userProps;

    UBMFVertex() = default;
    UBMFVertex(int _id, double x, double y) : id(_id), position(x, y) {}
};

struct UBMFWall {
    int id{-1};
    int point2{-1};         // Index of destination vertex forming wall segment
    int wall2{-1};          // Index of matching portal wall (-1 if solid wall)
    int nextsector{-1};     // Index of adjacent sector (-1 if solid wall)
    int nextwall{-1};       // Legacy Build nextwall index

    int16_t picnum{0};      // Texture ID
    int16_t overpicnum{0};  // Masked wall texture ID
    int16_t cstat{0};       // Wall flags (blocking, hitscan, x-flip, y-flip, masked, etc.)
    int8_t shade{0};        // Wall lighting shade offset
    uint8_t pal{0};         // Palette index

    int8_t xpanning{0};     // Texture X offset
    int8_t ypanning{0};     // Texture Y offset
    int16_t lotag{0};       // Build Engine low tag
    int16_t hitag{0};       // Build Engine high tag
    int16_t extra{-1};      // Blood XWALL reference index

    // WakuBlood / Monolith Blood Specific XWALL fields
    struct BloodXWall {
        uint16_t rxId{0};
        uint16_t txId{0};
        uint16_t state{0};
        uint16_t busy{0};
        uint16_t triggerType{0};
        bool isKeyLocked{false};
        int keyType{0};
    } xwallData;

    ExtraDataMap userProps;
};

struct UBMFSector {
    int id{-1};
    int wallptr{-1};        // Starting wall index
    int wallnum{0};         // Total number of walls in sector loop

    int32_t floorz{0};      // Ceiling/Floor Z positions (Build Z coordinates)
    int32_t ceilingz{0};

    int16_t floorstat{0};   // Sector floor status flags (sloped, relative alignment, etc.)
    int16_t ceilingstat{0}; // Sector ceiling status flags

    int16_t floorpicnum{0};   // Floor texture ID
    int16_t ceilingpicnum{0}; // Ceiling texture ID

    int8_t floorshade{0};
    int8_t ceilingshade{0};

    uint8_t floorpal{0};
    uint8_t ceilingpal{0};

    uint8_t floorxpanning{0};
    uint8_t floorypanning{0};
    uint8_t ceilingxpanning{0};
    uint8_t ceilingypanning{0};

    int16_t visibility{0};
    int16_t lotag{0};
    int16_t hitag{0};
    int16_t extra{-1};       // Blood XSECTOR reference index

    // Monolith Blood XSECTOR attributes (Dynamic lighting, water, motion, crushing)
    struct BloodXSector {
        uint16_t lightPhase{0};
        uint16_t waveForm{0};
        int16_t shadeAnimation{0};
        uint16_t rxId{0};
        uint16_t txId{0};
        int32_t floorSpeed{0};
        int32_t ceilingSpeed{0};
        bool isUnderwater{false};
        bool isDamageSector{false};
        int damageAmount{0};
    } xsectorData;

    ExtraDataMap userProps;
};

struct UBMFSprite {
    int id{-1};
    Vector3D position{0.0, 0.0, 0.0};
    int16_t cstat{0};       // Sprite flags (face, wall, floor, alignment, translucent)
    int16_t picnum{0};      // Sprite tile index
    int8_t shade{0};
    uint8_t pal{0};
    uint8_t clipdist{0};
    uint8_t xrepeat{64};    // Scale X
    uint8_t yrepeat{64};    // Scale Y
    int8_t xoffset{0};
    int8_t yoffset{0};

    int16_t sectnum{-1};    // Containing sector
    int16_t statnum{0};     // Engine status list index
    int16_t ang{0};         // Angle (0 - 2047)
    int16_t owner{-1};
    int16_t xvel{0};
    int16_t yvel{0};
    int16_t zvel{0};
    int16_t lotag{0};
    int16_t hitag{0};
    int16_t extra{-1};      // Blood XSPRITE index

    // Monolith Blood XSPRITE extensions (AI state, triggers, sound IDs, spawn data)
    struct BloodXSprite {
        uint16_t rxId{0};
        uint16_t txId{0};
        uint16_t state{0};
        uint16_t busy{0};
        uint16_t respawnPending{0};
        uint16_t dropItem{0};
        int health{100};
        bool isGoalTarget{false};
    } xspriteData;

    ExtraDataMap userProps;
};

struct UBMFHeader {
    std::string formatVersion{"1.0.0-WAKUBLOOD"};
    std::string mapTitle{"Untitled WakuBlood Map"};
    std::string author{"Anonymous Cultist"};
    std::string musicTrack{"Pebbles.ogg"};
    int skyTileId{80};
    int versionNum{7};     // Build MAP version 7 (Blood standard)
    Vector3D playerStart{0.0, 0.0, 0.0};
    int16_t startAngle{0};
    int16_t startSector{0};
    ExtraDataMap customGlobalProps;
};

class UBMFMap {
public:
    UBMFHeader header;
    std::vector<UBMFVertex> vertices;
    std::vector<UBMFWall> walls;
    std::vector<UBMFSector> sectors;
    std::vector<UBMFSprite> sprites;

    void Clear() {
        header = UBMFHeader();
        vertices.clear();
        walls.clear();
        sectors.clear();
        sprites.clear();
    }

    // Helper to add a vertex and return its assigned index
    int AddVertex(double x, double y) {
        int id = static_cast<int>(vertices.size());
        vertices.emplace_back(id, x, y);
        return id;
    }

    // Helper to add a sector
    int AddSector(const UBMFSector& sec) {
        int id = static_cast<int>(sectors.size());
        UBMFSector s = sec;
        s.id = id;
        sectors.push_back(s);
        return id;
    }

    // Helper to add a wall
    int AddWall(const UBMFWall& w) {
        int id = static_cast<int>(walls.size());
        UBMFWall wall = w;
        wall.id = id;
        walls.push_back(wall);
        return id;
    }

    // Helper to add a sprite
    int AddSprite(const UBMFSprite& sp) {
        int id = static_cast<int>(sprites.size());
        UBMFSprite sprite = sp;
        sprite.id = id;
        sprites.push_back(sprite);
        return id;
    }
};

enum class TokenType {
    Identifier,
    StringLiteral,
    NumberLiteral,
    OpenBrace,
    CloseBrace,
    Equals,
    Semicolon,
    EndOfFile,
    Unknown
};

struct Token {
    TokenType type{TokenType::Unknown};
    std::string text;
    size_t line{1};
};

class Lexer {
private:
    std::string input;
    size_t pos{0};
    size_t currentLine{1};

    char Peek() const {
        if (pos >= input.size()) return '\0';
        return input[pos];
    }

    char Get() {
        if (pos >= input.size()) return '\0';
        char c = input[pos++];
        if (c == '\n') currentLine++;
        return c;
    }

    void SkipWhitespaceAndComments() {
        while (pos < input.size()) {
            char c = Peek();
            if (std::isspace(c)) {
                Get();
            } else if (c == '/' && pos + 1 < input.size() && input[pos + 1] == '/') {
                // Line comment
                while (pos < input.size() && Peek() != '\n') Get();
            } else if (c == '/' && pos + 1 < input.size() && input[pos + 1] == '*') {
                // Block comment
                Get(); Get();
                while (pos + 1 < input.size() && !(input[pos] == '*' && input[pos + 1] == '/')) {
                    Get();
                }
                if (pos + 1 < input.size()) { Get(); Get(); } // Consume */
            } else {
                break;
            }
        }
    }

public:
    explicit Lexer(std::string textData) : input(std::move(textData)) {}

    Token NextToken() {
        SkipWhitespaceAndComments();

        if (pos >= input.size()) {
            return {TokenType::EndOfFile, "", currentLine};
        }

        char c = Peek();
        size_t tokLine = currentLine;

        if (c == '{') { Get(); return {TokenType::OpenBrace, "{", tokLine}; }
        if (c == '}') { Get(); return {TokenType::CloseBrace, "}", tokLine}; }
        if (c == '=') { Get(); return {TokenType::Equals, "=", tokLine}; }
        if (c == ';') { Get(); return {TokenType::Semicolon, ";", tokLine}; }

        // String literal
        if (c == '"') {
            Get();
            std::string str;
            while (pos < input.size() && Peek() != '"') {
                char ch = Get();
                if (ch == '\\' && pos < input.size()) {
                    char esc = Get();
                    if (esc == 'n') str += '\n';
                    else if (esc == 't') str += '\t';
                    else str += esc;
                } else {
                    str += ch;
                }
            }
            if (Peek() == '"') Get(); // closing quote
            return {TokenType::StringLiteral, str, tokLine};
        }

        // Identifier
        if (std::isalpha(c) || c == '_') {
            std::string ident;
            while (pos < input.size() && (std::isalnum(Peek()) || Peek() == '_' || Peek() == '.')) {
                ident += Get();
            }
            return {TokenType::Identifier, ident, tokLine};
        }

        // Numbers (integers / floats)
        if (std::isdigit(c) || c == '-' || c == '+') {
            std::string num;
            num += Get();
            while (pos < input.size() && (std::isdigit(Peek()) || Peek() == '.')) {
                num += Get();
            }
            return {TokenType::NumberLiteral, num, tokLine};
        }

        // Unknown symbol fallback
        std::string unk(1, Get());
        return {TokenType::Unknown, unk, tokLine};
    }
};

class UBMFParser {
private:
    Lexer lexer;
    Token currentToken;

    void Consume() {
        currentToken = lexer.NextToken();
    }

    void Expect(TokenType type, const std::string& errorMsg) {
        if (currentToken.type != type) {
            throw std::runtime_error("UBMF Parse Error (Line " + std::to_string(currentToken.line) + 
                                     "): " + errorMsg + " (found '" + currentToken.text + "')");
        }
        Consume();
    }

public:
    explicit UBMFParser(const std::string& inputContent) : lexer(inputContent) {
        Consume(); // Load first token
    }

    bool ParseMap(UBMFMap& outMap) {
        outMap.Clear();

        try {
            while (currentToken.type != TokenType::EndOfFile) {
                if (currentToken.type == TokenType::Identifier) {
                    std::string blockType = currentToken.text;
                    Consume();

                    Expect(TokenType::OpenBrace, "Expected '{' after block name");

                    if (blockType == "namespace") {
                        // Namespace definition block
                        Token nsTok = currentToken;
                        Expect(TokenType::StringLiteral, "Expected string for namespace name");
                        outMap.header.formatVersion = nsTok.text;
                        Expect(TokenType::Semicolon, "Expected ';' after namespace");
                        Expect(TokenType::CloseBrace, "Expected '}' to close namespace block");
                    } else if (blockType == "header") {
                        ParseHeaderBlock(outMap.header);
                    } else if (blockType == "vertex") {
                        UBMFVertex v;
                        ParseVertexBlock(v);
                        outMap.vertices.push_back(v);
                    } else if (blockType == "wall") {
                        UBMFWall w;
                        ParseWallBlock(w);
                        outMap.walls.push_back(w);
                    } else if (blockType == "sector") {
                        UBMFSector s;
                        ParseSectorBlock(s);
                        outMap.sectors.push_back(s);
                    } else if (blockType == "sprite") {
                        UBMFSprite sp;
                        ParseSpriteBlock(sp);
                        outMap.sprites.push_back(sp);
                    } else {
                        // Unknown block - skip gracefully
                        SkipBlock();
                    }
                } else {
                    Consume();
                }
            }
            return true;
        } catch (const std::exception& ex) {
            std::cerr << "[UBMF Parser Error] " << ex.what() << "\n";
            return false;
        }
    }

private:
    void SkipBlock() {
        int depth = 1;
        while (depth > 0 && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::OpenBrace) depth++;
            else if (currentToken.type == TokenType::CloseBrace) depth--;
            Consume();
        }
    }

    void ParseHeaderBlock(UBMFHeader& header) {
        while (currentToken.type != TokenType::CloseBrace && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::Identifier) {
                std::string key = currentToken.text;
                Consume();
                Expect(TokenType::Equals, "Expected '=' in key-value assignment");

                if (key == "maptitle") {
                    header.mapTitle = currentToken.text; Consume();
                } else if (key == "author") {
                    header.author = currentToken.text; Consume();
                } else if (key == "music") {
                    header.musicTrack = currentToken.text; Consume();
                } else if (key == "skytile") {
                    header.skyTileId = std::stoi(currentToken.text); Consume();
                } else if (key == "start_x") {
                    header.playerStart.x = std::stod(currentToken.text); Consume();
                } else if (key == "start_y") {
                    header.playerStart.y = std::stod(currentToken.text); Consume();
                } else if (key == "start_z") {
                    header.playerStart.z = std::stod(currentToken.text); Consume();
                } else if (key == "start_ang") {
                    header.startAngle = static_cast<int16_t>(std::stoi(currentToken.text)); Consume();
                } else if (key == "start_sect") {
                    header.startSector = static_cast<int16_t>(std::stoi(currentToken.text)); Consume();
                } else {
                    header.customGlobalProps[key] = AttributeValue::MakeString(currentToken.text);
                    Consume();
                }
                Expect(TokenType::Semicolon, "Expected ';' after property value");
            } else {
                Consume();
            }
        }
        Expect(TokenType::CloseBrace, "Expected '}' at end of header block");
    }

    void ParseVertexBlock(UBMFVertex& vert) {
        while (currentToken.type != TokenType::CloseBrace && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::Identifier) {
                std::string key = currentToken.text;
                Consume();
                Expect(TokenType::Equals, "Expected '='");
                if (key == "id") vert.id = std::stoi(currentToken.text);
                else if (key == "x") vert.position.x = std::stod(currentToken.text);
                else if (key == "y") vert.position.y = std::stod(currentToken.text);
                else vert.userProps[key] = AttributeValue::MakeString(currentToken.text);
                Consume();
                Expect(TokenType::Semicolon, "Expected ';'");
            } else {
                Consume();
            }
        }
        Expect(TokenType::CloseBrace, "Expected '}'");
    }

    void ParseWallBlock(UBMFWall& wall) {
        while (currentToken.type != TokenType::CloseBrace && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::Identifier) {
                std::string key = currentToken.text;
                Consume();
                Expect(TokenType::Equals, "Expected '='");
                if (key == "id") wall.id = std::stoi(currentToken.text);
                else if (key == "point2") wall.point2 = std::stoi(currentToken.text);
                else if (key == "wall2") wall.wall2 = std::stoi(currentToken.text);
                else if (key == "nextsector") wall.nextsector = std::stoi(currentToken.text);
                else if (key == "picnum") wall.picnum = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "cstat") wall.cstat = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "shade") wall.shade = static_cast<int8_t>(std::stoi(currentToken.text));
                else if (key == "pal") wall.pal = static_cast<uint8_t>(std::stoi(currentToken.text));
                else if (key == "lotag") wall.lotag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "hitag") wall.hitag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "blood_rx") wall.xwallData.rxId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else if (key == "blood_tx") wall.xwallData.txId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else wall.userProps[key] = AttributeValue::MakeString(currentToken.text);
                Consume();
                Expect(TokenType::Semicolon, "Expected ';'");
            } else {
                Consume();
            }
        }
        Expect(TokenType::CloseBrace, "Expected '}'");
    }

    void ParseSectorBlock(UBMFSector& sec) {
        while (currentToken.type != TokenType::CloseBrace && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::Identifier) {
                std::string key = currentToken.text;
                Consume();
                Expect(TokenType::Equals, "Expected '='");
                if (key == "id") sec.id = std::stoi(currentToken.text);
                else if (key == "wallptr") sec.wallptr = std::stoi(currentToken.text);
                else if (key == "wallnum") sec.wallnum = std::stoi(currentToken.text);
                else if (key == "floorz") sec.floorz = std::stoi(currentToken.text);
                else if (key == "ceilingz") sec.ceilingz = std::stoi(currentToken.text);
                else if (key == "floorpicnum") sec.floorpicnum = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "ceilingpicnum") sec.ceilingpicnum = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "floorshade") sec.floorshade = static_cast<int8_t>(std::stoi(currentToken.text));
                else if (key == "ceilingshade") sec.ceilingshade = static_cast<int8_t>(std::stoi(currentToken.text));
                else if (key == "lotag") sec.lotag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "hitag") sec.hitag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "blood_rx") sec.xsectorData.rxId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else if (key == "blood_tx") sec.xsectorData.txId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else if (key == "blood_underwater") sec.xsectorData.isUnderwater = (currentToken.text == "true" || currentToken.text == "1");
                else sec.userProps[key] = AttributeValue::MakeString(currentToken.text);
                Consume();
                Expect(TokenType::Semicolon, "Expected ';'");
            } else {
                Consume();
            }
        }
        Expect(TokenType::CloseBrace, "Expected '}'");
    }

    void ParseSpriteBlock(UBMFSprite& spr) {
        while (currentToken.type != TokenType::CloseBrace && currentToken.type != TokenType::EndOfFile) {
            if (currentToken.type == TokenType::Identifier) {
                std::string key = currentToken.text;
                Consume();
                Expect(TokenType::Equals, "Expected '='");
                if (key == "id") spr.id = std::stoi(currentToken.text);
                else if (key == "x") spr.position.x = std::stod(currentToken.text);
                else if (key == "y") spr.position.y = std::stod(currentToken.text);
                else if (key == "z") spr.position.z = std::stod(currentToken.text);
                else if (key == "picnum") spr.picnum = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "cstat") spr.cstat = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "sectnum") spr.sectnum = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "ang") spr.ang = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "lotag") spr.lotag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "hitag") spr.hitag = static_cast<int16_t>(std::stoi(currentToken.text));
                else if (key == "blood_rx") spr.xspriteData.rxId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else if (key == "blood_tx") spr.xspriteData.txId = static_cast<uint16_t>(std::stoi(currentToken.text));
                else if (key == "blood_health") spr.xspriteData.health = std::stoi(currentToken.text);
                else spr.userProps[key] = AttributeValue::MakeString(currentToken.text);
                Consume();
                Expect(TokenType::Semicolon, "Expected ';'");
            } else {
                Consume();
            }
        }
        Expect(TokenType::CloseBrace, "Expected '}'");
    }
};

class UBMFSerializer {
public:
    static std::string Serialize(const UBMFMap& map) {
        std::ostringstream ss;

        ss << "// Universal Build Map Format (UBMF) - Exported for WakuBlood\n";
        ss << "// Engine Specification: Monolith Blood / Build Engine v7\n\n";

        // Global format version
        ss << "namespace = \"" << map.header.formatVersion << "\";\n\n";

        // Header metadata block
        ss << "header\n{\n";
        ss << "    maptitle = \"" << map.header.mapTitle << "\";\n";
        ss << "    author = \"" << map.header.author << "\";\n";
        ss << "    music = \"" << map.header.musicTrack << "\";\n";
        ss << "    skytile = " << map.header.skyTileId << ";\n";
        ss << "    start_x = " << std::fixed << std::setprecision(2) << map.header.playerStart.x << ";\n";
        ss << "    start_y = " << map.header.playerStart.y << ";\n";
        ss << "    start_z = " << map.header.playerStart.z << ";\n";
        ss << "    start_ang = " << map.header.startAngle << ";\n";
        ss << "    start_sect = " << map.header.startSector << ";\n";

        for (const auto& [k, v] : map.header.customGlobalProps) {
            ss << "    " << k << " = " << v.ToString() << ";\n";
        }
        ss << "}\n\n";

        ss << "// ============================================================================\n";
        ss << "// VERTICES (" << map.vertices.size() << ")\n";
        ss << "// ============================================================================\n";
        for (const auto& v : map.vertices) {
            ss << "vertex // " << v.id << "\n{\n";
            ss << "    id = " << v.id << ";\n";
            ss << "    x = " << std::fixed << std::setprecision(2) << v.position.x << ";\n";
            ss << "    y = " << v.position.y << ";\n";
            for (const auto& [k, val] : v.userProps) {
                ss << "    " << k << " = " << val.ToString() << ";\n";
            }
            ss << "}\n\n";
        }

        ss << "// ============================================================================\n";
        ss << "// WALLS (" << map.walls.size() << ")\n";
        ss << "// ============================================================================\n";
        for (const auto& w : map.walls) {
            ss << "wall // " << w.id << "\n{\n";
            ss << "    id = " << w.id << ";\n";
            ss << "    point2 = " << w.point2 << ";\n";
            ss << "    wall2 = " << w.wall2 << ";\n";
            ss << "    nextsector = " << w.nextsector << ";\n";
            ss << "    picnum = " << w.picnum << ";\n";
            ss << "    cstat = " << w.cstat << ";\n";
            ss << "    shade = " << static_cast<int>(w.shade) << ";\n";
            ss << "    pal = " << static_cast<int>(w.pal) << ";\n";
            ss << "    lotag = " << w.lotag << ";\n";
            ss << "    hitag = " << w.hitag << ";\n";
            if (w.xwallData.rxId > 0) ss << "    blood_rx = " << w.xwallData.rxId << ";\n";
            if (w.xwallData.txId > 0) ss << "    blood_tx = " << w.xwallData.txId << ";\n";
            for (const auto& [k, val] : w.userProps) {
                ss << "    " << k << " = " << val.ToString() << ";\n";
            }
            ss << "}\n\n";
        }

        ss << "// ============================================================================\n";
        ss << "// SECTORS (" << map.sectors.size() << ")\n";
        ss << "// ============================================================================\n";
        for (const auto& s : map.sectors) {
            ss << "sector // " << s.id << "\n{\n";
            ss << "    id = " << s.id << ";\n";
            ss << "    wallptr = " << s.wallptr << ";\n";
            ss << "    wallnum = " << s.wallnum << ";\n";
            ss << "    floorz = " << s.floorz << ";\n";
            ss << "    ceilingz = " << s.ceilingz << ";\n";
            ss << "    floorpicnum = " << s.floorpicnum << ";\n";
            ss << "    ceilingpicnum = " << s.ceilingpicnum << ";\n";
            ss << "    floorshade = " << static_cast<int>(s.floorshade) << ";\n";
            ss << "    ceilingshade = " << static_cast<int>(s.ceilingshade) << ";\n";
            ss << "    lotag = " << s.lotag << ";\n";
            ss << "    hitag = " << s.hitag << ";\n";
            if (s.xsectorData.rxId > 0) ss << "    blood_rx = " << s.xsectorData.rxId << ";\n";
            if (s.xsectorData.txId > 0) ss << "    blood_tx = " << s.xsectorData.txId << ";\n";
            if (s.xsectorData.isUnderwater) ss << "    blood_underwater = true;\n";
            for (const auto& [k, val] : s.userProps) {
                ss << "    " << k << " = " << val.ToString() << ";\n";
            }
            ss << "}\n\n";
        }

        ss << "// ============================================================================\n";
        ss << "// SPRITES (" << map.sprites.size() << ")\n";
        ss << "// ============================================================================\n";
        for (const auto& spr : map.sprites) {
            ss << "sprite // " << spr.id << "\n{\n";
            ss << "    id = " << spr.id << ";\n";
            ss << "    x = " << std::fixed << std::setprecision(2) << spr.position.x << ";\n";
            ss << "    y = " << spr.position.y << ";\n";
            ss << "    z = " << spr.position.z << ";\n";
            ss << "    picnum = " << spr.picnum << ";\n";
            ss << "    cstat = " << spr.cstat << ";\n";
            ss << "    sectnum = " << spr.sectnum << ";\n";
            ss << "    ang = " << spr.ang << ";\n";
            ss << "    lotag = " << spr.lotag << ";\n";
            ss << "    hitag = " << spr.hitag << ";\n";
            if (spr.xspriteData.rxId > 0) ss << "    blood_rx = " << spr.xspriteData.rxId << ";\n";
            if (spr.xspriteData.txId > 0) ss << "    blood_tx = " << spr.xspriteData.txId << ";\n";
            if (spr.xspriteData.health != 100) ss << "    blood_health = " << spr.xspriteData.health << ";\n";
            for (const auto& [k, val] : spr.userProps) {
                ss << "    " << k << " = " << val.ToString() << ";\n";
            }
            ss << "}\n\n";
        }

        return ss.str();
    }
};

struct ValidationResult {
    bool isValid{true};
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

class MapValidator {
public:
    static ValidationResult Validate(const UBMFMap& map) {
        ValidationResult result;

        // 1. Check Vertex references in walls
        for (size_t i = 0; i < map.walls.size(); ++i) {
            const auto& wall = map.walls[i];
            if (wall.point2 < 0 || wall.point2 >= static_cast<int>(map.vertices.size())) {
                result.errors.push_back("Wall " + std::to_string(i) + " points to invalid vertex index point2 = " + std::to_string(wall.point2));
                result.isValid = false;
            }
            if (wall.nextsector >= static_cast<int>(map.sectors.size())) {
                result.errors.push_back("Wall " + std::to_string(i) + " points to invalid nextsector = " + std::to_string(wall.nextsector));
                result.isValid = false;
            }
            if (wall.wall2 >= static_cast<int>(map.walls.size())) {
                result.errors.push_back("Wall " + std::to_string(i) + " points to invalid matching wall2 = " + std::to_string(wall.wall2));
                result.isValid = false;
            }
        }

        // 2. Check sector wall counts and loops
        for (size_t s = 0; s < map.sectors.size(); ++s) {
            const auto& sector = map.sectors[s];
            if (sector.wallnum < 3) {
                result.errors.push_back("Sector " + std::to_string(s) + " has fewer than 3 walls (" + std::to_string(sector.wallnum) + ")");
                result.isValid = false;
            }
            if (sector.wallptr < 0 || sector.wallptr + sector.wallnum > static_cast<int>(map.walls.size())) {
                result.errors.push_back("Sector " + std::to_string(s) + " wall range [" + std::to_string(sector.wallptr) + 
                                       ".." + std::to_string(sector.wallptr + sector.wallnum) + "] out of bounds");
                result.isValid = false;
            }
            if (sector.ceilingz >= sector.floorz) {
                result.warnings.push_back("Sector " + std::to_string(s) + " has inverted ceiling/floor heights (Ceiling Z >= Floor Z in Build coordinates)");
            }
        }

        // 3. Check sprite sector bounds
        for (size_t sp = 0; sp < map.sprites.size(); ++sp) {
            const auto& sprite = map.sprites[sp];
            if (sprite.sectnum < 0 || sprite.sectnum >= static_cast<int>(map.sectors.size())) {
                result.warnings.push_back("Sprite " + std::to_string(sp) + " (Tile " + std::to_string(sprite.picnum) + 
                                          ") assigned to invalid sector " + std::to_string(sprite.sectnum));
            }
        }

        return result;
    }
};

class LegacyBloodMapConverter {
public:
    // Builds a simple demonstration Blood room map programmatically to simulate legacy binary conversion
    static UBMFMap BuildSampleBloodCryptMap() {
        UBMFMap map;

        map.header.formatVersion = "1.0.0-WAKUBLOOD";
        map.header.mapTitle = "E1M1: Crypt of Caleb";
        map.header.author = "Monolith / WakuBlood Team";
        map.header.musicTrack = "CRYPT.OGG";
        map.header.skyTileId = 84;
        map.header.playerStart = Vector3D(0.0, 0.0, 0.0);
        map.header.startAngle = 512;
        map.header.startSector = 0;

        // Main Crypt Room (Sector 0) - Square 1024x1024
        int v0 = map.AddVertex(-512.0, -512.0);
        int v1 = map.AddVertex(512.0, -512.0);
        int v2 = map.AddVertex(512.0, 512.0);
        int v3 = map.AddVertex(-512.0, 512.0);

        // Walls for Main Crypt
        UBMFWall w0; w0.point2 = v1; w0.picnum = 500; w0.cstat = 1; map.AddWall(w0);
        UBMFWall w1; w1.point2 = v2; w1.picnum = 500; w1.cstat = 1; map.AddWall(w1);
        UBMFWall w2; w2.point2 = v3; w2.picnum = 500; w2.cstat = 1; map.AddWall(w2);
        UBMFWall w3; w3.point2 = v0; w3.picnum = 500; w3.cstat = 1; map.AddWall(w3);

        UBMFSector cryptSec;
        cryptSec.wallptr = 0;
        cryptSec.wallnum = 4;
        cryptSec.ceilingz = -16384;
        cryptSec.floorz = 0;
        cryptSec.ceilingpicnum = 220; // Stone ceiling
        cryptSec.floorpicnum = 140;   // Blood stone floor
        cryptSec.xsectorData.rxId = 100;
        cryptSec.xsectorData.txId = 0;
        map.AddSector(cryptSec);

        // Add Caleb player spawn sprite
        UBMFSprite playerSpawn;
        playerSpawn.position = Vector3D(0.0, 0.0, -100.0);
        playerSpawn.picnum = 2500; // Caleb start sprite tile
        playerSpawn.sectnum = 0;
        playerSpawn.ang = 512;
        playerSpawn.lotag = 1; // Start position lotag
        map.AddSprite(playerSpawn);

        // Add Zombie Cultist Enemy
        UBMFSprite zombie;
        zombie.position = Vector3D(200.0, 200.0, -100.0);
        zombie.picnum = 3100; // Cultist Zombie tile
        zombie.sectnum = 0;
        zombie.xspriteData.health = 120;
        zombie.xspriteData.rxId = 101;
        zombie.xspriteData.txId = 102;
        zombie.userProps["ai_type"] = AttributeValue::MakeString("cultist_shotgun");
        map.AddSprite(zombie);

        return map;
    }
};

} // namespace UBMF
} // namespace WakuBlood

int main() {
    using namespace WakuBlood::UBMF;

    std::cout << "========================================================================\n";
    std::cout << "        WakuBlood Engine - Universal Build Map Format (UBMF) Core       \n";
    std::cout << "========================================================================\n\n";

    // Step 1: Create sample Blood map using internal builder
    std::cout << "[1] Generating sample Monolith Blood level ('E1M1: Crypt of Caleb')...\n";
    UBMFMap map = LegacyBloodMapConverter::BuildSampleBloodCryptMap();

    std::cout << "    - Vertices: " << map.vertices.size() << "\n";
    std::cout << "    - Walls:    " << map.walls.size() << "\n";
    std::cout << "    - Sectors:  " << map.sectors.size() << "\n";
    std::cout << "    - Sprites:  " << map.sprites.size() << "\n\n";

    // Step 2: Validate map geometry integrity
    std::cout << "[2] Running Map Geometry Integrity Checker...\n";
    ValidationResult val = MapValidator::Validate(map);
    if (val.isValid) {
        std::cout << "    [SUCCESS] Map geometry and sector loops are valid!\n";
    } else {
        std::cout << "    [ERROR] Map geometry validation failed!\n";
        for (const auto& err : val.errors) {
            std::cout << "      * " << err << "\n";
        }
    }
    for (const auto& warn : val.warnings) {
        std::cout << "      [WARNING] " << warn << "\n";
    }
    std::cout << "\n";

    // Step 3: Serialize map to UBMF text format
    std::cout << "[3] Serializing map structure to Universal Build Map Format (UBMF)...\n";
    std::string ubmfTextData = UBMFSerializer::Serialize(map);

    std::cout << "--- BEGIN UBMF TEXT PREVIEW ---\n";
    std::cout << ubmfTextData.substr(0, 1200) << "\n... [truncated for preview] ...\n";
    std::cout << "--- END UBMF TEXT PREVIEW ---\n\n";

    // Step 4: Test parsing the UBMF text string back into memory
    std::cout << "[4] Parsing serialized UBMF text back into memory via Lexer & Parser...\n";
    UBMFParser parser(ubmfTextData);
    UBMFMap reloadedMap;
    if (parser.ParseMap(reloadedMap)) {
        std::cout << "    [SUCCESS] UBMF map successfully parsed!\n";
        std::cout << "    - Parsed Title:    " << reloadedMap.header.mapTitle << "\n";
        std::cout << "    - Parsed Author:   " << reloadedMap.header.author << "\n";
        std::cout << "    - Parsed Vertices: " << reloadedMap.vertices.size() << "\n";
        std::cout << "    - Parsed Walls:    " << reloadedMap.walls.size() << "\n";
        std::cout << "    - Parsed Sectors:  " << reloadedMap.sectors.size() << "\n";
        std::cout << "    - Parsed Sprites:  " << reloadedMap.sprites.size() << "\n";
    } else {
        std::cout << "    [ERROR] Failed to parse UBMF text string!\n";
    }

    std::cout << "\n========================================================================\n";
    std::cout << "                     UBMF System Test Completed                         \n";
    std::cout << "========================================================================\n";

    return 0;
}
