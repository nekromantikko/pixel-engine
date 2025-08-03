#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "typedef.h"
#include "asset_types.h"

#include <climits>

#pragma region Graphics
constexpr u32 MAX_SPRITE_COUNT = 4096;
constexpr u32 MAX_SPRITES_PER_SCANLINE = 64;

constexpr u32 BPP = 3;
constexpr u32 TILE_DIM_PIXELS = 8;
constexpr u32 TILE_BYTES = (TILE_DIM_PIXELS * TILE_DIM_PIXELS * BPP) / CHAR_BIT;

constexpr u32 METATILE_DIM_TILES = 2;
constexpr u32 METATILE_TILE_COUNT = METATILE_DIM_TILES * METATILE_DIM_TILES;
constexpr u32 METATILE_DIM_PIXELS = TILE_DIM_PIXELS * METATILE_DIM_TILES;

constexpr u32 CHR_DIM_TILES = 16;
constexpr u32 CHR_DIM_PIXELS = CHR_DIM_TILES * TILE_DIM_PIXELS;

constexpr u32 CHR_SIZE_TILES = CHR_DIM_TILES * CHR_DIM_TILES;
constexpr u32 CHR_SIZE_BYTES = CHR_SIZE_TILES * TILE_BYTES;
constexpr u32 CHR_PAGE_COUNT = 4;
constexpr u32 CHR_COUNT = CHR_PAGE_COUNT * 2;
constexpr u32 CHR_MEMORY_SIZE = CHR_SIZE_BYTES * CHR_COUNT;

constexpr u32 NAMETABLE_COUNT = 2;
constexpr u32 NAMETABLE_DIM_TILES = 64;
constexpr u32 NAMETABLE_DIM_TILES_LOG2 = 6;
constexpr u32 NAMETABLE_SIZE_TILES = NAMETABLE_DIM_TILES * NAMETABLE_DIM_TILES;
constexpr u32 NAMETABLE_DIM_METATILES = NAMETABLE_DIM_TILES >> 1;
constexpr u32 NAMETABLE_SIZE_METATILES = NAMETABLE_SIZE_TILES >> 2;

constexpr u32 NAMETABLE_DIM_PIXELS = NAMETABLE_DIM_TILES * TILE_DIM_PIXELS;

constexpr u32 VIEWPORT_WIDTH_TILES = NAMETABLE_DIM_TILES;
constexpr u32 VIEWPORT_HEIGHT_TILES = 36;
constexpr u32 VIEWPORT_SIZE_TILES = VIEWPORT_WIDTH_TILES * VIEWPORT_HEIGHT_TILES;
constexpr u32 VIEWPORT_WIDTH_PIXELS = VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_HEIGHT_PIXELS = VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_WIDTH_METATILES = VIEWPORT_WIDTH_TILES >> 1;
constexpr u32 VIEWPORT_HEIGHT_METATILES = VIEWPORT_HEIGHT_TILES >> 1;
constexpr u32 VIEWPORT_SIZE_METATILES = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;

constexpr u32 COLOR_COUNT = 0x80;

constexpr u32 PALETTE_COUNT = 16;
constexpr u32 BG_PALETTE_COUNT = PALETTE_COUNT / 2;
constexpr u32 FG_PALETTE_COUNT = PALETTE_COUNT / 2;
constexpr u32 PALETTE_COLOR_COUNT = 8;
constexpr u32 PALETTE_MEMORY_SIZE = PALETTE_COUNT * PALETTE_COLOR_COUNT;

constexpr u32 SCANLINE_COUNT = VIEWPORT_HEIGHT_PIXELS;

struct alignas(4) Sprite {
	// y is first so we can easily set it offscreen when clearing
	s16 y;
	s16 x;
	u16 tileId : 10;
	u16 palette : 3;
	u16 priority : 1;
	u16 flipHorizontal : 1;
	u16 flipVertical : 1;
};

struct Scanline {
	s32 scrollX;
	s32 scrollY;
};

struct ChrTile {
	u64 p0;
	u64 p1;
	u64 p2;
};

struct ChrSheet {
	ChrTile tiles[0x100];
};

struct BgTile {
	u16 tileId : 10;
	u16 palette : 3;
	u16 unused : 1;
	u16 flipHorizontal : 1;
	u16 flipVertical : 1;
};

struct Nametable {
	BgTile tiles[NAMETABLE_SIZE_TILES];
};

struct Palette {
	u8 colors[PALETTE_COLOR_COUNT];
};

struct Metatile {
	BgTile tiles[METATILE_TILE_COUNT];
};

struct Metasprite {
	u32 spriteCount;
	// Sprite positions are relative to the metasprite origin
	u32 spritesOffset;

	inline Sprite* GetSprites() const {
		return (Sprite*)((u8*)this + spritesOffset);
	}
};
#pragma endregion

#pragma region Tilemaps
constexpr u32 TILESET_DIM = 16;
constexpr u32 TILESET_DIM_LOG2 = 4;
constexpr u32 TILESET_SIZE = TILESET_DIM * TILESET_DIM;
constexpr u32 TILESET_DIM_ATTRIBUTES = TILESET_DIM >> 1;
constexpr u32 TILESET_ATTRIBUTE_COUNT = TILESET_DIM_ATTRIBUTES * TILESET_DIM_ATTRIBUTES;

static_assert(TILESET_DIM == (1 << TILESET_DIM_LOG2));

enum TilesetTileType : s32 {
	TILE_EMPTY = 0,
	TILE_SOLID = 1,
	TILE_TYPE_COUNT
};

#ifdef EDITOR
constexpr const char* METATILE_TYPE_NAMES[TILE_TYPE_COUNT] = { "Empty", "Solid" };
#endif

struct TilesetTile {
	s32 type;
	Metatile metatile;
};

struct Tileset {
	TilesetTile tiles[TILESET_SIZE];
};

struct Tilemap {
	u32 width;
	u32 height;
	TilesetHandle tilesetHandle;
	u32 tilesOffset;

	inline u8* GetTileData() const {
		return (u8*)this + tilesOffset;
	}
};
#pragma endregion

#pragma region Collision
struct AABB {
	union {
		struct {
			r32 x1, y1;
		};
		glm::vec2 min;
	};
	union {
		struct {
			r32 x2, y2;
		};
		glm::vec2 max;
	};

	AABB() : min{}, max{} {}
	AABB(r32 x1, r32 x2, r32 y1, r32 y2) : x1(x1), x2(x2), y1(y1), y2(y2) {}
	AABB(const glm::vec2& min, const glm::vec2& max) : min(min), max(max) {}
};

// Blatant plagiarism from unreal engine
struct HitResult {
	bool32 blockingHit;
	bool32 startPenetrating;
	r32 distance;
	glm::vec2 impactNormal;
	glm::vec2 impactPoint;
	glm::vec2 location;
	glm::vec2 normal;
	u32 tileType;
};
#pragma endregion

#pragma region Audio
enum SoundChannelId {
	CHAN_ID_PULSE0 = 0,
	CHAN_ID_PULSE1,
	CHAN_ID_TRIANGLE,
	CHAN_ID_NOISE,
	//CHAN_ID_DPCM,

	CHAN_COUNT
};

struct SoundOperation {
	u8 opCode : 4;
	u8 address : 4;
	u8 data;
};

enum SoundType {
	SOUND_TYPE_SFX = 0,
	SOUND_TYPE_MUSIC,

	SOUND_TYPE_COUNT
};

struct Sound {
	u32 length;
	u32 loopPoint;
	u16 type;
	u16 sfxChannel;
	u32 dataOffset;

	inline SoundOperation* GetData() const {
		return (SoundOperation*)((u8*)this + dataOffset);
	}
};
#pragma endregion

#pragma region Animation
struct AnimationFrame {
	MetaspriteHandle metaspriteId;
};

struct Animation {
	u8 frameLength;
	s16 loopPoint;
	u16 frameCount;
	u32 framesOffset;

	inline AnimationFrame* GetFrames() const {
		return (AnimationFrame*)((u8*)this + framesOffset);
	}
};
#pragma endregion

#pragma region Dungeon
constexpr u32 DUNGEON_GRID_DIM = 32;
constexpr u32 DUNGEON_GRID_SIZE = DUNGEON_GRID_DIM * DUNGEON_GRID_DIM;
constexpr u32 MAX_DUNGEON_ROOM_COUNT = 128;

struct DungeonCell {
	s8 roomIndex = -1;
	u8 screenIndex = 0;
};

struct RoomInstance {
	u32 id;
	RoomTemplateHandle templateId;
};

struct Dungeon {
	u32 roomCount;
	RoomInstance rooms[MAX_DUNGEON_ROOM_COUNT];
	DungeonCell grid[DUNGEON_GRID_SIZE];
};
#pragma endregion

#pragma region Rooms
enum RoomScreenExitDir : u8 {
	SCREEN_EXIT_DIR_RIGHT,
	SCREEN_EXIT_DIR_LEFT,
	SCREEN_EXIT_DIR_BOTTOM,
	SCREEN_EXIT_DIR_TOP,

	SCREEN_EXIT_DIR_DEATH_WARP,
};

constexpr u32 ROOM_MAX_DIM_SCREENS = 4;
constexpr u32 ROOM_MAX_SCREEN_COUNT = ROOM_MAX_DIM_SCREENS * ROOM_MAX_DIM_SCREENS;
constexpr u32 ROOM_SCREEN_TILE_COUNT = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;
constexpr u32 ROOM_MAP_TILE_COUNT = ROOM_MAX_SCREEN_COUNT * 2;
constexpr u32 ROOM_TILE_COUNT = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

struct RoomActor {
	u32 id;
	ActorPrototypeHandle prototypeHandle;
	glm::vec2 position;
};

struct RoomTemplate {
	u8 width;
	u8 height;
	u32 mapTileOffset;
	Tilemap tilemap;
	u32 actorCount;
	u32 actorOffset;

	inline u32 GetMapTileCount() const {
		return width * height * 2; // 2 for each screen
	}

	inline BgTile* GetMapTiles() const {
		return (BgTile*)((u8*)this + mapTileOffset);
	}

	inline RoomActor* GetActors() const {
		return (RoomActor*)((u8*)this + actorOffset);
	}
};
#pragma endregion

#pragma region Overworld
constexpr u32 OVERWORLD_WIDTH_METATILES = 128;
constexpr u32 OVERWORLD_HEIGHT_METATILES = 128;
constexpr u32 OVERWORLD_METATILE_COUNT = OVERWORLD_WIDTH_METATILES * OVERWORLD_HEIGHT_METATILES;
constexpr u32 MAX_OVERWORLD_KEY_AREA_COUNT = 64;

struct OverworldKeyAreaFlags {
	u8 flipDirection : 1;
	u8 passthrough : 1;
};

struct OverworldKeyArea {
	DungeonHandle dungeonId;
	glm::i8vec2 position = { -1, -1 };
	glm::i8vec2 targetGridCell = { 0, 0 };
	OverworldKeyAreaFlags flags;
};

struct Overworld {
	Tilemap tilemap;
	OverworldKeyArea keyAreas[MAX_OVERWORLD_KEY_AREA_COUNT];
};
#pragma endregion
