#!/usr/bin/env python3
"""Build and validate the configured Qt/xmake project without extra packages."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xmake", default=shutil.which("xmake"), help="Path to xmake")
    parser.add_argument("--skip-build", action="store_true", help="Use already built targets")
    parser.add_argument("--clean", action="store_true", help="Clean all targets before building")
    parser.add_argument("--platform", default="offscreen", help="Qt platform plugin")
    args = parser.parse_args()
    if not args.xmake:
        parser.error("xmake is not on PATH; pass --xmake /path/to/xmake")
    if args.clean and args.skip_build:
        parser.error("--clean cannot be used with --skip-build")
    root = Path(__file__).resolve().parents[1]
    logs = root / "build" / "validation" / "latest"
    logs.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = args.platform
    # Discover checks from the build definition instead of maintaining a second list.
    targets = re.findall(r'^target\("([^"\n]+_test)"\)',
                         (root / "xmake.lua").read_text(encoding="utf-8"), re.MULTILINE)
    if not targets:
        parser.error("no test targets found in xmake.lua")
    results = []

    def run(name, arguments, timeout=120):
        with (logs / (name + ".log")).open("w", encoding="utf-8") as output:
            try:
                code = subprocess.run([args.xmake, *arguments], cwd=root, env=environment,
                                      stdout=output, stderr=subprocess.STDOUT,
                                      timeout=timeout).returncode
            except subprocess.TimeoutExpired:
                code = 124
            except OSError as error:
                output.write(str(error) + "\n")
                code = 127
        results.append((name, code))
        print(f"{name}: {'PASS' if code == 0 else 'FAIL (' + str(code) + ')'}", flush=True)
        (logs / "summary.txt").write_text(
            "".join(f"{target}: {status}\n" for target, status in results), encoding="utf-8")
        return code == 0

    if args.clean and not run("clean", ["clean", "-a"]):
        return 1
    if not args.skip_build and not run("build", ["build", "-a"], timeout=1800):
        return 1
    for target in targets:
        run(target, ["run", "--workdir=" + str(root), target])
    for version, fixture in (("v23", "tests/v23-project.afs.json"), ("v24", "tests/test.json")):
        run("startup-" + version,
            ["run", "--workdir=" + str(root), "AutoFlotationSheet",
             "--open-project", fixture, "--smoke-test"])
    print(f"Logs: {logs}", flush=True)
    return int(any(code != 0 for _, code in results))


if __name__ == "__main__":
    sys.exit(main())
