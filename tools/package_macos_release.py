#!/usr/bin/env python3
"""Package the tested macOS release binary with its runtime assets."""

from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
NAME = "ItsAlmostNight-macOS-arm64"


def main() -> None:
    binary = ROOT / "build/release/its_almost_night"
    assets = ROOT / "build/release/assets"
    if not binary.is_file() or not assets.is_dir():
        raise SystemExit("Build the release preset before packaging.")

    output = ROOT / "dist"
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ian-package-", dir=output) as temporary:
        staging = Path(temporary)
        bundle = staging / NAME
        bundle.mkdir()
        app = bundle / "ItsAlmostNight.app"
        executable_dir = app / "Contents" / "MacOS"
        executable_dir.mkdir(parents=True)
        shutil.copy2(binary, executable_dir / "ItsAlmostNight")
        shutil.copytree(
            assets, bundle / "assets",
            ignore=shutil.ignore_patterns(".DS_Store", "__MACOSX"),
        )
        with (app / "Contents" / "Info.plist").open("wb") as stream:
            plistlib.dump({
                "CFBundleDevelopmentRegion": "en",
                "CFBundleExecutable": "ItsAlmostNight",
                "CFBundleIdentifier": "com.romankazz.itsalmostnight",
                "CFBundleInfoDictionaryVersion": "6.0",
                "CFBundleName": "ItsAlmostNight",
                "CFBundlePackageType": "APPL",
                "CFBundleShortVersionString": "0.1.8",
                "CFBundleVersion": "8",
                "NSHighResolutionCapable": True,
            }, stream)
        subprocess.run(
            ["codesign", "--force", "--sign", "-", "--timestamp=none", str(app)],
            check=True,
        )
        shutil.copy2(ROOT / "README.md", bundle / "README.md")

        archive = staging / f"{NAME}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as result:
            for path in sorted(bundle.rglob("*")):
                if path.is_file():
                    result.write(path, path.relative_to(staging))

        destination = output / archive.name
        os.replace(archive, destination)
        print(destination)


if __name__ == "__main__":
    main()
