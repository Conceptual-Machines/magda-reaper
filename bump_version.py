#!/usr/bin/env python3
import re
import sys
from pathlib import Path

PYPROJECT_PATH = Path("pyproject.toml")
CMAKE_PATH = Path("cpp/CMakeLists.txt")
VCPKG_PATH = Path("vcpkg.json")

if len(sys.argv) != 2 or sys.argv[1] not in ("patch", "minor", "major"):
    print("Usage: python bump_version.py [patch|minor|major]")
    sys.exit(1)

bump_type = sys.argv[1]

# Read pyproject.toml
text = PYPROJECT_PATH.read_text()

# Find the version line
match = re.search(r"^version\s*=\s*['\"](\d+)\.(\d+)\.(\d+)['\"]", text, re.MULTILINE)
if not match:
    print("Could not find version in pyproject.toml")
    sys.exit(1)

major, minor, patch = map(int, match.groups())
old_version = f"{major}.{minor}.{patch}"

if bump_type == "patch":
    patch += 1
elif bump_type == "minor":
    minor += 1
    patch = 0
elif bump_type == "major":
    major += 1
    minor = 0
    patch = 0

new_version = f"{major}.{minor}.{patch}"

# Replace the version in pyproject.toml
new_text = re.sub(
    r"(^version\s*=\s*['\"])(\d+\.\d+\.\d+)(['\"])",
    rf"\g<1>{new_version}\3",
    text,
    flags=re.MULTILINE,
)
PYPROJECT_PATH.write_text(new_text)

# Update CMakeLists.txt
if CMAKE_PATH.exists():
    cmake_text = CMAKE_PATH.read_text()
    cmake_text = re.sub(
        r"(project\(magda_cpp VERSION )(\d+\.\d+\.\d+)( LANGUAGES CXX\))",
        rf"\g<1>{new_version}\3",
        cmake_text,
    )
    CMAKE_PATH.write_text(cmake_text)
    print(f"Updated {CMAKE_PATH}")

# Update vcpkg.json
if VCPKG_PATH.exists():
    vcpkg_text = VCPKG_PATH.read_text()
    vcpkg_text = re.sub(
        r"(\"version\": \")(\d+\.\d+\.\d+)(\")",
        rf"\g<1>{new_version}\3",
        vcpkg_text,
    )
    VCPKG_PATH.write_text(vcpkg_text)
    print(f"Updated {VCPKG_PATH}")

print(f"Bumped version: {old_version} -> {new_version}")
