import filecmp
import logging
import os
import subprocess
from pathlib import Path
from typing import Optional

import pytest


@pytest.fixture
def packer_path(request) -> Optional[Path]:
    val = request.config.getoption("--path-to-packer")
    return Path(val) if val else None


def run_packer(packer_bin: Path, mode: str, src: Path, dst: Path, cwd: Path):
    cmd = [str(packer_bin), mode, str(src), str(dst)]

    exc: subprocess.CalledProcessError | None = None
    try:
        logging.info(f"Running command: {' '.join(cmd)} in {cwd}")
        subprocess.run(cmd, cwd=cwd, check=True)
        return
    except subprocess.CalledProcessError as e:
        exc = e
    # If none succeeded, raise the last exception (with stderr if available)
    if exc is not None:
        msg = f"Invocation failed. Command: {exc.cmd}\n"
        if exc.stdout:
            msg += "stdout:\n" + exc.stdout.decode(errors="ignore") + "\n"
        if exc.stderr:
            msg += "stderr:\n" + exc.stderr.decode(errors="ignore") + "\n"
        raise RuntimeError(msg)
    raise RuntimeError("No commands were provided to try_run()")


def assert_dirs_equal(expected: Path, actual: Path):
    # Ensure that:
    # - every item in expected exists in actual and contents match
    # - no extra items exist in actual that are not in expected
    # - types of entries match (file/dir/symlink)
    # - targets of symlinks match
    # - subdirectories are compared recursively
    # raise one AssertionError with all differences found and paths relative to expected/actual
    if not expected.exists() or not expected.is_dir():
        raise AssertionError(f"Expected directory missing or not a dir: {expected}")
    if not actual.exists() or not actual.is_dir():
        raise AssertionError(f"Actual directory missing or not a dir: {actual}")

    diffs: list[str] = []

    def compare_dirs(rel: Path):
        exp_p = expected / rel
        act_p = actual / rel

        exp_entries = sorted(p.name for p in exp_p.iterdir())
        act_entries = sorted(p.name for p in act_p.iterdir())

        exp_set, act_set = set(exp_entries), set(act_entries)
        for name in sorted(act_set - exp_set):
            diffs.append(f"{rel / name}: unexpected in actual")

        for name in sorted(exp_entries):
            child_rel = rel / name
            exp_child = exp_p / name
            act_child = act_p / name

            if not act_child.exists(follow_symlinks=False):
                diffs.append(f"{child_rel}: missing in actual")
                continue

            if exp_child.is_symlink():
                if not act_child.is_symlink():
                    diffs.append(f"{child_rel}: type mismatch (symlink vs non-symlink)")
                    continue

                e_target = os.readlink(exp_child)
                a_target = os.readlink(act_child)
                if e_target != a_target:
                    diffs.append(
                        f"{child_rel}: symlink target mismatch: expected '{e_target}', actual '{a_target}'"
                    )
                continue

            if exp_child.is_dir():
                if not act_child.is_dir():
                    diffs.append(f"{child_rel}: type mismatch (dir vs non-dir)")
                else:
                    compare_dirs(child_rel)
                continue

            # regular file
            if not act_child.is_file():
                diffs.append(f"{child_rel}: type mismatch (file vs non-file)")
                continue
            if not filecmp.cmp(exp_child, act_child, shallow=False):
                diffs.append(f"{child_rel}: file contents differ")

    compare_dirs(Path("."))

    if diffs:
        msg = "Directory trees differ.\n"
        msg += f"Expected: {expected}\n"
        msg += f"Actual:   {actual}\n"
        msg += "Differences:\n- "
        msg += "\n- ".join(diffs)
        raise AssertionError(msg)


def test_pack_and_unpack_roundtrip(packer_path: Path, tmp_path: Path):
    repo_root = Path(__file__).resolve().parents[2]

    assert packer_path.exists(), f"Packer binary not found at: {packer_path}"
    assert packer_path.is_file(), f"Packer path is not a file: {packer_path}"
    # not strictly required but helpful
    if not (packer_path.stat().st_mode & 0o111):
        raise AssertionError(f"Packer binary is not executable: {packer_path}")

    input_dir = repo_root / "tests" / "data"
    assert input_dir.exists() and input_dir.is_dir(), (
        f"Test data directory not found: {input_dir}"
    )

    # Pack
    packed_file = tmp_path / "archive.pak"
    run_packer(packer_path, "pack", input_dir, packed_file, cwd=repo_root)
    assert packed_file.exists(), "Packer did not produce output archive"

    unpack_dir = tmp_path / "unpacked"
    unpack_dir.mkdir(parents=True, exist_ok=True)
    # Unpack
    run_packer(packer_path, "unpack", packed_file, unpack_dir, cwd=repo_root)

    # Compare unpacked tree to original input_dir
    assert_dirs_equal(input_dir, unpack_dir)
