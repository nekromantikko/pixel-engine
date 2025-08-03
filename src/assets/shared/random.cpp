#include "random.h"
#include <random>
#include <gtc/constants.hpp>

static std::random_device rd;
static std::mt19937 gen32(rd());
static std::mt19937_64 gen64(rd());

u64 Random::GenerateUUID() {
    u64 result = UUID_NULL;
    while (result == UUID_NULL) {
        std::uniform_int_distribution<u64> dist;
        result = dist(gen64);
    }
    return result;
}

u32 Random::GenerateUUID32() {
    u32 result = UUID_NULL;
    while (result == UUID_NULL) {
        std::uniform_int_distribution<u32> dist;
        result = dist(gen32);
    }
    return result;
}

s32 Random::GenerateInt(s32 min, s32 max) {
    std::uniform_int_distribution<s32> dist(min, max);
    return dist(gen32);
}

r32 Random::GenerateReal(r32 min, r32 max) {
    std::uniform_real_distribution<r32> dist(min, max);
    return dist(gen32);
}

glm::vec2 Random::GenerateDirection() {
    r32 angle = GenerateReal(0.0f, glm::two_pi<r32>());
    return glm::vec2(glm::cos(angle), glm::sin(angle));
}