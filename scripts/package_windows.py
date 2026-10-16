#!/usr/bin/env python3
"""Bundle a native UCRT64 build, verify its DLL closure, and publish its ZIP."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import urllib.request
import urllib.parse
import zipfile

source = Path(sys.argv[1]).resolve()
bin_dir = Path(sys.argv[2]).resolve()
tag = sys.argv[3]
if not re.fullmatch(r"v\d+\.\d+\.\d+(?:[-.][\w.-]+)?", tag):
    raise SystemExit("Invalid version tag")
if source.joinpath("VERSION").read_text().strip() != tag[1:]:
    raise SystemExit("Source version differs from the release tag")
name = f"OpenChess-{tag}-Windows-x64"
dist = source / "dist"
stage = dist / name
if stage.exists():
    shutil.rmtree(stage)
stage.mkdir(parents=True)
shutil.copy2(source / "openchess.exe", stage / "openchess.exe")
shutil.copytree(source / "assets", stage / "assets")
for file in ("LICENSE", "ATTRIBUTIONS.md", "chess.conf.example"):
    shutil.copy2(source / file, stage / file)

dlls = {p.name.lower(): p for p in bin_dir.glob("*.dll")}
p2p_prefix = source / ".cache/p2p"
custom_dlls = {p.name.lower(): p for p in (p2p_prefix / "bin").glob("*.dll")}
dlls.update(custom_dlls)
system = Path(os.environ["SystemRoot"]) / "System32"
pending = [stage / "openchess.exe"]
# SDL codecs may be opened dynamically and absent from the PE import table.
pending += [p for key, p in dlls.items() if re.search(
    r"mpg123|vorbis|opus|flac|png|jpeg|webp|tiff", key)]
seen = set()
copied = set()
packages = set()
while pending:
    binary = pending.pop()
    if binary.name.lower() in seen:
        continue
    seen.add(binary.name.lower())
    if binary.suffix.lower() == ".dll":
        shutil.copy2(binary, stage / binary.name)
        copied.add(binary.name)
        if binary.name.lower() in custom_dlls:
            owner = None
        else:
            owner = True
        posix_path = subprocess.check_output(["cygpath", "-u", str(binary)], text=True).strip()
        if owner:
            owner = subprocess.run(["pacman", "-Qqo", posix_path], capture_output=True,
                                   text=True, check=True).stdout.strip()
            packages.add(owner)
    imports = subprocess.run(["objdump", "-p", str(binary)], capture_output=True,
                             text=True, check=True).stdout
    for dep in re.findall(r"DLL Name:\s*(\S+)", imports):
        key = dep.lower()
        if key == "msys-2.0.dll":
            raise SystemExit("Release must not depend on the MSYS runtime")
        if key in dlls:
            pending.append(dlls[key])
        elif (system / dep).exists() or key.startswith(("api-ms-", "ext-ms-")):
            continue
        else:
            raise SystemExit(f"Missing runtime dependency: {dep}")

if (source / "src/net_p2p.c").exists() and not any("datachannel" in name.lower() for name in copied):
    raise SystemExit("P2P runtime is missing from the release package")

licenses = stage / "licenses"
if (p2p_prefix / "share/licenses").is_dir():
    shutil.copytree(p2p_prefix / "share/licenses", licenses, dirs_exist_ok=True)
manifest = []
for package in sorted(packages):
    manifest.append(subprocess.run(["pacman", "-Q", package], capture_output=True,
                                  text=True, check=True).stdout.strip())
    files = subprocess.run(["pacman", "-Qlq", package], capture_output=True,
                           text=True, check=True).stdout.splitlines()
    for file in files:
        if "/share/licenses/" not in file:
            continue
        native_file = Path(subprocess.check_output(["cygpath", "-m", file], text=True).strip())
        if native_file.is_file():
            dest = licenses / package / file.split("/share/licenses/", 1)[1]
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(native_file, dest)
(stage / "DEPENDENCIES.txt").write_text(
    "MSYS2 UCRT64 runtime packages (sources and recipes: https://github.com/msys2/MINGW-packages):\n"
    + "\n".join(manifest) + "\n", encoding="utf-8")
(stage / "BUILD.txt").write_text(
    "Source ref: " + os.environ["RELEASE_SOURCE_REF"] + "\n",
    encoding="utf-8")
(stage / "OpenChess.bat").write_bytes(
    b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nset "PATH=%~dp0;%PATH%"\r\nstart "" "%~dp0openchess.exe"\r\n')
(stage / "README-Windows.txt").write_text(
    f"OpenChess {tag} - Windows 10/11 x64 portable build\n\n"
    "Extract the entire ZIP, then run OpenChess.bat (or openchess.exe).\n"
    "Keep assets and DLL files beside the executable. MSYS2 is not required.\n\n"
    "OpenChess detects existing engines first. When none is found it installs\n"
    "official Stockfish automatically under %APPDATA%\\openchess\\engines.\n"
    "Internet access is needed for the first installation. Settings > Engine\n"
    "shows status and allows retrying. You can also select a custom engine or\n"
    "place stockfish.exe beside openchess.exe.\n\n"
    "Settings and exported games: %APPDATA%\\openchess\\\n",
    encoding="utf-8")
# Exercise the packaged executable with all build-tool directories removed.
env = os.environ.copy()
env.pop("OPENCHESS_ASSETS", None)
env["PATH"] = str(stage) + ";" + str(system) + ";" + str(system.parent)
version = subprocess.run([str(stage / "openchess.exe"), "--version"], cwd=stage,
                         env=env, capture_output=True, text=True, timeout=20, check=True)
if version.stdout.strip() != "OpenChess " + tag[1:]:
    raise SystemExit("Packaged executable version check failed: " + version.stdout)
app = subprocess.Popen([str(stage / "openchess.exe")], cwd=stage, env=env)
try:
    import time
    time.sleep(4)
    if app.poll() is not None:
        raise SystemExit(f"Packaged GUI failed to stay running: {app.returncode}")
finally:
    if app.poll() is None:
        app.terminate()
        app.wait(timeout=10)

archive = dist / (name + ".zip")
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as out:
    for file in sorted(stage.rglob("*")):
        if file.is_file():
            out.write(file, file.relative_to(dist))
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
print(f"Verified portable EXE with {len(copied)} DLLs; ZIP SHA256: {digest}", flush=True)
if os.environ.get("OPENCHESS_PACKAGE_ONLY") == "1":
    print("ARCHIVE=" + str(archive), flush=True)
    sys.exit(0)
repo = os.environ["GITHUB_REPOSITORY"]
token = os.environ["GH_TOKEN"]
def api(method, url, data=None, kind="application/json"):
    request = urllib.request.Request(url, data=data, method=method, headers={
        "Authorization": "Bearer " + token, "User-Agent": "OpenChess-Windows-release",
        "Accept": "application/vnd.github+json", "Content-Type": kind})
    with urllib.request.urlopen(request, timeout=240) as response:
        return json.load(response)
base = f"https://api.github.com/repos/{repo}"
# GitHub's by-tag endpoint does not return draft releases. Paginate the
# authenticated release list so both fresh drafts and published versions work.
release = None
page = 1
while release is None:
    rows = api("GET", base + f"/releases?per_page=100&page={page}")
    release = next((row for row in rows if row["tag_name"] == tag), None)
    if release is None and not rows:
        raise SystemExit("Release not found: " + tag)
    page += 1
old_assets = [a for a in release["assets"] if a["name"] == archive.name]
# Keep the previous download available until its replacement is verified.
temporary_name = archive.name + ".pending-" + os.environ.get("GITHUB_RUN_ID", digest[:12])
upload = release["upload_url"].split("{", 1)[0] + "?name=" + urllib.parse.quote(temporary_name)
asset = api("POST", upload, archive.read_bytes(), "application/zip")
if asset["size"] != archive.stat().st_size or asset["state"] != "uploaded":
    raise SystemExit("Uploaded ZIP verification failed")
if asset.get("digest") and asset["digest"] != "sha256:" + digest:
    raise SystemExit("Uploaded ZIP checksum mismatch")
for old in old_assets:
    request = urllib.request.Request(base + "/releases/assets/" + str(old["id"]), method="DELETE",
        headers={"Authorization": "Bearer " + token, "User-Agent": "OpenChess-Windows-release"})
    with urllib.request.urlopen(request, timeout=60) as response:
        if response.status != 204:
            raise SystemExit("Could not replace previous Windows asset")
asset = api("PATCH", base + "/releases/assets/" + str(asset["id"]),
            json.dumps({"name": archive.name}).encode())
print("DOWNLOAD_URL=" + asset["browser_download_url"], flush=True)
