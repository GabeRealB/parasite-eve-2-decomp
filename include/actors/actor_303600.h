#ifndef INCLUDE_ACTORS_ACTOR_303600_H
#define INCLUDE_ACTORS_ACTOR_303600_H

#include "common.h"

#include "gameplay/animation.h"

#include "main/tmd_types.h"
#include "main/task_types.h"

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

/// Scene figure yaw/roll samples, one four-byte record per scene update.
///
/// Indexed in 0..699 by the Eve scene figure; its cursor starts at zero and
/// saturates at 699. Unsigned halfword angles use 4096 units per turn and narrow
/// into the figure's signed Euler vector. Pitch stays fixed at 1024. The Eve
/// package loads actor_303600 and borrows these samples through scene playback.
extern const Actor303600RotSample gActor303600SceneFigureRotationSamples[ACTOR_303600_ROT_SAMPLE_COUNT];

/// Compact camera transforms, one 24-byte key per scene update through frame 569.
///
/// Later updates hold key 569. Rotation is row-major Q12; translation widens
/// from signed halfwords as the negated world-space camera origin. Projection
/// distance is supplied separately. The Eve scene figure borrows this read-only
/// track while actor_303600 remains loaded alongside it.
extern const Actor303600ViewKey gActor303600SceneCameraKeys[ACTOR_303600_VIEW_KEY_COUNT];

extern AnimationSet gActor303600Animation075A0;

extern AnimationSet gActor303600Animation077F0;

extern AnimationSet gActor303600Animation07C30;

extern AnimationSet gActor303600Animation07E5C;

extern TmdSource gActor303600Model02DD0;

/// Shaft cutscene recipes: 0 scene controller, 1 white flash, 2 fade to white, 3 black cover.
///
/// All four are bodyless tasks at priority `TASK_PRIORITY_CONTROL`, with no terminator. Controller
/// and cover arguments are unused. Fade argument 1 supplies unsigned-halfword
/// intensity units per update, normally 4 or 8; argument 2 is unused. The Eve
/// package loads actor_303600 before spawning entry 0 after its death sequence.
/// Keep actor_303600 and the scene's borrowed resources loaded until teardown.
extern TaskDesc gActor303600CutsceneFadeTaskDescriptors[4];

/// Recipe indices for the scrolling shaft root and its repeating TMD segments.
enum {
    ACTOR_303600_SHAFT_ROOT_DESCRIPTOR    = 0,
    ACTOR_303600_SHAFT_SEGMENT_DESCRIPTOR = 1,
    ACTOR_303600_SHAFT_DESCRIPTOR_COUNT   = 2,
};

/// Scrolling-shaft recipes: a coordinate-body root and a TMD segment, both priority 192.
///
/// There is no terminator. The root ignores spawn arguments and owns its work
/// and up to five segments. A segment requires its live root task as parent,
/// borrows the root coordinate and joins its teardown tree. Eve loads this
/// package before spawning the root; keep it loaded until all tasks finish.
extern TaskDesc gActor303600ShaftTaskDescriptors[ACTOR_303600_SHAFT_DESCRIPTOR_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_303600_H
