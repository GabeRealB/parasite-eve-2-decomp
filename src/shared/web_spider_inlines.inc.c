/* Part of the library; see web_spider.h. Inline helpers the fragments use. */

/// Switches the work's animation id, resetting the slots to the blend value the
/// table gives for the new id; otherwise ticks every slot one frame.
static inline void spiderTickAnimInline(Task* task)
{
    Actor105500Work* work;
    s32              i;
    s32              value;

    work = task->work;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = gSpiderAnimBlend[work->field_392];
        for (i = 1; i < 8; i++) {
            func_800B4114(&work->anim, i, work->field_392, 0, value);
        }
    } else {
        work->field_396++;
        for (i = 1; i < 8; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}
