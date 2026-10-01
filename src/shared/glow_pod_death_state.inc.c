/* Part of the glow pod library; see glow_pod.h. */

/// Dying-state tick of the second enemy, under the `Gp_StateF0.actorControl` mode byte: 1
/// does nothing and 2 hides the model. Otherwise the root's matrix is saved
/// into `field_264` and refolded with the decaying Y scale. Once `field_288` is
/// set the enemy is destroyed after 0x3D frames; before that, the kill
/// countdown running out releases state 0xF0, sets `field_288` and unlinks the
/// enemy's node and its three bodies, and the two animation slots are rebound
/// or advanced.
void glowPodDeathState(Enemy* arg0, Task* arg1)
{
    GlowPodWork* work;
    TmdObject*   obj;
    GfxCoord*    coord;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    switch (Gp_StateF0.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        glowPodFlatten(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    glowPodFlatten(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add(arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C);
        Gp_UnlinkObj(&work->field_FC);
        Gp_UnlinkObj(&work->field_184);
    }
    glowEnemy2TickAnim(arg1);
}
