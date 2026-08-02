#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""launch_editor.py - UE Editor 启动烟雾测试。

启动 UnrealEditor.exe，等待初始化完成，然后关闭编辑器。
不预设错误规则——只判断编辑器是否正常启动，完整日志交给 AI 诊断。

Usage:
  python harness/scripts/launch_editor.py
Exit code:
  0 = 编辑器启动成功 / 1 = 启动失败 / 2 = SKIP (找不到引擎)
"""
from __future__ import annotations

import io
import json
import os
import re
import subprocess
import sys
import tempfile
import time

try:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
except Exception:
    pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
EDITOR_REL = os.path.join("Engine", "Binaries", "Win64", "UnrealEditor.exe")
LAUNCH_STATE_PATH = os.path.join(PROJECT_DIR, "Saved", "harness_last_launch.json")
LAUNCH_TIMEOUT_SECONDS = 300

_INIT_RE = re.compile(
    r"LogInit:\s*Display:\s*(Engine is initialized|Started initializing)",
    re.IGNORECASE)


def _write_state(success: bool, status: str, summary: str) -> None:
    state = {
        "status": status,
        "success": success,
        "ts": time.strftime("%Y-%m-%d %H:%M:%S"),
        "summary": summary,
    }
    state_dir = os.path.dirname(LAUNCH_STATE_PATH)
    os.makedirs(state_dir, exist_ok=True)
    fd, tmp = -1, ""
    try:
        fd, tmp = tempfile.mkstemp(
            prefix=".harness_last_launch.", suffix=".tmp", dir=state_dir)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
            fd = -1
            json.dump(state, f, ensure_ascii=False, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, LAUNCH_STATE_PATH)
        tmp = ""
    finally:
        if fd >= 0:
            os.close(fd)
        if tmp:
            try:
                os.unlink(tmp)
            except OSError:
                pass


def find_engine() -> str | None:
    from resolve_engine import resolve_engine
    return resolve_engine()


def find_uproject() -> str | None:
    from resolve_engine import find_uproject
    return find_uproject()


def _terminate(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return
    try:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
    except OSError:
        pass


def main() -> int:
    uproject = find_uproject()
    if not uproject:
        print(f"[ERROR] No .uproject found in {PROJECT_DIR}")
        _write_state(False, "failed", "No .uproject found")
        return 1

    engine = find_engine()
    if not engine:
        print("[SKIP] Engine root not found.")
        _write_state(False, "skipped", "Engine not found")
        return 2

    editor_exe = os.path.join(engine, EDITOR_REL)
    if not os.path.isfile(editor_exe):
        print(f"[ERROR] UnrealEditor.exe not found at {editor_exe}")
        _write_state(False, "failed", "UnrealEditor.exe not found")
        return 1

    _write_state(False, "starting", "Launch in progress")

    print(f"Engine:  {engine}")
    print(f"Project: {os.path.basename(uproject)}")
    print(f"Timeout: {LAUNCH_TIMEOUT_SECONDS}s\n")

    args = [
        editor_exe, uproject,
        "-log", "-stdout", "-FullStdOutLogOutput",
        "-unattended", "-NoSplash", "-nullrhi",
    ]
    print("Running: UnrealEditor " + " ".join(args[1:]) + "\n")

    init_detected = False
    deadline = time.monotonic() + LAUNCH_TIMEOUT_SECONDS
    process: subprocess.Popen | None = None

    try:
        with (tempfile.TemporaryFile(mode="w+b",
                                     prefix="workbuddy_launch_stdout_") as stdout_file,
              tempfile.TemporaryFile(mode="w+b",
                                     prefix="workbuddy_launch_stderr_") as stderr_file):
            process = subprocess.Popen(
                args, stdout=stdout_file, stderr=stderr_file,
                cwd=os.path.dirname(editor_exe),
            )

            while time.monotonic() < deadline and process.poll() is None:
                stdout_file.seek(0)
                try:
                    text = stdout_file.read().decode("utf-8", errors="replace")
                except Exception:
                    text = ""
                for line in text.split("\n"):
                    if _INIT_RE.search(line):
                        init_detected = True
                        print(f"[OK] {line.strip()[:120]}")
                        break
                if init_detected:
                    break
                time.sleep(0.5)

            if not init_detected and process.poll() is None:
                print(f"[TIMEOUT] Editor did not initialize within {LAUNCH_TIMEOUT_SECONDS}s")
    except Exception as exc:
        print(f"[ERROR] Launch exception: {exc}")
    finally:
        if process is not None:
            _terminate(process)

    # 保存完整日志供 AI 诊断
    log_path = os.path.join(PROJECT_DIR, "Saved", "harness_launch_last.log")
    full_log = ""
    if "stdout_file" in dir():
        try:
            stdout_file.seek(0)
            full_log = stdout_file.read().decode("utf-8", errors="replace")
        except Exception:
            pass
    if "stderr_file" in dir():
        try:
            stderr_file.seek(0)
            full_log += "\n" + stderr_file.read().decode("utf-8", errors="replace")
        except Exception:
            pass

    try:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(full_log)
    except OSError:
        log_path = ""

    print("-" * 60)
    if init_detected:
        print(f"[PASS] Editor initialized successfully.")
        print(f"Full log: {log_path}")
        _write_state(True, "success", "Editor initialized")
        return 0

    print(f"[FAIL] Editor did not start normally.")
    tail = full_log[-2000:] if full_log else "(no log)"
    print(f"Log tail:\n{tail}")
    print(f"\nFull log: {log_path}")
    _write_state(False, "failed", "Editor did not initialize")
    return 1


if __name__ == "__main__":
    sys.exit(main())
