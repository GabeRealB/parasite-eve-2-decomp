"""Shared process-pool helpers for peassets extract tools.

Inside `shared_pool()` every `run_jobs` and `map_calls` submits to one pool, so phases run from threads of the
main process (extract.py's work graph) keep the same workers fed instead of each draining a pool of its own.
"""

from __future__ import annotations

import logging
import multiprocessing as mp
import os
from concurrent.futures import ProcessPoolExecutor, as_completed
from contextlib import contextmanager
from typing import Any, Callable, Iterator

_SHARED: ProcessPoolExecutor | None = None


def default_jobs(cap: int = 16) -> int:
    n = os.cpu_count() or 4
    return max(1, min(n, cap))


@contextmanager
def shared_pool(jobs: int | None = None) -> Iterator[ProcessPoolExecutor]:
    """One fork pool for everything run while it is open. Its workers are all forked on entry, while this process
    has a single thread: forking once phases run in threads could copy a lock some thread holds."""
    global _SHARED
    n_jobs = default_jobs() if jobs is None else max(1, jobs)
    with ProcessPoolExecutor(max_workers=n_jobs, mp_context=mp.get_context("fork")) as pool:
        pool.submit(int).result()                   # a fork pool starts all its workers at the first submit
        _SHARED = pool
        try:
            yield pool
        finally:
            _SHARED = None


def map_calls(fn: Callable[..., Any], calls: list[tuple]) -> list[Any]:
    """fn(*args) for each args in `calls`, on the shared pool where one is open (else here, in order); the results
    in call order, raising the first call's exception that failed."""
    if _SHARED is None:
        return [fn(*args) for args in calls]
    futures = [_SHARED.submit(fn, *args) for args in calls]
    return [f.result() for f in futures]


def run_jobs(
    worker: Callable[[dict[str, Any]], dict[str, Any]],
    work: list[dict[str, Any]],
    *,
    jobs: int | None = None,
    label_key: str = "stem",
) -> list[dict[str, Any]]:
    """Run *worker* over *work* with a process pool (or sequentially if jobs==1).

    Returns results in **completion order**. Caller should re-order if needed.
    Each *worker* input/output must be picklable (plain dicts preferred).
    """
    if not work:
        return []
    n_jobs = default_jobs() if jobs is None else max(1, jobs)
    if n_jobs == 1 or len(work) <= 1:
        return [worker(job) for job in work]

    if _SHARED is not None:
        return _collect(_SHARED, worker, work, label_key)
    with ProcessPoolExecutor(max_workers=n_jobs, mp_context=mp.get_context("fork")) as pool:
        return _collect(pool, worker, work, label_key)


def _collect(pool: ProcessPoolExecutor, worker, work, label_key: str) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    futures = {
        pool.submit(worker, job): job.get(label_key, str(i))
        for i, job in enumerate(work)
    }
    for fut in as_completed(futures):
        label = futures[fut]
        try:
            results.append(fut.result())
        except Exception as ex:
            logging.exception("FAIL %s (worker crash): %s", label, ex)
            results.append({"ok": False, label_key: label, "error": str(ex)})
    return results
