#pragma once
#include "typedef.h"
#include <type_traits>

enum AssetType : u8 {
	ASSET_TYPE_CHR_BANK,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_TILESET,
	ASSET_TYPE_METASPRITE,
	ASSET_TYPE_ACTOR_PROTOTYPE,
	ASSET_TYPE_ROOM_TEMPLATE,
	ASSET_TYPE_DUNGEON,
	ASSET_TYPE_OVERWORLD,
	ASSET_TYPE_ANIMATION,
	ASSET_TYPE_PALETTE,
	ASSET_TYPE_SHADER,

	ASSET_TYPE_COUNT,
};

template <AssetType T>
struct AssetDataType {
	using Type = void;
};

template <AssetType T>
struct AssetHandle {
	u64 id;

	constexpr AssetHandle() = default;
	constexpr AssetHandle(const AssetHandle& other) = default;
	explicit constexpr AssetHandle(u64 id) : id(id) {}

	bool operator==(const AssetHandle& other) const {
		return id == other.id;
	}
	bool operator!=(const AssetHandle& other) const {
		return id != other.id;
	}

	inline static AssetHandle Null() {
		return AssetHandle(0);
	}
};

template <typename T>
struct AssetHandleTraits : std::false_type {};

template <AssetType T>
struct AssetHandleTraits<AssetHandle<T>> : std::true_type {
	using asset_type = std::integral_constant<AssetType, T>;
	using data_type = typename AssetDataType<T>::Type;
};

template <typename T>
concept IsAssetHandle = AssetHandleTraits<T>::value;

struct ChrSheet;
template <>
struct AssetDataType<ASSET_TYPE_CHR_BANK> {
	using Type = ChrSheet;
};

struct Sound;
template <>
struct AssetDataType<ASSET_TYPE_SOUND> {
	using Type = Sound;
};

struct Tileset;
template <>
struct AssetDataType<ASSET_TYPE_TILESET> {
	using Type = Tileset;
};

struct Metasprite;
template <>
struct AssetDataType<ASSET_TYPE_METASPRITE> {
	using Type = Metasprite;
};

struct ActorPrototype;
template <>
struct AssetDataType<ASSET_TYPE_ACTOR_PROTOTYPE> {
	using Type = ActorPrototype;
};

struct RoomTemplate;
template <>
struct AssetDataType<ASSET_TYPE_ROOM_TEMPLATE> {
	using Type = RoomTemplate;
};

struct Dungeon;
template <>
struct AssetDataType<ASSET_TYPE_DUNGEON> {
	using Type = Dungeon;
};

struct Overworld;
template <>
struct AssetDataType<ASSET_TYPE_OVERWORLD> {
	using Type = Overworld;
};

struct Animation;
template <>
struct AssetDataType<ASSET_TYPE_ANIMATION> {
	using Type = Animation;
};

struct Palette;
template <>
struct AssetDataType<ASSET_TYPE_PALETTE> {
	using Type = Palette;
};

struct Shader;
template <>
struct AssetDataType<ASSET_TYPE_SHADER> {
	using Type = Shader;
};

typedef AssetHandle<ASSET_TYPE_CHR_BANK> ChrBankHandle;
typedef AssetHandle<ASSET_TYPE_SOUND> SoundHandle;
typedef AssetHandle<ASSET_TYPE_TILESET> TilesetHandle;
typedef AssetHandle<ASSET_TYPE_METASPRITE> MetaspriteHandle;
typedef AssetHandle<ASSET_TYPE_ACTOR_PROTOTYPE> ActorPrototypeHandle;
typedef AssetHandle<ASSET_TYPE_ROOM_TEMPLATE> RoomTemplateHandle;
typedef AssetHandle<ASSET_TYPE_DUNGEON> DungeonHandle;
typedef AssetHandle<ASSET_TYPE_OVERWORLD> OverworldHandle;
typedef AssetHandle<ASSET_TYPE_ANIMATION> AnimationHandle;
typedef AssetHandle<ASSET_TYPE_PALETTE> PaletteHandle;
typedef AssetHandle<ASSET_TYPE_SHADER> ShaderHandle;

#ifdef EDITOR
constexpr const char* ASSET_TYPE_NAMES[ASSET_TYPE_COUNT] = { "Chr bank", "Sound", "Tileset", "Metasprite", "Actor prototype", "Room template", "Dungeon", "Overworld", "Animation", "Palette", "Shader" };
#endif
