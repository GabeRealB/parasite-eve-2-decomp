#ifndef INCLUDE_ACTORS_ACTOR_303600_H
#define INCLUDE_ACTORS_ACTOR_303600_H

#include "common.h"

#include "gameplay/animation.h"

#include "main/tmd_types.h"

/// Commands carried by `ActorCommand::command` for the scrolling shaft.
///
/// Speeds are world-coordinate units per callback tick. The ramp stops only
/// after crossing its limit, retaining the resulting speed. Other commands
/// tear down the shaft and its child segments.
enum {
    ACTOR_303600_SHAFT_COMMAND_SCROLL_FORWARD = 0, // Start at +384, add +8 per tick toward +768.
    ACTOR_303600_SHAFT_COMMAND_REVERSE_SCROLL = 1, // Add -6 per tick to the current speed toward -768.
};

/// Lengths of the two sequences that drive the figure and the camera of the
/// scene `actor_403600` plays, in records.
///
/// Both are indexed by the frames the scene has run. The figure's turn has a
/// sample for every frame it changes on; the camera path is shorter, and the
/// camera rests on its last key for the remainder. A frame past the end of
/// either sequence takes that sequence's last record.
enum {
    ACTOR_303600_ROT_SAMPLE_COUNT = 700,
    ACTOR_303600_VIEW_KEY_COUNT   = 570,
};

/// One frame of the scene figure's turn: two of the Euler angles its rotation
/// is rebuilt from that frame, 4096 to a turn.
///
/// The angle about X is not sampled; the figure keeps one for the whole scene.
typedef struct {
    u16 z; // Angle about Z
    u16 y; // Angle about Y
} Actor303600RotSample;
STATIC_ASSERT_SIZEOF(Actor303600RotSample, 0x4);

/// One frame of the scene's camera path: the transform of a `ViewCamera` with
/// every component a halfword.
///
/// A key is widened into `ViewCamera::transform` to be applied, so its members
/// mean what that transform's do. The projection distance is not part of a
/// key.
typedef struct {
    s16 rotation[9];    // World-to-camera rotation, the nine coefficients in row-major order, 4096 = 1.0
    s16 translation[3]; // Negated world-space camera origin on X, Y and Z
} Actor303600ViewKey;
STATIC_ASSERT_SIZEOF(Actor303600ViewKey, 0x18);

extern AnimationSet gActor303600Animation075A0;

extern AnimationSet gActor303600Animation077F0;

extern AnimationSet gActor303600Animation07C30;

extern AnimationSet gActor303600Animation07E5C;

extern TmdSource gActor303600Model02DD0;

#endif // INCLUDE_ACTORS_ACTOR_303600_H
