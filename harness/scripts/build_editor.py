#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_editor.py - UE 项目 Editor target 编译检查（C++ 修改后必须 0 error）。

Drives the real compile chain: UnrealBuildTool.exe -> cl.exe -> link -> Editor target.
Parses UBT/compiler output, extracts error lines, and reports a compact summary so the
AI can read what failed instead of a log flood.

Usage:
  python harness/scripts/build_editor.py
Exit code:
  0 = PASS (0 errors) / 1 = FAIL (compile errors) / 2 = SKIP (no engine found)

Design (mirrors HET check_build_compile.ps1):
  - call UnrealBuildTool.exe directly (NOT Build.bat) with -FromMsBuild so the
    compiler's native errors are emitted
  - atomically invalidate the previous success state before launching UBT
  - put the Windows UBT tree in a KILL_ON_JOB_CLOSE Job Object
  - capture output to delete-on-close temp files (NOT a pipe)
  - keep the whole build lifecycle inside the WorkBuddy outer timeout
  - read back as UTF-8 with errors='replace'
  - detect 4 failure classes: UBT failure / C++ compile error / MSBuild MSB error / UHT error
  - report only the matched error lines (<=10), full raw log kept on disk
Note: engine root resolved via resolve_engine.py（环境变量 > .uproject EngineAssociation/注册表）。
"""
from __future__ import annotations

import io
import os
import re
import signal
import subprocess
import sys
import tempfile
import time

from source_fingerprint import (
    SourceFingerprintError,
    fingerprint_matches,
    fingerprint_project_sources,
)

# Force UTF-8 stdout/stderr so Chinese paths/messages don't garble on Windows (GBK).
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
MAX_ERROR_LINES = 10
BUILD_STATE_PATH = os.path.join(PROJECT_DIR, "Saved", "harness_last_build.json")
BUILD_TOTAL_BUDGET_SECONDS = 1800
WORKBUDDY_POST_TOOL_TIMEOUT_SECONDS = 1860
BUILD_REPORT_RESERVE_SECONDS = (
    WORKBUDDY_POST_TOOL_TIMEOUT_SECONDS - BUILD_TOTAL_BUDGET_SECONDS)


def _write_build_state(success: bool, exit_code: int, errors: list,
                       source_fingerprint: dict | None = None,
                       *, status: str) -> None:
    """Record the last build result so other guards (pre_commit / Stop) can enforce it.
    The atomic replace prevents readers from observing partial JSON."""
    import json as _json
    state = {
        "status": status,
        "success": bool(success),
        "exit_code": int(exit_code),
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
        "errors": [str(e)[:200] for e in (errors or [])][:MAX_ERROR_LINES],
    }
    if source_fingerprint is not None:
        state["source_fingerprint"] = source_fingerprint
    state_dir = os.path.dirname(BUILD_STATE_PATH)
    os.makedirs(state_dir, exist_ok=True)
    file_descriptor = -1
    temporary_path = ""
    try:
        file_descriptor, temporary_path = tempfile.mkstemp(
            prefix=".harness_last_build.", suffix=".tmp",
            dir=state_dir)
        with os.fdopen(
                file_descriptor, "w", encoding="utf-8", newline="\n") as stream:
            file_descriptor = -1
            _json.dump(state, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, BUILD_STATE_PATH)
        temporary_path = ""
    finally:
        if file_descriptor >= 0:
            os.close(file_descriptor)
        if temporary_path:
            try:
                os.unlink(temporary_path)
            except OSError:
                pass


def _record_failure_state(exit_code: int, errors: list,
                          *, status: str = "failed") -> bool:
    try:
        _write_build_state(
            False, exit_code, errors, status=status)
        return True
    except OSError as exc:
        print(f"[ERROR] Cannot record non-success build state: {exc}")
        return False


def _remaining_seconds(deadline_monotonic: float) -> float:
    return deadline_monotonic - time.monotonic()


def find_engine() -> str | None:
    """从环境变量或 .uproject EngineAssociation 推断引擎路径。"""
    from resolve_engine import resolve_engine
    return resolve_engine()


def find_uproject() -> str | None:
    from resolve_engine import find_uproject as _find_uproject
    return _find_uproject()


def find_uprojects() -> list[str]:
    from resolve_engine import find_uprojects as _find_uprojects
    return _find_uprojects()


def _create_windows_kill_job() -> int:
    """创建关闭句柄即终止全部成员进程的 Windows Job Object。"""
    import ctypes
    from ctypes import wintypes

    class _BasicLimitInformation(ctypes.Structure):
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

    class _IoCounters(ctypes.Structure):
        _fields_ = [
            ("ReadOperationCount", ctypes.c_ulonglong),
            ("WriteOperationCount", ctypes.c_ulonglong),
            ("OtherOperationCount", ctypes.c_ulonglong),
            ("ReadTransferCount", ctypes.c_ulonglong),
            ("WriteTransferCount", ctypes.c_ulonglong),
            ("OtherTransferCount", ctypes.c_ulonglong),
        ]

    class _ExtendedLimitInformation(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", _BasicLimitInformation),
            ("IoInfo", _IoCounters),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.SetInformationJobObject.argtypes = [
        wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    kernel32.SetInformationJobObject.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        raise OSError(
            ctypes.get_last_error(), "CreateJobObjectW failed")
    info = _ExtendedLimitInformation()
    info.BasicLimitInformation.LimitFlags = 0x00002000
    if not kernel32.SetInformationJobObject(
            job, 9, ctypes.byref(info), ctypes.sizeof(info)):
        error = ctypes.get_last_error()
        kernel32.CloseHandle(job)
        raise OSError(
            error, "SetInformationJobObject(KILL_ON_JOB_CLOSE) failed")
    return int(job)


def _assign_windows_job(job_handle: int,
                        process: subprocess.Popen) -> None:
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.AssignProcessToJobObject.argtypes = [
        wintypes.HANDLE, wintypes.HANDLE]
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    if not kernel32.AssignProcessToJobObject(
            wintypes.HANDLE(job_handle),
            wintypes.HANDLE(int(process._handle))):
        raise OSError(
            ctypes.get_last_error(), "AssignProcessToJobObject failed")


def _close_windows_handle(handle: int) -> None:
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    if not kernel32.CloseHandle(wintypes.HANDLE(handle)):
        raise OSError(ctypes.get_last_error(), "CloseHandle failed")


def _terminate_process_tree(
        process: subprocess.Popen, job_handle: int | None) -> None:
    """终止受管进程树；Windows 优先关闭 KILL_ON_JOB_CLOSE job。"""
    if job_handle is not None:
        try:
            _close_windows_handle(job_handle)
        except OSError:
            pass
    if os.name == "nt" and process.poll() is None:
        try:
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                capture_output=True, timeout=30, check=False)
        except (OSError, subprocess.SubprocessError):
            pass
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except OSError:
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


def _run_managed_process(args: list[str], *, stdout, stderr,
                         cwd: str | None,
                         timeout: float) -> int:
    """运行一个受进程树托管的命令；父进程退出时 Windows job 自动清理子树。"""
    job_handle: int | None = None
    process: subprocess.Popen | None = None
    try:
        popen_kwargs = {
            "stdout": stdout,
            "stderr": stderr,
            "cwd": cwd,
        }
        if os.name == "nt":
            job_handle = _create_windows_kill_job()
            popen_kwargs["creationflags"] = (
                subprocess.CREATE_NEW_PROCESS_GROUP)
        else:
            popen_kwargs["start_new_session"] = True
        process = subprocess.Popen(args, **popen_kwargs)
        if job_handle is not None:
            _assign_windows_job(job_handle, process)
        return process.wait(timeout=max(0.001, timeout))
    except BaseException:
        if process is not None and process.poll() is None:
            _terminate_process_tree(process, job_handle)
            job_handle = None
        raise
    finally:
        if job_handle is not None:
            _close_windows_handle(job_handle)


def _find_cl_exe(engine: str, deadline_monotonic: float) -> str | None:
    """Locate cl.exe via vswhere (VS2022+) under the detected VS installation."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    candidates = []
    if os.path.isfile(vswhere):
        try:
            remaining = _remaining_seconds(deadline_monotonic)
            if remaining <= 0:
                return None
            out = subprocess.run([vswhere, "-latest", "-products", "*",
                                  "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                  "-property", "installationPath"],
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace",
                                 timeout=min(30, remaining))
            vs_path = (out.stdout or "").strip().splitlines()
            if vs_path:
                msvc_root = os.path.join(vs_path[0], "VC", "Tools", "MSVC")
                if os.path.isdir(msvc_root):
                    for ver in sorted(os.listdir(msvc_root), reverse=True):
                        cl = os.path.join(msvc_root, ver, "bin", "Hostx64", "x64", "cl.exe")
                        if os.path.isfile(cl):
                            candidates.append(cl)
        except Exception:
            pass
    return candidates[0] if candidates else None


# Match cl error lines like: path\file.cpp(9,42): error C2065: ...
_CL_ERROR_RE = re.compile(r"^\s*\S+\.(?:cpp|cc|cxx|h|hpp|inl)\(\d+(?:,\d+)?\)\s*:\s*(?:error|fatal error)",
                          re.IGNORECASE)
# Match UBT line that carries the failing .rsp path: cl.exe @".../X.cpp.obj.rsp"
_RSP_RE = re.compile(r'cl\.exe\s+@"([^"]+\.rsp)"', re.IGNORECASE)


def _extract_cl_errors(
        ubt_output: str, engine: str, max_lines: int,
        deadline_monotonic: float) -> list[str]:
    """After a failed UBT build, re-run cl.exe on the failing .rsp to recover real
    compiler diagnostics (file(line,col): error Cxxxx), which UBT/UBA otherwise swallows."""
    cl = _find_cl_exe(engine, deadline_monotonic)
    if not cl:
        return []
    rsps = _RSP_RE.findall(ubt_output)
    seen, errors = set(), []
    for rsp in rsps:
        remaining = _remaining_seconds(deadline_monotonic)
        if remaining <= 0:
            break
        if rsp in seen or not os.path.isfile(rsp):
            continue
        seen.add(rsp)
        try:
            proc = subprocess.run([cl, f"@{rsp}"], cwd=os.path.join(engine, "Engine", "Source"),
                                  capture_output=True,
                                  timeout=min(120, remaining))
            raw = proc.stdout + proc.stderr
        except Exception:
            continue
        # cl.exe (MSVC) prints in the system ANSI codepage (GBK on zh-CN), not UTF-8.
        # Try GBK first so Chinese diagnostics don't garble; fall back to UTF-8.
        if isinstance(raw, bytes):
            try:
                text = raw.decode("gbk", errors="replace")
            except Exception:
                text = raw.decode("utf-8", errors="replace")
        else:
            text = raw
        for line in text.split("\n"):
            if _CL_ERROR_RE.search(line):
                errors.append(line.strip())
                if len(errors) >= max_lines:
                    return errors
    return errors


def main() -> int:
    deadline_monotonic = (
        time.monotonic() + BUILD_TOTAL_BUDGET_SECONDS)
    try:
        _write_build_state(
            False, -1, ["Build initialization in progress"],
            status="starting")
    except OSError as exc:
        print(
            "[ERROR] Cannot invalidate the previous successful build state "
            f"before starting: {exc}")
        return 1

    projects = find_uprojects()
    if not projects:
        message = f"No .uproject found in {PROJECT_DIR}"
        print(f"[ERROR] {message}")
        _record_failure_state(1, [message])
        return 1
    if len(projects) > 1:
        names = [os.path.basename(path) for path in projects]
        message = f"Multiple .uproject files found: {names}"
        print(f"[ERROR] {message}")
        _record_failure_state(1, [message])
        return 1

    uproject = projects[0]
    project_name = os.path.splitext(os.path.basename(uproject))[0]
    target = f"{project_name}Editor"

    engine = find_engine()
    if not engine:
        message = "Engine root not found (set ENGINE_ROOT/UE_ENGINE_ROOT or fix EngineAssociation)"
        print(f"[SKIP] {message}.")
        print("[SKIP] Cannot compile without engine - no coverage.")
        _record_failure_state(2, [message], status="skipped")
        return 2
    ubt = os.path.join(engine, UBT_REL)

    try:
        source_fingerprint_before = fingerprint_project_sources(
            PROJECT_DIR,
            deadline_monotonic=deadline_monotonic)
    except SourceFingerprintError as exc:
        message = f"Cannot fingerprint UE compile inputs before build: {exc}"
        print(f"[ERROR] {message}")
        _record_failure_state(1, [message])
        return 1

    try:
        _write_build_state(
            False, -1, ["Compilation in progress"],
            source_fingerprint_before, status="running")
    except OSError as exc:
        message = (
            "Cannot record running build state before launching UBT: "
            f"{exc}")
        print(f"[ERROR] {message}")
        _record_failure_state(1, [message])
        return 1

    print(f"Engine:  {engine}")
    print(f"Target:  {target} {PLATFORM} {CONFIG}")
    print(f"Project: {os.path.basename(uproject)}")
    print(
        "Inputs:  "
        f"{source_fingerprint_before['file_count']} files, "
        f"{source_fingerprint_before['digest']}")
    print(
        "Budget:  "
        f"{BUILD_TOTAL_BUDGET_SECONDS}s internal + "
        f"{BUILD_REPORT_RESERVE_SECONDS}s client report reserve")
    print()

    args = [ubt, target, PLATFORM, CONFIG,
            f"-Project={uproject}", "-WaitMutex", "-FromMsBuild", "-architecture=x64"]
    print("Running: UnrealBuildTool " + " ".join(os.path.basename(a) if i == 0 else a
                                                for i, a in enumerate(args)))
    print()

    # Anonymous temporary files are removed by the OS when handles close, including
    # cancellation; this avoids persistent per-run temp directories.
    try:
        remaining = _remaining_seconds(deadline_monotonic)
        if remaining <= 0:
            raise subprocess.TimeoutExpired(
                args, BUILD_TOTAL_BUDGET_SECONDS)
        with (tempfile.TemporaryFile(
                mode="w+b", prefix="workbuddy_compile_stdout_") as stdout_file,
              tempfile.TemporaryFile(
                mode="w+b", prefix="workbuddy_compile_stderr_") as stderr_file):
            exit_code = _run_managed_process(
                args, stdout=stdout_file, stderr=stderr_file,
                cwd=None, timeout=remaining)
            stdout_file.seek(0)
            stderr_file.seek(0)
            stdout_raw = stdout_file.read().decode(
                "utf-8", errors="replace")
            stderr_raw = stderr_file.read().decode(
                "utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        message = (
            "Compile time budget exhausted "
            f"({BUILD_TOTAL_BUDGET_SECONDS}s total)")
        print(f"[ERROR] {message}.")
        _record_failure_state(124, [message], status="timeout")
        return 1
    except (FileNotFoundError, OSError) as e:
        message = f"Failed to run or read UBT output: {e}"
        print(f"[ERROR] {message}")
        _record_failure_state(1, [message])
        return 1
    all_output = stdout_raw + "\n" + stderr_raw

    # Keep the full raw log for reference.
    log_path = os.path.join(PROJECT_DIR, "Saved", "harness_build_last.log")
    try:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(all_output)
    except OSError:
        log_path = None

    # Detect 4 failure classes (exit code is authoritative but UBT can pass rc yet fail).
    failed_result = re.search(r"Result:\s*Failed\s*\(", all_output)
    comp_error = re.search(r"OtherCompilationError|CompilationErrorException", all_output)
    msb_error = re.search(r"MSB\d+\s*:\s*[Ee]rror", all_output)
    uht_error = re.search(r"Error\[", all_output)
    warning_lines = len(re.findall(r"(?<![A-Z])\b[Ww]arning\b", all_output))
    build_failed = bool(exit_code != 0 or failed_result or comp_error or msb_error or uht_error)
    freshness_error = ""
    source_fingerprint_after: dict | None = None
    if not build_failed:
        try:
            source_fingerprint_after = fingerprint_project_sources(
                PROJECT_DIR,
                deadline_monotonic=deadline_monotonic)
        except SourceFingerprintError as exc:
            freshness_error = (
                f"Cannot fingerprint UE compile inputs after build: {exc}")
        else:
            if not fingerprint_matches(
                    source_fingerprint_before, source_fingerprint_after):
                freshness_error = (
                    "UE compile inputs changed while the build was running; "
                    "this build cannot prove the current source state")
        if freshness_error:
            build_failed = True

    print("-" * 50)
    if build_failed:
        print(f"Exit Code: {exit_code} (FAILED)")
        if freshness_error:
            print(f"Freshness: {freshness_error}")
        if failed_result:
            print("UBT:       Result: Failed")
        if comp_error:
            print("UBT:       CompilationError detected")
        if msb_error:
            print("MSBuild:   MSB error detected")
        if uht_error:
            print("UHT:       Error detected")

        # Recover real compiler diagnostics by re-running cl on the failing .rsp
        # (UBT/UBA swallows cl output; this is what Rider/the editor shows).
        cl_errors = (
            []
            if freshness_error
            else _extract_cl_errors(
                all_output, engine, MAX_ERROR_LINES,
                deadline_monotonic)
        )

        print("\nError details:")
        shown = 0
        for ln in cl_errors:
            print(f"  {ln[:200]}")
            shown += 1
        if cl_errors:
            print("  (compiler diagnostics recovered via cl re-run on failing .rsp)")

        # Fallback: UBT-level error lines (when cl re-run finds nothing).
        if not cl_errors:
            err_re = re.compile(
                r"Result:\s*Failed|MSB\d+|OtherCompilationError|error\s+\w+\d+|"
                r"Build FAILED|EXEC.*[Ee]rror|Error\[|error C\d+|fatal error",
                re.IGNORECASE)
            error_lines = [ln.strip() for ln in all_output.split("\n") if err_re.search(ln)]
            for ln in error_lines:
                if shown >= MAX_ERROR_LINES:
                    print("  ... (truncated)")
                    break
                print(f"  {ln[:200]}")
                shown += 1
    else:
        print("Exit Code: 0 (OK)")
        print(f"Warnings: {warning_lines}")

    print("-" * 50)
    if log_path:
        print(f"Full log: {log_path}")

    if build_failed:
        effective_exit_code = exit_code if exit_code != 0 else 1
        print(f"[FAIL] Compilation/freshness validation failed "
              f"(exit code {effective_exit_code}).")
        failure_errors = cl_errors or ([freshness_error] if freshness_error else [])
        _record_failure_state(
            effective_exit_code, failure_errors)
        return 1
    if warning_lines > 0:
        print(f"[WARN] Compilation succeeded but {warning_lines} warning(s).")
        try:
            _write_build_state(
                True, 0, [], source_fingerprint_after,
                status="success")
        except OSError as exc:
            print(
                "[ERROR] Compilation succeeded but the success state "
                f"could not be recorded atomically: {exc}")
            return 1
        return 0
    print("[PASS] Compilation succeeded. 0 errors, 0 warnings.")
    try:
        _write_build_state(
            True, 0, [], source_fingerprint_after,
            status="success")
    except OSError as exc:
        print(
            "[ERROR] Compilation succeeded but the success state "
            f"could not be recorded atomically: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
