#ifndef MAIN_PRIVATE_PAD_TYPES_H
#define MAIN_PRIVATE_PAD_TYPES_H

#include "common.h"

#include "main/pad_types.h"

/// One controller port's libpad receive buffer, as registered with `PadInitDirect`.
///
/// libpad owns the two header bytes and copies the controller's response after
/// them whenever an exchange completes. A failed exchange rewrites the header
/// only, so the response keeps its previous contents; the game therefore sets
/// both button bytes to 0xFF (nothing held) itself while no usable controller
/// is attached. The response is laid out as the digital and analog controller
/// formats send it, the only ones the game decodes.
typedef struct {
    u8   status;                          // Last exchange (0 response received, 0xFF none); maintained by libpad, not read by the game
    u8   controllerId;                    // Controller ID byte: type in the high nibble, response halfword count in the low; 0 after a failure. Not read by the game
    u8   buttonsHigh;                     // Active low, bits 0..7: Select, L3, R3, Start, Up, Right, Down, Left; high byte of the game's button word
    u8   buttonsLow;                      // Active low, bits 0..7: L2, R2, L1, R1, Triangle, Circle, Cross, Square; low byte of the game's button word
    u8   stickAxes[PAD_STICK_AXIS_COUNT]; // Unsigned axes: right X/Y, left X/Y; meaningful only for an analog controller
    byte unknown_8[0x1C];                 // Longer responses and the rest of the 0x24 stride; not accessed by the game, division unproven
} PadRawPort;
STATIC_ASSERT_SIZEOF(PadRawPort, 0x24);

/// Active-low controller button word assembled from the two response bytes.
///
/// The receive buffer sends the high byte first. The byte view places it after
/// the low byte in little-endian memory so `word` uses Psy-Q's button bit order.
typedef union {
    u16 word;    // Active-low button bits in Psy-Q order
    struct {
        u8 low;  // Shoulder and face buttons, from `PadRawPort::buttonsLow`
        u8 high; // Select, stick clicks, Start and D-pad, from `PadRawPort::buttonsHigh`
    } bytes;
} PadRawButtons;
STATIC_ASSERT_SIZEOF(PadRawButtons, 0x2);

#endif // MAIN_PRIVATE_PAD_TYPES_H
