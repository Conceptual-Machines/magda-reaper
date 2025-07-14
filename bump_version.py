#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path

PYPROJECT_PATH = Path("pyproject.toml")
CMAKE_PATH = Path("cpp/CMakeLists.txt")
VCPKG_PATH = Path("vcpkg.json")


def run_command(cmd, check=True):
    """Run a shell command and return the result."""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, check=check
        )
        return result.stdout.strip(), result.stderr.strip(), result.returncode
    except subprocess.CalledProcessError as e:
        return e.stdout.strip(), e.stderr.strip(), e.returncode


def get_current_branch():
    """Get the current git branch name."""
    stdout, stderr, returncode = run_command("git branch --show-current")
    if returncode != 0:
        print(f"Error getting current branch: {stderr}")
        return None
    return stdout


def check_git_status():
    """Check if there are uncommitted changes."""
    stdout, stderr, returncode = run_command("git status --porcelain")
    if returncode != 0:
        print(f"Error checking git status: {stderr}")
        return False
    return stdout.strip() != ""


def create_tag_only(version):
    """Create and push a release tag without creating a branch."""
    tag_name = f"v{version}"

    # Check if tag already exists
    stdout, stderr, returncode = run_command(f"git tag --list {tag_name}")
    if returncode == 0 and tag_name in stdout:
        print(f"❌ Tag {tag_name} already exists!")
        return False

    # Create the tag
    print(f"Creating tag {tag_name}...")
    stdout, stderr, returncode = run_command(f"git tag {tag_name}")
    if returncode != 0:
        print(f"❌ Failed to create tag: {stderr}")
        return False

    # Push the tag
    print(f"Pushing tag {tag_name}...")
    stdout, stderr, returncode = run_command(f"git push origin {tag_name}")
    if returncode != 0:
        print(f"❌ Failed to push tag: {stderr}")
        print(
            f"Tag was created locally. You can push it manually with: git push origin {tag_name}"
        )
        return False

    print(f"✅ Successfully created and pushed tag {tag_name}")
    print("🚀 Release workflow will now run automatically!")
    return True


def create_release_branch_and_tag(version):
    """Create a release branch, commit changes, and create tag."""
    tag_name = f"v{version}"
    release_branch = f"release/{version}"

    # Check if tag already exists
    stdout, stderr, returncode = run_command(f"git tag --list {tag_name}")
    if returncode == 0 and tag_name in stdout:
        print(f"❌ Tag {tag_name} already exists!")
        return False

    # Check if release branch already exists
    stdout, stderr, returncode = run_command(f"git branch --list {release_branch}")
    if returncode == 0 and release_branch in stdout:
        print(f"❌ Release branch {release_branch} already exists!")
        return False

    # Create release branch
    print(f"Creating release branch {release_branch}...")
    stdout, stderr, returncode = run_command(f"git checkout -b {release_branch}")
    if returncode != 0:
        print(f"❌ Failed to create release branch: {stderr}")
        return False

    # Commit the version bump
    print("Committing version bump...")
    stdout, stderr, returncode = run_command("git add .")
    if returncode != 0:
        print(f"❌ Failed to stage changes: {stderr}")
        return False

    stdout, stderr, returncode = run_command(
        f'git commit -m "bump: release v{version}"'
    )
    if returncode != 0:
        print(f"❌ Failed to commit changes: {stderr}")
        return False

    # Push the release branch
    print(f"Pushing release branch {release_branch}...")
    stdout, stderr, returncode = run_command(f"git push origin {release_branch}")
    if returncode != 0:
        print(f"❌ Failed to push release branch: {stderr}")
        print(
            f"Release branch was created locally. You can push it manually with: git push origin {release_branch}"
        )
        return False

    # Create the tag on the release branch
    print(f"Creating tag {tag_name} on release branch...")
    stdout, stderr, returncode = run_command(f"git tag {tag_name}")
    if returncode != 0:
        print(f"❌ Failed to create tag: {stderr}")
        return False

    # Push the tag
    print(f"Pushing tag {tag_name}...")
    stdout, stderr, returncode = run_command(f"git push origin {tag_name}")
    if returncode != 0:
        print(f"❌ Failed to push tag: {stderr}")
        print(
            f"Tag was created locally. You can push it manually with: git push origin {tag_name}"
        )
        return False

    print(f"✅ Successfully created release branch {release_branch} and tag {tag_name}")
    print("🚀 Release workflow will now run automatically!")
    return True


if len(sys.argv) < 2 or sys.argv[1] not in ("patch", "minor", "major"):
    print("Usage: python bump_version.py [patch|minor|major] [--tag|--tag-only]")
    print("")
    print("Options:")
    print("  --tag       Create release branch and tag after bumping version")
    print(
        "  --tag-only  Create only a release tag (cheaper, works with protected branches)"
    )
    sys.exit(1)

bump_type = sys.argv[1]
create_tag = "--tag" in sys.argv
create_tag_only_flag = "--tag-only" in sys.argv

# Check git status
if check_git_status():
    print("❌ You have uncommitted changes. Please commit or stash them first.")
    sys.exit(1)

# Check current branch
current_branch = get_current_branch()
if not current_branch:
    sys.exit(1)

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

print(f"✅ Bumped version: {old_version} -> {new_version}")

# Create tag or release branch based on option
if create_tag_only_flag:
    print(f"\n🎯 Creating release tag v{new_version} (tag-only mode)...")

    if create_tag_only(new_version):
        print(f"\n🎉 Release v{new_version} is ready!")
        print(
            "📋 Check GitHub Actions for release progress: https://github.com/lucaromagnoli/magda/actions"
        )
        print(
            "\n💡 Note: Version bump is only local. Consider committing it to your current branch."
        )
    else:
        print(f"\n⚠️  Version bumped to {new_version}, but tag creation failed.")
        print("You can create the tag manually with:")
        print(f"  git tag v{new_version} && git push origin v{new_version}")

elif create_tag:
    print(f"\n🎯 Creating release branch and tag v{new_version}...")

    if create_release_branch_and_tag(new_version):
        print(f"\n🎉 Release v{new_version} is ready!")
        print(
            "📋 Check GitHub Actions for release progress: https://github.com/lucaromagnoli/magda/actions"
        )
        print("\n📝 After release is complete, you can:")
        print(
            f"1. Merge the release branch to main: git checkout main && git merge release/v{new_version}"
        )
        print(
            f"2. Delete the release branch: git branch -d release/v{new_version} && git push origin --delete release/v{new_version}"
        )
    else:
        print(
            f"\n⚠️  Version bumped to {new_version}, but release branch/tag creation failed."
        )
        print("You can create them manually:")
        print(f"  git checkout -b release/v{new_version}")
        print(f"  git add . && git commit -m 'bump: release v{new_version}'")
        print(f"  git push origin release/v{new_version}")
        print(f"  git tag v{new_version} && git push origin v{new_version}")
else:
    print("\n📝 Next steps:")
    print(
        f"1. Commit these changes: git add . && git commit -m 'bump: release v{new_version}'"
    )
    if current_branch != "main":
        print(f"2. Push and create PR: git push origin {current_branch}")
        print("3. Merge PR to main")
    print(
        f"4. Create release tag: git tag v{new_version} && git push origin v{new_version}"
    )
    print(
        f"5. Or use: python bump_version.py {bump_type} --tag (creates release branch)"
    )
    print(
        f"6. Or use: python bump_version.py {bump_type} --tag-only (creates tag only, cheaper)"
    )
