#!/usr/bin/env python3
"""
Build script for MAGDA Python library.

This script helps build, test, and prepare the package for distribution.
"""

import subprocess
import sys
from pathlib import Path


def run_command(cmd: list[str], description: str) -> bool:
    """Run a command and return success status."""
    print(f"\n🔄 {description}...")
    print(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        print(f"✅ {description} completed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ {description} failed:")
        print(f"stdout: {e.stdout}")
        print(f"stderr: {e.stderr}")
        return False


def main():
    """Main build process."""
    print("🚀 Building MAGDA Python Library")
    print("=" * 50)

    # Check if we're in the right directory
    if not Path("pyproject.toml").exists():
        print(
            "❌ Error: pyproject.toml not found. Run this script from the python/ directory."
        )
        sys.exit(1)

    # Step 1: Clean previous builds
    if not run_command(
        ["rm", "-rf", "dist", "build", "*.egg-info"], "Cleaning previous builds"
    ):
        print("⚠️  Warning: Could not clean previous builds")

    # Step 2: Install dependencies
    if not run_command(["uv", "sync"], "Installing dependencies"):
        print("❌ Failed to install dependencies")
        sys.exit(1)

    # Step 3: Run tests
    if not run_command(["uv", "run", "pytest", "tests/", "-v"], "Running tests"):
        print("❌ Tests failed")
        sys.exit(1)

    # Step 4: Run linting
    if not run_command(["uv", "run", "ruff", "check", "."], "Running linting"):
        print("⚠️  Warning: Linting issues found")

    # Step 5: Build the package
    if not run_command(["uv", "run", "python", "-m", "build"], "Building package"):
        print("❌ Build failed")
        sys.exit(1)

    # Step 6: Check the built package
    if not run_command(
        ["uv", "run", "twine", "check", "dist/*"], "Checking built package"
    ):
        print("❌ Package check failed")
        sys.exit(1)

    print("\n🎉 Build completed successfully!")
    print("\n📦 Package files created in dist/ directory")
    print("\n📋 Next steps:")
    print("  1. Test installation: pip install dist/magda-*.whl")
    print("  2. Upload to PyPI: twine upload dist/*")
    print("  3. Or install locally: pip install -e .")


if __name__ == "__main__":
    main()
