/* Part of the No. 9 GOLEM library; see no9_golem.h. */

/// Aims the head coordinate (`coords[4]`) at the player. Takes the head into
/// view space, offsets the player position by 0x600 in Y, rotates that delta
/// into the body's frame, clamps it to +/-0x400 yaw, +/-0x300 pitch and a
/// minimum 0x200 forward, then builds the head rotation from it.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void no9GolemAimHead(Task* arg0)
{
    ActorAimScratch* scratch;
    GfxCoord*        coord;
    GfxCoord*        head;
    s32              offsetY;

    coord = arg0->extra.tmd->coords;
    head  = &coord[4];
    SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorAimScratch));
    scratch = SCRATCH_STACK_CURSOR(ActorAimScratch);

    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &head->workm, &scratch->view);
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - offsetY;
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - scratch->view.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->delta, &scratch->local);

    if (scratch->local.vx < -0x400) {
        scratch->local.vx = -0x400;
    } else if (scratch->local.vx > 0x400) {
        scratch->local.vx = 0x400;
    }
    if (scratch->local.vy < -0x300) {
        scratch->local.vy = -0x300;
    } else if (scratch->local.vy > 0x300) {
        scratch->local.vy = 0x300;
    }
    if (scratch->local.vz < 0x200) {
        scratch->local.vz = 0x200;
    }
    Gp_OrientAlong(&scratch->local, &head->coord, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorAimScratch));
}
