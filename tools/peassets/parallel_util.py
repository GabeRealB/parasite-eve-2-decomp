"""Shared process-pool helpers for peassets extract tools."""

from __future__ import annotations

import logging
import os
from concurrent.futures import ProcessPoolExecutor, as_completed
from typing import Any, Callable


def default_jobs(cap: int = 16) -> int:
    n = os.cpu_count() or 4
    return max(1, min(n, cap))


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

    results: list[dict[str, Any]] = []
    try:
        import multiprocessing as mp

        ctx = mp.get_context("fork")
    except ValueError:
        ctx = None

    with ProcessPoolExecutor(max_workers=n_jobs, mp_context=ctx) as pool:
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
