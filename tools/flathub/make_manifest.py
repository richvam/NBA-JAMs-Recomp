#!/usr/bin/env python3
"""make_manifest.py - the manifest to submit to Flathub, from the one CI builds.

    python3 tools/flathub/make_manifest.py <tag> [out.yml]

flatpak/io.github.richvam.NBAJamRecomp.yml builds from the checked-out folder (`type: dir`)
and turns off the AppStream compose step. Flathub builds from a fixed release instead, and
runs compose itself. This writes the same manifest with:
  * the program's source as `type: git` at <tag>, with the tag's commit (Flathub wants both)
  * `appstream-compose: false` removed
  * the version written into the build, since a Flathub build has no git history to read
<tag> must exist (git fetch --tags); the commit is looked up with git rev-parse.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "flatpak", "io.github.richvam.NBAJamRecomp.yml")
REPO_URL = "https://github.com/richvam/NBA-JAMs-Recomp.git"


def make(tag, commit, text):
    out = re.sub(r"(?m)^# The metadata is checked.*\n(^#.*\n)*appstream-compose: false\n", "", text)
    if "appstream-compose" in out:
        raise SystemExit("could not remove appstream-compose")
    dir_src = re.compile(r"(?m)^      - type: dir\n        path: \.\.\n        skip:\n(          - .*\n)+")
    if not dir_src.search(out):
        raise SystemExit("could not find the dir source")
    out = dir_src.sub("      - type: git\n        url: %s\n        tag: %s\n        commit: %s\n" % (REPO_URL, tag, commit), out)
    version = tag[1:] if tag.startswith("v") else tag
    out = out.replace('APP_VERSION="$(cat .flatpak-version 2>/dev/null || echo dev)"', "APP_VERSION=%s" % version)
    if "APP_VERSION=%s" % version not in out:
        raise SystemExit("could not set the version")
    return out


def self_test():
    text = open(SRC, encoding="utf-8").read()
    out = make("v1.2.3", "0" * 40, text)
    ok = "type: git" in out and "tag: v1.2.3" in out and "commit: " + "0" * 40 in out and "APP_VERSION=1.2.3" in out
    ok = ok and "type: dir" not in out and "appstream-compose" not in out and "sha256: " in out
    print("self-test:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--self-test":
        return self_test()
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    tag = sys.argv[1]
    commit = subprocess.run(["git", "-C", ROOT, "rev-parse", tag + "^{commit}"], capture_output=True, text=True, check=True).stdout.strip()
    out = make(tag, commit, open(SRC, encoding="utf-8").read())
    path = sys.argv[2] if len(sys.argv) > 2 else "io.github.richvam.NBAJamRecomp.yml"
    open(path, "w", encoding="utf-8", newline="\n").write(out)
    print("wrote", path, "for", tag, commit)
    return 0


if __name__ == "__main__":
    sys.exit(main())
