import os
import zipfile

release_dir = r"c:\Users\x\Documents\antigravity\braun_rb-26\releases"
build_dir = r"c:\Users\x\Documents\antigravity\braun_rb-26\build\BRAUN_RB26_artefacts\Release"
root_dir = r"c:\Users\x\Documents\antigravity\braun_rb-26"

os.makedirs(release_dir, exist_ok=True)

# Extract version from package.json
import json
with open(os.path.join(root_dir, "package.json"), "r", encoding="utf-8") as f:
    pkg = json.load(f)
version = pkg.get("version", "1.4.4")

# 1. Package Windows-x64 full zip
zip_path = os.path.join(release_dir, f"BRAUN_RB26-v{version}-Windows-x64.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "BRAUN_RB26.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    clap_file = os.path.join(build_dir, "CLAP", "BRAUN_RB26.clap")
    if os.path.exists(clap_file):
        zf.write(clap_file, "BRAUN_RB26.clap")
    standalone_file = os.path.join(build_dir, "Standalone", "BRAUN_RB26.exe")
    zf.write(standalone_file, "BRAUN_RB26.exe")
    zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")
    zf.write(os.path.join(root_dir, "README.md"), "README.md")

print(f"Created {zip_path}: {os.path.getsize(zip_path):,} bytes")

# 2. Package VST3-only zip
vst3_zip_path = os.path.join(release_dir, f"BRAUN_RB26-v{version}-VST3-Windows-x64.zip")
with zipfile.ZipFile(vst3_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "BRAUN_RB26.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")
    zf.write(os.path.join(root_dir, "README.md"), "README.md")

print(f"Created {vst3_zip_path}: {os.path.getsize(vst3_zip_path):,} bytes")
