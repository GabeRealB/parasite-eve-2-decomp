/* Part of the player detection library; see player_detection.h. */

/// Rotates the slot-3 player's and this actor's raised root positions into
/// world space and returns `func_800E0308` on the pair.
s32 detectSightBlocked(Task* arg0)
{
    Task*              player;
    u8*                head;
    ActorSightScratch* s;
    SVECTOR*           local;
    SVECTOR*           v;
    SVECTOR*           out;

    player                   = gameGetPtrSlot(3);
    head                     = SCRATCH_STACK_CURSOR(u8);
    local                    = (SVECTOR*)(head - 0xC);
    s                        = (ActorSightScratch*)(head - 0x1C);
    s->local.vx              = player->extra.tmd->coords->coord.t[0];
    s->local.vy              = player->extra.tmd->coords->coord.t[1] - 1000;
    SCRATCH_STACK_CURSOR(u8) = (u8*)s;
    s->local.vz              = player->extra.tmd->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = arg0->extra.tmd->coords->coord.t[0];
    s->local.vy = arg0->extra.tmd->coords->coord.t[1] - 1000;
    s->local.vz = arg0->extra.tmd->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx += gGfxViewCoord.workm.t[0];
    s->from.vy += gGfxViewCoord.workm.t[1];
    s->from.vz += gGfxViewCoord.workm.t[2];
    s->hit      = func_800E0308(&s->out, out);
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
    return s->hit;
}
