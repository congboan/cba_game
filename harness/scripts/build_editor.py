#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_editor.py - UE 项目 Editor target 编译检查。

Drives UnrealBuildTool.exe -> cl.exe -> link -> Editor target.
不预设错误类型——只判断编译是否通过并验证源码新鲜度，完整日志交给 AI 诊断。

Usage:
  python harness/scripts/build_editor.py
Exit code:
  0 = PASS / 1 = FAIL / 2 = SKIP (no engine found)
"""
from __future__ import annotations

import io
import json
import os
import subprocess
import sys
import tempfile
import time

from source_fingerprint import (
    SourceFingerprintError,
    fingerprint_matches,
    fingerprint_project_sources,
)

try:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
except Exception:
    pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
PLATFORM = "Win64"
CONFIG = "Development"
UBT_REL = os.path.join("Engine", "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.exe")
BUILD_STATE_PATH = os.path.join(PROJECT_DIR, "Saved", "harness_last_build.json")
BUILD_TOTAL_BUDGET_SECONDS = 1800
WORKBUDDY_POST_TOOL_TIMEOUT_SECONDS = 1860
BUILD_REPORT_RESERVE_SECONDS = (
    WORKBUDDY_POST_TOOL_TIMEOUT_SECONDS - BUILD_TOTAL_BUDGET_SECONDS)


def _write_build_state(success: bool, exit_code: int,
                       source_fingerprint: dict | None = None,
                       *, status: str) -> None:
    state = {
        "status": status,
        "success": bool(success),
        "exit_code": int(exit_code),
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    if source_fingerprint is not None:
        state["source_fingerprint"] = source_fingerprint
    state_dir = os.path.dirname(BUILD_STATE_PATH)
    os.makedirs(state_dir, exist_ok=True)
    fd, tmp = -1, ""
    try:
        fd, tmp = tempfile.mkstemp(
            prefix=".harness_last_build.", suffix=".tmp", dir=state_dir)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
            fd = -1
            json.dump(state, f, ensure_ascii=False, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, BUILD_STATE_PATH)
        tmp = ""
    finally:
        if fd >= 0:
            os.close(fd)
        if tmp:
            try:
                os.unlink(tmp)
            except OSError:
                pass


def _record_failure_state(exit_code: int, *, status: str = "failed") -> bool:
    try:
        _write_build_state(False, exit_code, status=status)
        return True
    except OSError as exc:
        print(f"[ERROR] Cannot record build state: {exc}")
        return False


def _remaining_seconds(deadline_monotonic: float) -> float:
    return deadline_monotonic - time.monotonic()


def find_engine() -> str | None:
    from resolve_engine import resolve_engine
    return resolve_engine(allow_env=True)


def find_uproject() -> str | None:
    from resolve_engine import find_uproject as _find_uproject
    return _find_uproject()


def find_uprojects() -> list[str]:
    from resolve_engine import find_uprojects as _find_uprojects
    return _find_uprojects()


# ── Process management ──────────────────────────────────────────────

def _create_windows_kill_job():
    import ctypes
    from ctypes import wintypes

    class _Basic(ctypes.Structure):
        _fields_ = [
            ("PerProcessUserTimeLimit", ctypes.c_longlong),
            ("PerJobUserTimeLimit", ctypes.c_longlong),
            ("LimitFlags", wintypes.DWORD),
            ("MinimumWorkingSetSize", ctypes.c_size_t),
            ("MaximumWorkingSetSize", ctypes.c_size_t),
            ("ActiveProcessLimit", wintypes.DWORD),
            ("Affinity", ctypes.c_size_t),
            ("PriorityClass", wintypes.DWORD),
            ("SchedulingClass", wintypes.DWORD),
        ]

    class _Io(ctypes.Structure):
        _fields_ = [
            ("ReadOperationCount", ctypes.c_ulonglong),
            ("WriteOperationCount", ctypes.c_ulonglong),
            ("OtherOperationCount", ctypes.c_ulonglong),
            ("ReadTransferCount", ctypes.c_ulonglong),
            ("WriteTransferCount", ctypes.c_ulonglong),
            ("OtherTransferCount", ctypes.c_ulonglong),
        ]

    class _Extended(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", _Basic),
            ("IoInfo", _Io),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t),
        ]

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    k32.CreateJobObjectW.restype = wintypes.HANDLE
    k32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    k32.SetInformationJobObject.restype = wintypes.BOOL
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    k32.CloseHandle.restype = wintypes.BOOL

    job = k32.CreateJobObjectW(None, None)
    if not job:
        raise OSError(ctypes.get_last_error(), "CreateJobObjectW failed")
    info = _Extended()
    info.BasicLimitInformation.LimitFlags = 0x00002000
    if not k32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
        k32.CloseHandle(job)
        raise OSError(ctypes.get_last_error(), "SetInformationJobObject failed")
    return int(job)


def _assign_job(job_handle: int, process: subprocess.Popen) -> None:
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    k32.AssignProcessToJobObject.restype = wintypes.BOOL
    if not k32.AssignProcessToJobObject(wintypes.HANDLE(job_handle), wintypes.HANDLE(int(process._handle))):
        raise OSError(ctypes.get_last_error(), "AssignProcessToJobObject failed")


def _close_handle(handle: int) -> None:
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    k32.CloseHandle.restype = wintypes.BOOL
    k32.CloseHandle(wintypes.HANDLE(handle))


def _terminate_tree(process: subprocess.Popen, job_handle: int | None) -> None:
    if job_handle is not None:
        try:
            _close_handle(job_handle)
        except OSError:
            pass
    if os.name == "nt" and process.poll() is None:
        try:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           capture_output=True, timeout=30, check=False)
        except (OSError, subprocess.SubprocessError):
            pass
    if process.poll() is None:
        try:
            process.kill()
        except OSError:
            pass
    try:
        process.wait(timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        pass


def _run_managed(args: list[str], *, stdout, stderr,
                 cwd: str | None, timeout: float) -> int:
    job_handle: int | None = None
    process: subprocess.Popen | None = None
    try:
        popen_kwargs = {"stdout": stdout, "stderr": stderr, "cwd": cwd}
        if os.name == "nt":
            job_handle = _create_windows_kill_job()
            popen_kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
        process = subprocess.Popen(args, **popen_kwargs)
        if job_handle is not None:
            _assign_job(job_handle, process)
        return process.wait(timeout=max(0.001, timeout))
    except BaseException:
        if process is not None and process.poll() is None:
            _terminate_tree(process, job_handle)
            job_handle = None
        raise
    finally:
        if job_handle is not None:
            _close_handle(job_handle)


# ── Main ─────────────────────────────────────────────────────────────

def main() -> int:
    deadline = time.monotonic() + BUILD_TOTAL_BUDGET_SECONDS

    try:
        _write_build_state(False, -1, status="starting")
    except OSError as exc:
        print(f"[ERROR] Cannot invalidate previous build state: {exc}")
        return 1

    projects = find_uprojects()
    if not projects:
        print(f"[ERROR] No .uproject found in {PROJECT_DIR}")
        _record_failure_state(1)
        return 1
    if len(projects) > 1:
        names = [os.path.basename(p) for p in projects]
        print(f"[ERROR] Multiple .uproject files: {names}")
        _record_failure_state(1)
        return 1

    uproject = projects[0]
    project_name = os.path.splitext(os.path.basename(uproject))[0]
    target = f"{project_name}Editor"

    engine = find_engine()
    if not engine:
        msg = "Engine root not found (set ENGINE_ROOT/UE_ENGINE_ROOT or fix EngineAssociation)"
        print(f"[SKIP] {msg}")
        _record_failure_state(2, status="skipped")
        return 2

    ubt = os.path.join(engine, UBT_REL)

    try:
        fp_before = fingerprint_project_sources(PROJECT_DIR, deadline_monotonic=deadline)
    except SourceFingerprintError as exc:
        print(f"[ERROR] Cannot fingerprint sources: {exc}")
        _record_failure_state(1)
        return 1

    try:
        _write_build_state(False, -1, source_fingerprint=fp_before, status="running")
    except OSError as exc:
        print(f"[ERROR] Cannot record running state: {exc}")
        _record_failure_state(1)
        return 1

    print(f"Engine:  {engine}")
    print(f"Target:  {target} {PLATFORM} {CONFIG}")
    print(f"Project: {os.path.basename(uproject)}")
    print(f"Inputs:  {fp_before['file_count']} files, {fp_before['digest']}")
    print(f"Budget:  {BUILD_TOTAL_BUDGET_SECONDS}s internal + {BUILD_REPORT_RESERVE_SECONDS}s reserve\n")

    args = [ubt, target, PLATFORM, CONFIG,
            f"-Project={uproject}", "-WaitMutex", "-NoHotReload", "-architecture=x64"]
    print("Running: UnrealBuildTool " + " ".join(
        os.path.basename(a) if i == 0 else a for i, a in enumerate(args)))
    print()

    try:
        remaining = _remaining_seconds(deadline)
        if remaining <= 0:
            raise subprocess.TimeoutExpired(args, BUILD_TOTAL_BUDGET_SECONDS)
        with (tempfile.TemporaryFile(mode="w+b", prefix="wbuddy_build_stdout_") as stdout_file,
              tempfile.TemporaryFile(mode="w+b", prefix="wbuddy_build_stderr_") as stderr_file):
            exit_code = _run_managed(args, stdout=stdout_file, stderr=stderr_file,
                                     cwd=None, timeout=remaining)
            stdout_file.seek(0)
            stderr_file.seek(0)
            stdout_raw = stdout_file.read().decode("utf-8", errors="replace")
            stderr_raw = stderr_file.read().decode("utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        print(f"[ERROR] Compile time budget exhausted ({BUILD_TOTAL_BUDGET_SECONDS}s)")
        _record_failure_state(124, status="timeout")
        return 1
    except (FileNotFoundError, OSError) as e:
        print(f"[ERROR] Failed to run UBT: {e}")
        _record_failure_state(1)
        return 1

    all_output = stdout_raw + "\n" + stderr_raw
    build_failed = bool(exit_code != 0)

    # 保存完整日志
    log_path = os.path.join(PROJECT_DIR, "Saved", "harness_build_last.log")
    try:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(all_output)
    except OSError:
        log_path = ""

    # 源码新鲜度验证
    freshness_error = ""
    fp_after: dict | None = None
    if not build_failed:
        try:
            fp_after = fingerprint_project_sources(PROJECT_DIR, deadline_monotonic=deadline)
        except SourceFingerprintError as exc:
            freshness_error = f"Cannot fingerprint after build: {exc}"
        else:
            if not fingerprint_matches(fp_before, fp_after):
                freshness_error = "Source changed during build; result cannot prove current state"
        if freshness_error:
            build_failed = True

    print("-" * 60)
    print(f"UBT Exit Code: {exit_code}")
    if freshness_error:
        print(f"Freshness:     {freshness_error}")

    if build_failed:
        print(f"[FAIL] Build failed.")
        print(f"Full log: {log_path}" if log_path else "(log not saved)")
        _record_failure_state(exit_code if exit_code != 0 else 1)
        return 1

    print("[PASS] Build succeeded.")
    print(f"Full log: {log_path}" if log_path else "")
    try:
        _write_build_state(True, 0, source_fingerprint=fp_after, status="success")
    except OSError as exc:
        print(f"[ERROR] Cannot record success state: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
