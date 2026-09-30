/* Part of the model placement library; see model_placement.h. */

/// Tick state of the child in the second state table: copies the spawner's
/// model flag bits 0x80 (hidden) and 0x4 (draw buffers allocated) onto this
/// task's model, rebuilding the buffers through `Tmd_AllocBuffers` when the
/// spawner's are gone.
void modelPlacementMirrorParent(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = ((Task*)task->spawnArg2.pointer)->extra.tmd;
    object       = task->extra.tmd;

    if (!(parentObject->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        object->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObject->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        object->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
}
