# 🚀 MAGDA Release Process

This document explains how to create releases for MAGDA, including our automated scripts and GitHub Actions workflow.

## 📋 Overview

MAGDA uses a **tag-based release workflow** that automatically creates GitHub releases when semantic version tags are pushed. The process ensures version consistency across all project files and generates release notes from git history.

## 🛠️ Release Scripts

### `bump_version.py`

**Purpose**: Automatically bumps version numbers across all project files to maintain consistency.

**Usage**:
```bash
python bump_version.py [patch|minor|major] [--tag]
```

**What it does**:
1. **Reads current version** from `pyproject.toml`
2. **Bumps version** according to semantic versioning:
   - `patch`: Increments patch version (0.2.3 → 0.2.4)
   - `minor`: Increments minor version, resets patch (0.2.3 → 0.3.0)
   - `major`: Increments major version, resets minor and patch (0.2.3 → 1.0.0)
3. **Updates multiple files**:
   - `pyproject.toml` - Python package version
   - `cpp/CMakeLists.txt` - C++ project version
   - `vcpkg.json` - vcpkg package version (if exists)
4. **Optional release branch creation** (with `--tag` flag):
   - Creates a release branch (e.g., `release/v0.2.4`)
   - Commits version bump to release branch
   - Pushes release branch to remote
   - Creates and pushes release tag
   - Triggers automated release workflow
   - Works with protected main branches

**Examples**:
```bash
# Just bump version (from any branch)
python bump_version.py patch
# Result: 0.2.4 in all files

# Bump version and create release branch/tag (from any branch)
python bump_version.py patch --tag
# Result: 0.2.4 in all files + release branch + tag + release workflow
```

## 🔄 Release Workflow (`.github/workflows/release.yml`)

### Trigger
- **Activates on**: Push of tags matching `v*.*.*` (e.g., `v0.2.4`, `v1.0.0`)
- **Does NOT run on**: Regular commits, PR merges, or non-semantic tags

### Jobs

#### 1. `check-open-prs`
- **Purpose**: Prevents duplicate releases when PRs are open
- **Action**: Skips release if open PRs target `main` or `develop`
- **Output**: `has_open_prs` (true/false)

#### 2. `validate-version`
- **Purpose**: Ensures version consistency and format
- **Checks**:
  - Version format is valid (MAJOR.MINOR.PATCH or with prerelease suffix)
  - Tag version is newer than versions in `pyproject.toml` and `CMakeLists.txt`
- **Output**: `version` and `is_prerelease`

#### 3. `create-release`
- **Purpose**: Creates the actual GitHub release
- **Actions**:
  - Generates release notes from git history since last tag
  - Creates GitHub release with changelog
  - Marks as prerelease if tag contains `alpha`, `beta`, or `rc`

## 📝 Step-by-Step Release Process

### Prerequisites
- Feature branch is complete and tested
- All changes are committed
- You have write access to the repository

### 1. Prepare Release Branch
```bash
# Ensure you're on your feature branch
git checkout feature/your-feature-branch

# Bump version (choose appropriate level)
python bump_version.py patch    # for bug fixes
python bump_version.py minor    # for new features
python bump_version.py major    # for breaking changes

# Commit version bump
git add .
git commit -m "bump: release v0.2.4"
```

### 2. Create Release PR
```bash
# Push your branch
git push origin feature/your-feature-branch

# Create PR to main branch
gh pr create --title "Release v0.2.4" --body "Release v0.2.4 with [list of changes]"
```

### 3. Merge and Tag
```bash
# After PR is approved and merged
git checkout main
git pull origin main

# Option A: Manual tag creation
git tag v0.2.4
git push origin v0.2.4

# Option B: Automated release branch/tag creation (recommended)
python bump_version.py patch --tag
```

### 4. Monitor Release
- The release workflow will automatically run
- Check GitHub Actions tab for progress
- Release will appear in GitHub Releases section

## 🔍 Version Validation Rules

The release workflow enforces these rules:

1. **Format**: Must match `MAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH-prerelease`
2. **Consistency**: Tag version must match versions in project files
3. **Progression**: Tag version must be newer than current versions
4. **No Conflicts**: No open PRs targeting main/develop

## 🚨 Common Issues and Solutions

### Issue: "Version validation failed"
**Cause**: Tag version is not newer than current versions in project files
**Solution**: Use `python bump_version.py` to bump version before creating tag

### Issue: "Found open PR(s). Skipping release"
**Cause**: Open PRs targeting main or develop branches
**Solution**: Merge or close the PRs before creating release tag

### Issue: "Invalid version format"
**Cause**: Tag doesn't follow semantic versioning (e.g., `v32` instead of `v3.2.0`)
**Solution**: Use proper semantic versioning format

### Issue: Release workflow doesn't run
**Cause**: Tag doesn't match `v*.*.*` pattern
**Solution**: Ensure tag follows semantic versioning (e.g., `v0.2.4`)

## 📊 Release Types

### Stable Release
- Tag format: `v1.2.3`
- Creates regular GitHub release
- Available to all users

### Prerelease
- Tag format: `v1.2.3-alpha.1`, `v1.2.3-beta.2`, `v1.2.3-rc.1`
- Creates prerelease on GitHub
- Marked as "pre-release" in GitHub UI
- Good for testing before stable release

## 🔧 Quick Release (One Command)

For quick releases that work with protected branches:

```bash
# Bump version, create release branch, and create release tag in one command
python bump_version.py patch --tag
```

This will:
1. Bump the version in all files
2. Create a release branch (e.g., `release/v0.2.4`)
3. Commit the changes to the release branch
4. Push the release branch to remote
5. Create and push the release tag
6. Trigger the automated release workflow
7. After release, merge release branch to main (optional)

## 🔧 Manual Release (Advanced)

If you need more control over the release process:

```bash
# 1. Bump version
python bump_version.py patch

# 2. Commit changes
git add .
git commit -m "bump: release v0.2.4"

# 3. Create tag
git tag v0.2.4

# 4. Push both commit and tag
git push origin main
git push origin v0.2.4
```

## 📚 Best Practices

1. **Always test before release**: Ensure all tests pass
2. **Use descriptive commit messages**: Helps with changelog generation
3. **Choose appropriate version bump**:
   - `patch`: Bug fixes and minor improvements
   - `minor`: New features (backward compatible)
   - `major`: Breaking changes
4. **Review release notes**: Check generated changelog before publishing
5. **Tag from main branch**: Ensures release is from stable code

## 🆘 Troubleshooting

### Check Current Versions
```bash
# Python version
grep "version = " pyproject.toml

# C++ version
grep "project(magda_cpp VERSION" cpp/CMakeLists.txt
```

### List Recent Tags
```bash
git tag --list | tail -10
```

### Check Workflow Status
```bash
gh run list --workflow=release.yml --limit 5
```

### Manual Workflow Trigger
If needed, you can manually trigger the workflow from GitHub Actions tab, but this bypasses some validation.

---

For questions or issues with the release process, please open an issue in the repository.
