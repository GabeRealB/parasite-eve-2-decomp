"""Loaded after gcc_trace_gdb.py inside GDB; audited i386 cc1 layout only.

Writes only an allocation-order vector or an equal-priority ready-list order.
Conflicts, RTL semantics, readiness and hazard checks remain in the compiler.
"""
APPLIED = False


def write_int(address, number):
    gdb.selected_inferior().write_memory(address & 0xffffffff, int(number).to_bytes(4, 'little', signed=True))


def global_order():
    global APPLIED
    if APPLIED or CONFIG['intervention']['kind'] != 'global_order':
        return
    spec = CONFIG['intervention']
    before, after = spec['before'], spec['after']
    maximum = value('max_regno')
    if not (76 <= before < maximum and 76 <= after < maximum):
        raise ValueError('intervention pseudo outside this function register range')
    a, b = array('reg_allocno', before), array('reg_allocno', after)
    if a < 0 or b < 0 or a == b:
        raise ValueError('intervention requires two distinct globally allocated allocnos')
    count = value('global.c:max_allocno')
    pointer = value('global.c:allocno_order')
    old = [integer(pointer + 4*i) for i in range(count)]
    if sorted(old) != list(range(count)):
        raise ValueError('unexpected allocation-order layout')
    new = list(old)
    if new.index(a) > new.index(b):
        new.remove(a)
        new.insert(new.index(b), a)
    if not CONFIG['control']:
        for i, number in enumerate(new):
            write_int(pointer + 4*i, number)
    emit('intervention', kind='global_order', control=CONFIG['control'], before=old, after=new,
         pseudos=[before, after], allocnos=[a, b], changed=old != new)
    APPLIED = True


def schedule_ready():
    global APPLIED
    if APPLIED or CONFIG['intervention']['kind'] != 'schedule_ready':
        return
    spec = CONFIG['intervention']
    phase = 'sched2' if value('reload_completed') else 'sched1'
    if phase != spec['pass']:
        return
    ready, count = arg(0), arg(1)
    if not (0 < count < 100000):
        raise ValueError('unexpected ready-list size')
    pointers = [integer(ready + 4*i) for i in range(count)]
    uids = [integer(p + 4) for p in pointers]
    if spec['uid'] not in uids:
        return
    at = uids.index(spec['uid'])
    priority = array('sched.c:insn_priority', uids[0])
    if any(array('sched.c:insn_priority', uid) != priority for uid in uids[:at+1]):
        return  # Do not cross priority groups or bypass readiness/hazard checks.
    new = [pointers[at]] + pointers[:at] + pointers[at+1:]
    if not CONFIG['control']:
        for i, pointer in enumerate(new):
            write_int(ready + 4*i, pointer)
    emit('intervention', kind='schedule_ready', control=CONFIG['control'], before=uids,
         after=[integer(p + 4) for p in new], changed=at != 0, **{'pass': phase})
    APPLIED = True


Entry('global.c:prune_preferences', global_order)
Entry('sched.c:schedule_select', schedule_ready)
