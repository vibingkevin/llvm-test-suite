#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Stage a cross-built test-suite, including data and empty Output directories."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tarfile
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--device", required=True)
parser.add_argument("--remote-root", required=True)
a = parser.parse_args()
build = a.build.resolve()
hdc = ["hdc", "-t", a.device]
remote = a.remote_root.rstrip("/")


def call(*args):
    r = subprocess.run(hdc + list(args), capture_output=True, check=True)
    if b"[Fail]" in r.stdout + r.stderr:
        raise RuntimeError((r.stdout + r.stderr).decode(errors="replace"))
    return r.stdout


def include(info):
    parts = Path(info.name).parts
    if "CMakeFiles" in parts or "Output" in parts or info.name.endswith((".o", ".cmake")):
        return None
    return info


with tempfile.TemporaryDirectory(prefix="ohos-stage-") as tmp:
    archive = Path(tmp) / "tests.tar"
    with tarfile.open(archive, "w", dereference=True) as tar:
        for name in ("SingleSource", "MultiSource"):
            if (build / name).is_dir():
                tar.add(build / name, arcname=name, filter=include)
        for parent in sorted({p.parent for p in build.rglob("*.test")}):
            info = tarfile.TarInfo(str(parent.relative_to(build) / "Output"))
            info.type = tarfile.DIRTYPE
            info.mode = 0o755
            tar.addfile(info)
    call("shell", "mkdir -p " + shlex.quote(remote))
    call("file", "send", str(archive), remote + "/.stage.tar")
    script = "tar -xf {0}/.stage.tar -C {0} && rm {0}/.stage.tar && printf __OHOS_STAGE_OK__".format(shlex.quote(remote))
    if not call("shell", script).endswith(b"__OHOS_STAGE_OK__"):
        raise RuntimeError("Device extraction failed")
print("Staged tests and data; no SDK runtime libraries were deployed.")
