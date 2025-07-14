#!/usr/bin/env python3
"""
MAGDA Release Script

Automates the entire release process:
1. Bump version in pyproject.toml
2. Create git tag
3. Push changes and tag
4. Optionally create GitHub release

Usage:
    python scripts/release.py [patch|minor|major] [--dry-run] [--no-push]
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

PYPROJECT_PATH = Path("python/pyproject.toml")


def run_command(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess:
    """Run a shell command and return the result."""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=check)
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    return result


def get_current_version() -> str:
    """Get the current version from pyproject.toml."""
    text = PYPROJECT_PATH.read_text()
    match = re.search(r'^version\s*=\s*["\'](\d+\.\d+\.\d+)["\']', text, re.MULTILINE)
    if not match:
        raise ValueError("Could not find version in pyproject.toml")
    return match.group(1)


def bump_version(bump_type: str) -> tuple[str, str]:
    """Bump the version and return (old_version, new_version)."""
    text = PYPROJECT_PATH.read_text()
    match = re.search(
        r'^version\s*=\s*["\'](\d+)\.(\d+)\.(\d+)["\']', text, re.MULTILINE
    )
    if not match:
        raise ValueError("Could not find version in pyproject.toml")

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
    else:
        raise ValueError(f"Invalid bump type: {bump_type}")

    new_version = f"{major}.{minor}.{patch}"

    # Replace the version in the file
    new_text = re.sub(
        r'(^version\s*=\s*["\'])(\d+\.\d+\.\d+)(["\'])',
        rf"\g<1>{new_version}\3",
        text,
        flags=re.MULTILINE,
    )

    PYPROJECT_PATH.write_text(new_text)
    return old_version, new_version


def check_git_status() -> bool:
    """Check if git working directory is clean."""
    result = run_command(["git", "status", "--porcelain"], check=False)
    return result.returncode == 0 and not result.stdout.strip()


def create_git_tag(version: str, dry_run: bool = False) -> None:
    """Create a git tag for the version."""
    tag_name = f"v{version}"

    if dry_run:
        print(f"[DRY RUN] Would create tag: {tag_name}")
        return

    # Create tag
    run_command(["git", "tag", "-a", tag_name, "-m", f"Release {tag_name}"])
    print(f"Created tag: {tag_name}")


def push_changes_and_tag(
    version: str, dry_run: bool = False, no_push: bool = False
) -> None:
    """Push changes and tag to remote."""
    tag_name = f"v{version}"

    if no_push:
        print(f"[NO PUSH] Skipping push for tag: {tag_name}")
        return

    if dry_run:
        print(f"[DRY RUN] Would push changes and tag: {tag_name}")
        return

    # Add and commit changes
    run_command(["git", "add", "python/pyproject.toml"])
    run_command(["git", "commit", "-m", f"chore: bump version to {version}"])

    # Push changes
    run_command(["git", "push"])
    print("Pushed changes")

    # Push tag
    run_command(["git", "push", "origin", tag_name])
    print(f"Pushed tag: {tag_name}")


def main():
    parser = argparse.ArgumentParser(description="MAGDA Release Script")
    parser.add_argument(
        "bump_type", choices=["patch", "minor", "major"], help="Type of version bump"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without making changes",
    )
    parser.add_argument(
        "--no-push", action="store_true", help="Don't push changes to remote"
    )

    args = parser.parse_args()

    print("🎵 MAGDA Release Script")
    print("=" * 50)

    # Check git status
    if not check_git_status() and not args.dry_run:
        print("❌ Error: Git working directory is not clean")
        print("Please commit or stash your changes before releasing")
        sys.exit(1)

    try:
        # Get current version
        current_version = get_current_version()
        print(f"Current version: {current_version}")

        # Bump version
        if args.dry_run:
            print(
                f"[DRY RUN] Would bump version from {current_version} ({args.bump_type})"
            )
            # Simulate version bump for dry run
            major, minor, patch = map(int, current_version.split("."))
            if args.bump_type == "patch":
                patch += 1
            elif args.bump_type == "minor":
                minor += 1
                patch = 0
            elif args.bump_type == "major":
                major += 1
                minor = 0
                patch = 0
            new_version = f"{major}.{minor}.{patch}"
        else:
            old_version, new_version = bump_version(args.bump_type)
            print(f"Bumped version: {old_version} → {new_version}")

        # Create git tag
        create_git_tag(new_version, dry_run=args.dry_run)

        # Push changes and tag
        push_changes_and_tag(new_version, dry_run=args.dry_run, no_push=args.no_push)

        print("\n✅ Release process completed!")
        if not args.dry_run and not args.no_push:
            print(f"🎉 Version {new_version} has been released!")
            print("The GitHub Actions workflow will automatically create a release.")
        elif args.dry_run:
            print("🔍 This was a dry run. No changes were made.")
        elif args.no_push:
            print("📦 Changes are ready but not pushed. Run 'git push' when ready.")

    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
