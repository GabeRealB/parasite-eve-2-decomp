#ifndef MAIN_PAD_TYPES_H
#define MAIN_PAD_TYPES_H

#include "common.h"

/// Number of controller ports indexed by resident pad APIs (ports 0 and 1).
enum { PAD_PORT_COUNT = 2 };

enum {
    PAD_VIBRATION_INACTIVE       = 0,
    PAD_VIBRATION_ACTIVE         = 1,
    PAD_VIBRATION_MOTOR_BINARY   = 0,
    PAD_VIBRATION_MOTOR_VARIABLE = 1,
};

// Input format codes and wire-order stick indices used by PadState.
enum {
    PAD_INPUT_FORMAT_MOUSE        = 0x12,
    PAD_INPUT_FORMAT_DIGITAL      = 0x41,
    PAD_INPUT_FORMAT_ANALOG       = 0x73,
    PAD_INPUT_FORMAT_UNAVAILABLE  = 0xFF,
    PAD_STICK_RIGHT_X             = 0,
    PAD_STICK_RIGHT_Y             = 1,
    PAD_STICK_LEFT_X              = 2,
    PAD_STICK_LEFT_Y              = 3,
    PAD_STICK_AXIS_COUNT          = 4,
    PAD_STICK_FRACTION_BITS       = 12,
    PAD_STICK_FULL_SCALE          = 1 << PAD_STICK_FRACTION_BITS,
    PAD_STICK_DIRECTION_THRESHOLD = PAD_STICK_FULL_SCALE / 2,
};

/// Timed vibration contribution for one controller motor.
///
/// `PadState::vibrationRequests` holds eight requests per motor. The binary motor is on
/// when any active request has nonzero intensity; the variable motor uses the
/// greatest active intensity. The countdown advances on serviced controller
/// polls, normally once per VSync, and pauses while polling skips the controller.
/// A request still contributes on the poll that decrements its countdown to zero.
///
/// `Pad_PostEvent` doubles a signed halfword duration and stores the low halfword
/// without clamping; intensity is narrowed to a byte. Slots belong to the
/// resident controller state and can be replaced by later requests. Expiration
/// clears only `active`, so the other fields are meaningful only while active.
typedef struct {
    u8  active;         // Slot state (0 inactive, 1 active)
    u8  intensity;      // Motor drive (bank 0: 0 off, nonzero on; bank 1: 0..255)
    s16 pollsRemaining; // Serviced controller polls left; expires after decrement to zero
} PadVibrationRequest;
STATIC_ASSERT_SIZEOF(PadVibrationRequest, 0x4);

/// Resident input, controller setup and vibration state for one controller port.
///
/// `gPadStates` reserves two persistent entries. Controller setup, axes and
/// vibration advance during VSync polling; button edges and input blocking
/// advance during main-loop input updates. Both paths currently service port zero.
/// Direction repeat adds held D-pad bits to `pressedButtons` while a UI is open.
/// Stick axes use signed Q12, from -4096 to +4096, with negative left/up.
///
/// `actuatorCommand` is the two-byte buffer registered with libpad. Aligned
/// controllers use binary-motor on/off followed by variable-motor intensity;
/// legacy controllers use a 0x40 prefix followed by binary-motor on/off.
/// The buffer must remain alive while controller communication is running.
typedef struct {
    s16                 inputFormat;                        // Input format (0x12 mouse, 0x41 digital, 0x73 analog, 0xFF unavailable)
    u8                  nextVibrationSlot;                  // Next request slot (0..7), shared by both motor banks
    u8                  modeSetupPending;                   // Analog-mode setup (0 inspected/request accepted, 1 needs inspection/request)
    u16                 buttons;                            // Held buttons, active high; includes synthesized left-stick directions
    u16                 pressedButtons;                     // Newly pressed buttons plus UI direction repeat; consumers may clear bits
    u16                 releasedButtons;                    // Buttons released since the preceding input update
    volatile u8         inputBlockPolls;                    // Input updates left to suppress; expiry samples held buttons without edges
    u8                  directionRepeatTicks;               // Unchanged D-pad time in display ticks, with byte wrap and UI acceleration
    u8                  stickCenters[PAD_STICK_AXIS_COUNT]; // Raw axis centers in wire order; initialized to 128, clamped to 26..229
    PadVibrationRequest vibrationRequests[2][8];            // Timed contributions: binary motor bank, then variable motor bank
    s16                 stickAxes[PAD_STICK_AXIS_COUNT];    // Signed Q12 axes: right X/Y, left X/Y; zero inside the raw dead zone
    u8                  actuatorAlignmentReady;             // Actuator mapping (0 not established, 1 aligned); reset when disconnected/searching
    byte                unknown_59;                         // Cleared at initialization; role unproven, no other observed access
    u8                  actuatorCommand[2];                 // Persistent libpad command bytes; encoding depends on controller protocol
} PadState;
STATIC_ASSERT_SIZEOF(PadState, 0x5C);

#endif // MAIN_PAD_TYPES_H
