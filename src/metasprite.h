#pragma once
#include "rendering.h"

constexpr u32 METASPRITE_MAX_SPRITE_COUNT = 64;

struct Metasprite {
	u32 spriteCount;
	Sprite spritesRelativePos[METASPRITE_MAX_SPRITE_COUNT];
};

#ifdef EDITOR
#include <nlohmann/json.hpp>

static void from_json(const nlohmann::json& j, Sprite& sprite) {
	sprite.x = j.at("x").get<s16>();
	sprite.y = j.at("y").get<s16>();
	sprite.tileId = j.at("tile_id").get<u16>();
	sprite.palette = j.at("palette").get<u8>();
	sprite.priority = j.at("priority").get<bool>() ? 1 : 0;
	sprite.flipHorizontal = j.at("flip_horizontal").get<bool>() ? 1 : 0;
	sprite.flipVertical = j.at("flip_vertical").get<bool>() ? 1 : 0;
}

static void to_json(nlohmann::json& j, const Sprite& sprite) {
	j["x"] = sprite.x;
	j["y"] = sprite.y;
	j["tile_id"] = sprite.tileId;
	j["palette"] = sprite.palette;
	j["priority"] = sprite.priority != 0;
	j["flip_horizontal"] = sprite.flipHorizontal != 0;
	j["flip_vertical"] = sprite.flipVertical != 0;
}

static void to_json(nlohmann::json& j, const Metasprite& metasprite) {
	j["sprites"] = nlohmann::json::array();
	for (u32 i = 0; i < metasprite.spriteCount; ++i) {
		j["sprites"].push_back(metasprite.spritesRelativePos[i]);
	}
}

static void from_json(const nlohmann::json& j, Metasprite& metasprite) {
	metasprite.spriteCount = j.at("sprites").size();
	for (u32 i = 0; i < metasprite.spriteCount; ++i) {
		metasprite.spritesRelativePos[i] = j.at("sprites").at(i).get<Sprite>();
	}
}

#endif