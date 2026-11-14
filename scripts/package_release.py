import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import sys
import zipfile

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
release_dir = os.path.join(root_dir, "releases")
build_dir = os.path.join(root_dir, "build", "BRAUN_RB26_artefacts", "Release")

os.makedirs(release_dir, exist_ok=True)

# Extract version from command line or package.json
if len(sys.argv) > 1 and sys.argv[1].strip():
    version = sys.argv[1].strip()
else:
    with open(os.path.join(root_dir, "package.json"), "r", encoding="utf-8") as f:
        pkg = json.load(f)
    version = pkg.get("version", "1.5.0")

expected_company = "Sneed's Feed & Seed Ltd."


def verify_windows_pe_version_info(binary_path, expected_ver, expected_comp):
    if sys.platform != "win32":
        return
    if not os.path.isfile(binary_path):
        raise FileNotFoundError(f"Missing release binary: {binary_path}")

    ver_dll = ctypes.WinDLL("version", use_last_error=True)
    ver_dll.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    ver_dll.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    ver_dll.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID]
    ver_dll.GetFileVersionInfoW.restype = wintypes.BOOL
    ver_dll.VerQueryValueW.argtypes = [
        wintypes.LPCVOID,
        wintypes.LPCWSTR,
        ctypes.POINTER(wintypes.LPVOID),
        ctypes.POINTER(wintypes.UINT),
    ]
    ver_dll.VerQueryValueW.restype = wintypes.BOOL

    dummy = wintypes.DWORD(0)
    size = ver_dll.GetFileVersionInfoSizeW(binary_path, ctypes.byref(dummy))
    if not size:
        raise RuntimeError(f"PE VERSIONINFO resource missing in {binary_path}")

    buf = ctypes.create_string_buffer(size)
    if not ver_dll.GetFileVersionInfoW(binary_path, 0, size, buf):
        raise RuntimeError(f"GetFileVersionInfoW failed for {binary_path}")

    ptr = wintypes.LPVOID()
    length = wintypes.UINT(0)
    if not ver_dll.VerQueryValueW(buf, r"\VarFileInfo\Translation", ctypes.byref(ptr), ctypes.byref(length)) or length.value < 4:
        lang_codepage = "040904E4"
    else:
        lang, codepage = ctypes.cast(ptr, ctypes.POINTER(wintypes.WORD * 2)).contents
        lang_codepage = f"{lang:04X}{codepage:04X}"

    def query_str(field_name):
        for lc in (lang_codepage, "040904E4", "040904B0"):
            sub_block = f"\\StringFileInfo\\{lc}\\{field_name}"
            sp = wintypes.LPVOID()
            slen = wintypes.UINT(0)
            if ver_dll.VerQueryValueW(buf, sub_block, ctypes.byref(sp), ctypes.byref(slen)) and slen.value > 0:
                return ctypes.wstring_at(sp.value).strip()
        return ""

    file_ver = query_str("FileVersion")
    prod_ver = query_str("ProductVersion")
    company = query_str("CompanyName")

    if not file_ver.startswith(expected_ver) or not prod_ver.startswith(expected_ver) or company != expected_comp:
        raise RuntimeError(
            f"PE VERSIONINFO mismatch in {os.path.basename(binary_path)}: "
            f"FileVersion='{file_ver}', ProductVersion='{prod_ver}', CompanyName='{company}' "
            f"(expected version '{expected_ver}', CompanyName '{expected_comp}')"
        )
    print(
        f"Verified PE VERSIONINFO ({os.path.basename(binary_path)}): "
        f"FileVersion='{file_ver}', ProductVersion='{prod_ver}', CompanyName='{company}'"
    )


standalone_binary = os.path.join(build_dir, "Standalone", "BRAUN_RB26.exe")
vst3_binary = os.path.join(build_dir, "VST3", "BRAUN_RB26.vst3", "Contents", "x86_64-win", "BRAUN_RB26.vst3")
verify_windows_pe_version_info(standalone_binary, version, expected_company)
verify_windows_pe_version_info(vst3_binary, version, expected_company)

expected_zips = {
    f"BRAUN_RB26-v{version}-Windows-x64.zip",
    f"BRAUN_RB26-v{version}-VST3-Windows-x64.zip",
}

# Remove stale old-version .zip archives from releases/ before packaging
for existing_name in os.listdir(release_dir):
    if existing_name.endswith(".zip") and existing_name not in expected_zips:
        stale_path = os.path.join(release_dir, existing_name)
        os.remove(stale_path)
        print(f"Removed stale archive: {stale_path}")

# 1. Package Windows-x64 full zip
zip_path = os.path.join(release_dir, f"BRAUN_RB26-v{version}-Windows-x64.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "BRAUN_RB26.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        dirs.sort()
        for fname in sorted(files):
            full = os.path.join(root, fname)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    clap_file = os.path.join(build_dir, "CLAP", "BRAUN_RB26.clap")
    if os.path.exists(clap_file):
        zf.write(clap_file, "BRAUN_RB26.clap")
    standalone_file = os.path.join(build_dir, "Standalone", "BRAUN_RB26.exe")
    zf.write(standalone_file, "BRAUN_RB26.exe")
    zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")
    zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "ARCHITECTURE.md")):
        zf.write(os.path.join(root_dir, "ARCHITECTURE.md"), "ARCHITECTURE.md")

print(f"Created {zip_path}: {os.path.getsize(zip_path):,} bytes")

# 2. Package VST3-only zip
vst3_zip_path = os.path.join(release_dir, f"BRAUN_RB26-v{version}-VST3-Windows-x64.zip")
with zipfile.ZipFile(vst3_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    vst3_dir = os.path.join(build_dir, "VST3", "BRAUN_RB26.vst3")
    for root, dirs, files in os.walk(vst3_dir):
        dirs.sort()
        for fname in sorted(files):
            full = os.path.join(root, fname)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")
    zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "ARCHITECTURE.md")):
        zf.write(os.path.join(root_dir, "ARCHITECTURE.md"), "ARCHITECTURE.md")

print(f"Created {vst3_zip_path}: {os.path.getsize(vst3_zip_path):,} bytes")


# 3. Compute SHA-256 sums and write releases/SHA256SUMS.txt
def sha256_file(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


sums_file = os.path.join(release_dir, "SHA256SUMS.txt")
packages = [zip_path, vst3_zip_path]

lines = []
for p in sorted(packages, key=lambda x: os.path.basename(x)):
    if os.path.exists(p):
        digest = sha256_file(p)
        filename = os.path.basename(p)
        lines.append(f"{digest}  {filename}")
        print(f"SHA256 ({filename}) = {digest}")

with open(sums_file, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print(f"Wrote SHA-256 checksums to {sums_file}")
