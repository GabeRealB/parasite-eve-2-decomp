/* Part of the No. 9 GOLEM library; see no9_golem.h. */

/// Scratch-stack block of the No. 9 GOLEM's head turned to aim at the player.
///
/// The aim reserves one block a tick, works out where the player stands from
/// the head in the frame of the model's root, limits that offset and builds
/// the head's rotation along it; the block is released before the aim
/// returns and nothing reads it afterwards.
typedef struct {
    MATRIX headWorld; // The head part's transform taken out of the view: its rotation and position along the world's axes. Only the translation is read
    VECTOR toTarget;  // Offset from the head to the point 0x600 above the player's root (toward negative Y), along the world's axes; `pad` is never written
    VECTOR aim;       // `toTarget` turned into the frame of the model's root, then limited to [-0x400, 0x400] on X, [-0x300, 0x300] on Y and at least 0x200 on Z: the direction the head is turned along
} _No9GolemHeadAimScratch;
STATIC_ASSERT_SIZEOF(_No9GolemHeadAimScratch, 0x40);

/// Aims the head coordinate (`coords[4]`) at the player. Takes the head's
/// transform out of the view, onto the world's axes, measures the offset from
/// it to the point 0x600 above the player's root, rotates that offset into the
/// root's frame, clamps it to +/-0x400 on X, +/-0x300 on Y and a minimum 0x200
/// forward, then builds the head rotation along it.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void no9GolemAimHead(Task* arg0)
{
    _No9GolemHeadAimScratch* scratch;
    GfxCoord*                coord;
    GfxCoord*                head;
    s32                      offsetY;

    coord = arg0->extra.tmd->coords;
    head  = &coord[4];
    SCRATCH_STACK_RESERVE_BYTES(sizeof(_No9GolemHeadAimScratch));
    scratch = SCRATCH_STACK_CURSOR(_No9GolemHeadAimScratch);

    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &head->workm, &scratch->headWorld);
    scratch->toTarget.vx = gPlayerStatus.coordMtx->t[0] - scratch->headWorld.t[0];
    offsetY              = scratch->headWorld.t[1] + 0x600;
    scratch->toTarget.vy = gPlayerStatus.coordMtx->t[1] - offsetY;
    scratch->toTarget.vz = gPlayerStatus.coordMtx->t[2] - scratch->headWorld.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->toTarget, &scratch->aim);

    if (scratch->aim.vx < -0x400) {
        scratch->aim.vx = -0x400;
    } else if (scratch->aim.vx > 0x400) {
        scratch->aim.vx = 0x400;
    }
    if (scratch->aim.vy < -0x300) {
        scratch->aim.vy = -0x300;
    } else if (scratch->aim.vy > 0x300) {
        scratch->aim.vy = 0x300;
    }
    if (scratch->aim.vz < 0x200) {
        scratch->aim.vz = 0x200;
    }
    gfxBuildDirectionRotation(&scratch->aim, &head->coord, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_No9GolemHeadAimScratch));
}
