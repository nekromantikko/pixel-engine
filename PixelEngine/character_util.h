#pragma once
#include "rendering.h"

Rendering::Sprite characterSprites[8] = {
    { 0, 8, 0, 0b00000001 }, { 0, 16, 1, 0b00000001 },
    { 8, 8, 16, 0b00000001 }, { 8, 16, 17, 0b00000001 },
    { 16, 8, 128, 0b00000001 }, { 16, 16, 129, 0b00000001 },
    { 24, 8, 144, 0b00000001 }, { 24, 16, 145, 0b00000001 },
};