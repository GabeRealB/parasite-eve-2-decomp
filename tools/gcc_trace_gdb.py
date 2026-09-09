"""GDB observer loaded by trace_gcc.py, pinned to the bundled i386 cc1.

Only reads inferior memory. Entry breakpoints precede the prologue, so cdecl
arguments are at esp+4. The wrapper verifies compiler identity and compares
observed output with an ordinary compile. No calls into the inferior are made.
"""
import gdb
import json

SYMS = CONFIG['symbols']
OUTPUT = open(CONFIG['events'], 'w')
BLOCK = None
GLOBAL_SEEN = False
ACTIVE = False
COMPARISON = None


def emit(event, **values):
    OUTPUT.write(json.dumps(dict(event=event, **values), sort_keys=True) + '\n')
    OUTPUT.flush()


def memory(address, size):
    return bytes(gdb.selected_inferior().read_memory(address & 0xffffffff, size))


def integer(address, size=4, signed=True):
    return int.from_bytes(memory(address, size), 'little', signed=signed)


def value(name):
    return integer(SYMS[name])


def array(name, index, size=4):
    return integer(value(name) + index * size, size)


def arg(index):
    return integer(int(gdb.parse_and_eval('$esp')) + 4 * (index + 1))


def bits(name, index):
    # HARD_REG_SET has 76 bits, stored in three 32-bit host words.
    data = int.from_bytes(memory(value(name) + index * 12, 12), 'little')
    return [r for r in range(76) if data & (1 << r)]


def priority(refs, span, size):
    return int(((refs.bit_length() - 1) * refs / span) * 10000 * size) if refs > 0 and span else None


def local_priority(refs, span, size):
    # QTY_CMP_PRI multiplies size before division, unlike allocno_compare.
    return int(((refs.bit_length() - 1) * refs * size / span) * 10000) if refs > 0 and span else None


def dispositions():
    return [array('reg_renumber', i, 2) for i in range(value('max_regno'))]


def string(address):
    return gdb.Value(address & 0xffffffff).cast(gdb.lookup_type('char').pointer()).string()


def rtl(pointer, depth=4):
    if not pointer:
        return None
    code = integer(pointer, 2, False)
    name = string(integer(SYMS['rtx_name'] + code * 4))
    mode = integer(pointer + 2, 1, False)
    row = {'code': name, 'mode': string(integer(SYMS['mode_name'] + mode * 4))}
    if name == 'reg':
        row['reg'] = integer(pointer + 4)
    elif name == 'const_int':
        row['value'] = integer(pointer + 4)
    elif name == 'symbol_ref':
        row['symbol'] = string(integer(pointer + 4))
    elif depth:
        fmt = string(integer(SYMS['rtx_format'] + code * 4))
        row['operands'] = [rtl(integer(pointer + 4 + i * 4), depth-1)
                           for i, kind in enumerate(fmt) if kind == 'e']
    if name == 'mem':
        flags = integer(pointer + 3, 1, False)
        row.update(in_struct=bool(flags & 16), volatile=bool(flags & 8))
    return row


class Finished(gdb.FinishBreakpoint):
    def __init__(self, callback):
        super().__init__(gdb.newest_frame(), internal=True)
        self.callback = callback

    def stop(self):
        try:
            self.callback(int(gdb.parse_and_eval('(int)$eax')))
        except Exception as error:
            emit('error', message=str(error))
            return True
        return False


class Entry(gdb.Breakpoint):
    def __init__(self, symbol, callback, always=False):
        super().__init__('*' + hex(SYMS[symbol]), internal=True)
        self.callback, self.always = callback, always

    def stop(self):
        if ACTIVE or self.always:
            try:
                self.callback()
            except Exception as error:
                emit('error', message=str(error))
                return True
        return False


def begin_function():
    global ACTIVE, GLOBAL_SEEN
    name = string(value('current_function_name'))
    ACTIVE = not CONFIG['function'] or name == CONFIG['function']
    GLOBAL_SEEN = False
    if ACTIVE:
        emit('function', name=name)


def begin_block():
    global BLOCK
    BLOCK = arg(0)


def local_choice():
    q = arg(2)
    prefix = 'local-alloc.c:'
    members, reg = [], array(prefix + 'qty_first_reg', q)
    while reg >= 0:
        if reg in members:
            raise ValueError('cyclic quantity membership')
        members.append(reg)
        reg = array(prefix + 'reg_next_in_qty', reg)
    refs, birth, death, size = [array(prefix + 'qty_' + key, q)
                               for key in ('n_refs', 'birth', 'death', 'size')]
    row = dict(block=BLOCK, qty=q, members=members, refs=refs, birth=birth, death=death,
               span=death-birth, size=size, priority=local_priority(refs, death-birth, size),
               copy_suggestions=bits(prefix + 'qty_phys_copy_sugg', q),
               suggestions=bits(prefix + 'qty_phys_sugg', q),
               suggested_only=arg(4), caller_save=arg(3), reg_class=arg(0),
               searched_birth=arg(5), searched_death=arg(6))
    Finished(lambda result: emit('local_choice', **row, result=result))


def global_choice():
    global GLOBAL_SEEN
    prefix = 'global.c:'
    if not GLOBAL_SEEN:
        emit('global_order', allocnos=[array(prefix + 'allocno_order', i)
                                      for i in range(value(prefix + 'max_allocno'))])
        GLOBAL_SEEN = True
    a = arg(0)
    reg = array(prefix + 'allocno_reg', a)
    members = [r for r in range(76, value('max_regno')) if array('reg_allocno', r) == a]
    refs, span, size, calls = [array(prefix + 'allocno_' + key, a)
                              for key in ('n_refs', 'live_length', 'size', 'calls_crossed')]
    row = dict(allocno=a, reg=reg, members=members, refs=refs, span=span, size=size,
               calls=calls, priority=priority(refs, span, size), alternate=arg(2),
               caller_save=arg(3), retry=arg(4),
               hard_conflicts=bits(prefix + 'hard_reg_conflicts', a),
               copy_preferences=bits(prefix + 'hard_reg_copy_preferences', a),
               preferences=bits(prefix + 'hard_reg_preferences', a))
    Finished(lambda result: emit('global_choice', **row, result=array('reg_renumber', reg, 2)))


def reload_begin():
    before = dispositions()
    emit('before_reload', dispositions=before)
    Finished(lambda result: emit('after_reload', dispositions=dispositions()))


def reload_order():
    def finished(result):
        address = SYMS['reload1.c:potential_reload_regs']
        emit('reload_order', order=[integer(address + 2*i, 2) for i in range(76)])
    Finished(finished)


def spill():
    before, reg, forced = dispositions(), arg(0), arg(3)
    def finished(result):
        after = dispositions()
        emit('spill', reg=reg, forced=forced, changes=[(i, b, a) for i, (b, a)
                                                     in enumerate(zip(before, after)) if b != a])
    Finished(finished)


def schedule_compare():
    global COMPARISON
    def insn(pointer):
        uid = integer(pointer + 4)
        return dict(uid=uid, priority=array('sched.c:insn_priority', uid),
                    luid=array('sched.c:insn_luid', uid))
    x, y = insn(integer(arg(0))), insn(integer(arg(1)))
    if x['uid'] not in CONFIG['uids'] and y['uid'] not in CONFIG['uids']:
        return
    last = value('sched.c:last_scheduled_insn')
    row = {'x': x, 'y': y, 'last': integer(last + 4) if last else None,
           'pass': 'sched2' if value('reload_completed') else 'sched1'}
    # LOG_LINKS(last) points at INSN_LIST nodes: fld[0]=insn, fld[1]=next.
    links = {}
    link = integer(last + 24) if last else 0
    while link:
        links[integer(integer(link + 4) + 4)] = integer(link + 2, 1, False)
        link = integer(link + 8)
    row['dependency_kinds'] = links
    row['costs'] = {}
    COMPARISON = row
    def finished(result):
        global COMPARISON
        reason = 'priority'
        if x['priority'] == y['priority']:
            classes = {}
            for insn in (x, y):
                uid = insn['uid']
                classes[uid] = 3 if uid not in links or row['costs'].get(uid) == 1 else (1 if links[uid] == 0 else 2)
            row['dependency_classes'] = classes
            reason = 'dependency class' if classes[x['uid']] != classes[y['uid']] else 'original RTL order'
        emit('schedule_compare', **row, result=result, reason=reason,
             winner=x['uid'] if result < 0 else y['uid'])
        COMPARISON = None
    Finished(finished)


def postreload_set():
    set_rtx, uid = arg(0), integer(arg(1) + 4)
    if CONFIG['uids'] and uid not in CONFIG['uids']:
        return
    before = rtl(integer(set_rtx + 8))
    def finished(result):
        if result:
            emit('postreload_set', uid=uid, before=before,
                 after=rtl(integer(set_rtx + 8)),
                 note='proposed substitution; confirm retained change in sched2 dump')
    Finished(finished)


def schedule_cost():
    if COMPARISON is not None:
        row, uid = COMPARISON, integer(arg(0) + 4)
        Finished(lambda result: row['costs'].__setitem__(uid, result))


Entry('local_alloc', begin_function, always=True)
Entry('local-alloc.c:block_alloc', begin_block)
Entry('local-alloc.c:find_free_reg', local_choice)
Entry('global.c:find_reg', global_choice)
Entry('reload', reload_begin)
Entry('reload1.c:order_regs_for_reload', reload_order)
Entry('reload1.c:spill_hard_reg', spill)
Entry('reload1.c:reload_cse_simplify_set', postreload_set)
# Sched1 precedes local_alloc, so activate function filtering at its entry too.
Entry('schedule_insns', begin_function, always=True)
if CONFIG['uids']:
    Entry('sched.c:rank_for_schedule', schedule_compare)
    Entry('sched.c:insn_cost', schedule_cost)
gdb.events.exited.connect(lambda event: emit('exit', code=getattr(event, 'exit_code', None)))
