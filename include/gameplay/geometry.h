#ifndef GAMEPLAY_GEOMETRY_H
#define GAMEPLAY_GEOMETRY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// A signed 16.16 fixed-point value. One whole unit is 0x10000.
///
/// `word` is the full value. `halves.fraction` is the unsigned low half and
/// `halves.integer` the signed high half. Storing the fraction back into
/// `word` keeps that half and clears the integer.
typedef union {
    s32 word;         // Full value
    struct {
        u16 fraction; // Low half, in 1/65536 of a unit
        s16 integer;  // High half, in whole units
    } halves;
} Fixed16;
STATIC_ASSERT_SIZEOF(Fixed16, 4);

/// A world-space collision correction with fixed-point and SDK vector views.
///
/// Contact resolution writes the three `fixed` components in signed 16.16
/// game-coordinate units; it leaves the fourth word untouched. Callers can
/// keep the fractions or apply the signed integer halves to a coordinate.
/// The same storage is also reused through `vector` for whole-unit offsets
/// and directions whose unit length is 0x1000. The active operation determines
/// the scale; changing views performs no conversion.
///
/// A value may live on the ordinary stack or inside a scratch-stack block.
/// Scratch-backed pointers remain valid only until that block is released.
typedef union {
    VECTOR vector;  // SDK XYZ view; the SDK's fourth word is unused
    struct {
        Fixed16 vx; // X correction in signed 16.16 game-coordinate units
        Fixed16 vy; // Y correction in signed 16.16 game-coordinate units
        Fixed16 vz; // Z correction in signed 16.16 game-coordinate units
    } fixed;        // Fraction and signed integer halves of the collision correction
} WorldCollisionDelta;
STATIC_ASSERT_SIZEOF(WorldCollisionDelta, 0x10);

#endif // GAMEPLAY_GEOMETRY_H
