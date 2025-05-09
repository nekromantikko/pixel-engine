#pragma once
#include <cstdint>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;
typedef int16 bool16;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

typedef int8 s8;
typedef int8 s08;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;
typedef bool16 b16;

typedef uint8 u8;
typedef uint8 u08;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 r32;
typedef real64 r64;

struct Actor;
struct ActorPrototype;
struct PersistedActorData;

typedef void (*ActorInitFn)(Actor*, const ActorPrototype*, const PersistedActorData*);
typedef void (*ActorUpdateFn)(Actor*, const ActorPrototype*);
typedef bool (*ActorDrawFn)(const Actor*, const ActorPrototype*);

typedef u16 TActorType;
typedef u16 TActorSubtype;