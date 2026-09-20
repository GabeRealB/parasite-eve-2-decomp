#!/usr/bin/env python3
"""Real-compiler structured search on bundled fixtures or an archived permuter input.

An archive must contain preprocessed base.c and target.o. Copy its inputs into
a new scratch; never modify the archive or execute its old compile.sh.
"""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import sys

from smoke import ROOT, TOOLS, build, run
from evidence import digest, COMPILER
from experiments import Runner
from report_paths import public_data, public_path
from scratch import Attempt
from smart import search
from transforms import DEFAULT_OPERATORS


def check(output, case, budget=64, seed=0, archive=None):
    output.mkdir(parents=True, exist_ok=False)
    scratch = output / 'scratch'
    scratch.mkdir()
    if archive:
        for name in ('base.c', 'target.o'):
            shutil.copyfile(archive / name, scratch / name)
        run([sys.executable, ROOT / 'tools/claude-decomp-env/objdump.py', scratch / 'target.o'], ROOT,
            scratch / 'target_object_dump_normalized.s')
    else:
        for name in ('base', 'target'):
            (scratch / f'{name}.c').write_text(case[name])
        build(scratch / 'target.c')
    build(scratch / 'base.c')
    original = (scratch / 'base.c').read_bytes()
    command = f'exec {shlex.quote(sys.executable)} -B {shlex.quote(str(TOOLS / "tests/smoke.py"))} --build "$1"\n'
    (scratch / 'build.sh').write_text('#!/bin/sh\nset -eu\n' + command)
    baseline = Attempt(scratch, 'base', case['function'])
    runner = Runner(baseline, output / 'search', budget=budget)
    result = search(runner, operators=DEFAULT_OPERATORS, beam=4, fanout=16, depth=3, seed=seed)
    assert (scratch / 'base.c').read_bytes() == original
    result.update(case=case['name'], compiler_sha256=digest(COMPILER), target_object_sha256=digest(scratch / 'target.o'),
                  archive=public_path(archive) if archive else None)
    # Independently rebuild the exported result, without relying on a cached
    # score or the search's success flag.
    if (output / 'search/best.c').is_file():
        shutil.copyfile(output / 'search/best.c', scratch / 'verify.c')
        build(scratch / 'verify.c')
        verified = json.loads((scratch / 'verify.score.json').read_text())['distance']
        assert verified == result['best_distance']
        result['verified_distance'] = verified
        if not archive and verified == 0:
            for stem in ('target', 'verify'):
                run(['mips-linux-gnu-objcopy', '-O', 'binary', '--only-section=.text',
                     scratch / f'{stem}.o', scratch / f'{stem}.text.bin'], ROOT)
            result['text_bytes_identical'] = (scratch / 'target.text.bin').read_bytes() == (scratch / 'verify.text.bin').read_bytes()
            assert result['text_bytes_identical']
        best_row = next(row for row in runner.rows if public_path(row['source']) == result['best_candidate'])
        result['winning_history'] = [step['operator'] for step in best_row['history']]
    (output / 'result.json').write_text(json.dumps(public_data(result), indent=2) + '\n')
    print(f"{case['name']}: {result.get('baseline_distance')} -> {result.get('best_distance')}; {result['status']}; {result.get('used')} builds", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--function')
    parser.add_argument('--budget', type=int, default=64)
    parser.add_argument('--seed', type=int, default=0)
    args = parser.parse_args()
    output = args.output.resolve()
    if args.archive:
        if not args.function:
            parser.error('--archive needs --function')
        check(output, {'name': args.function, 'function': args.function}, args.budget, args.seed, args.archive.resolve())
        return 0
    output.mkdir(parents=True, exist_ok=False)
    cases = json.loads(Path(__file__).with_name('smart_checks.json').read_text())['cases']
    results = [check(output / case['name'], case, args.budget, args.seed) for case in cases]
    passed = all(r.get('verified_distance') == 0 and r.get('baseline_distance', 0) > 0 for r in results)
    (output / 'results.json').write_text(json.dumps(public_data({'passed': passed, 'cases': results}), indent=2) + '\n')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
