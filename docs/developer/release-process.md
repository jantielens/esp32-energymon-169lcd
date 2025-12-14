# Release Process

Complete guide to creating and publishing releases for the ESP32 Energy Monitor firmware.

## Table of Contents

- [Overview](#overview)
- [Version Management](#version-management)
- [Release Workflow](#release-workflow)
- [Release Scenarios](#release-scenarios)
- [Automation](#automation)
- [Troubleshooting](#troubleshooting)

## Overview

The project uses an automated release workflow that triggers on version tags. When you push a tag like `v1.2.0`, GitHub Actions automatically:

1. Builds firmware for all supported boards (ESP32, ESP32-C3)
2. Extracts version-specific changelog notes
3. Creates a GitHub Release with branded firmware binaries
4. Generates SHA256 checksums for verification

**Release Components:**

- **Version Tracking**: [`src/version.h`](../../src/version.h) - Semantic version numbers (MAJOR.MINOR.PATCH)
- **Release Notes**: [`CHANGELOG.md`](../../CHANGELOG.md) - Keep a Changelog format
- **Automation**: [`create-release.sh`](../../create-release.sh) - Release preparation helper
- **Extraction**: [`tools/extract-changelog.sh`](../../tools/extract-changelog.sh) - Parse changelog sections
- **CI/CD**: `.github/workflows/release.yml` - GitHub Actions pipeline

## Version Management

### Semantic Versioning

The project follows [Semantic Versioning 2.0.0](https://semver.org/):

**Format**: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incompatible API changes or major architectural changes
- **MINOR**: New features, backward-compatible
- **PATCH**: Bug fixes, backward-compatible

**Examples:**
- `1.0.0` → `1.1.0`: Added LCD brightness control (new feature)
- `1.1.0` → `1.1.1`: Fixed WiFi reconnection bug (bug fix)
- `1.1.1` → `2.0.0`: Changed MQTT message format (breaking change)

### Version Files

#### src/version.h

C header file with version macros:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
```

**Used for:**
- Firmware bootup logging
- Web portal "About" page
- OTA update compatibility checks

**Auto-updated by**: `create-release.sh` script

#### CHANGELOG.md

Human-readable release notes in [Keep a Changelog](https://keepachangelog.com/) format:

```markdown
## [Unreleased]

### Added
- Feature currently in development

---

## [1.2.0] - 2025-12-13

### Added
- LCD brightness control via web portal

### Changed
- Improved WiFi connection stability

### Fixed
- Display color accuracy for anti-aliased graphics
```

**Sections:**
- **Unreleased**: Work in progress, not yet released
- **[VERSION] - DATE**: Published releases with grouped changes
  - **Added**: New features
  - **Changed**: Changes to existing features
  - **Deprecated**: Soon-to-be removed features
  - **Removed**: Removed features
  - **Fixed**: Bug fixes
  - **Security**: Security fixes

**Auto-updated by**: `create-release.sh` (moves Unreleased → versioned section)

## Release Workflow

### Standard Release (Recommended)

**Use for**: Regular version releases (e.g., `v1.2.0`, `v2.0.0`)

#### Step 1: Prepare Release

Run the automated preparation script:

```bash
./create-release.sh 1.2.0 "LCD brightness control and stability improvements"
```

**What it does:**
1. ✅ Validates version format (MAJOR.MINOR.PATCH)
2. ✅ Checks you're on `main` branch with no uncommitted changes
3. ✅ Pulls latest changes from origin
4. ✅ Creates branch `release/v1.2.0`
5. ✅ Updates `src/version.h` with version numbers (1, 2, 0)
6. ✅ Moves `[Unreleased]` section in `CHANGELOG.md` to `[1.2.0] - 2025-12-14`
7. ✅ Adds new empty `[Unreleased]` section for next development
8. ✅ Commits changes with message: `chore: bump version to 1.2.0`
9. ✅ Pushes branch to origin
10. ✅ Provides PR creation URL

**Example output:**
```
=== ESP32 Release Preparation ===
Version: 1.2.0
Title: LCD brightness control and stability improvements

Step 1: Pulling latest changes...
Already up to date.

Step 2: Creating release branch release/v1.2.0...
Switched to a new branch 'release/v1.2.0'

Step 3: Updating src/version.h...
Updated version.h:
#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0

Step 4: Updating CHANGELOG.md...
Updated CHANGELOG.md with version 1.2.0 - 2025-12-14

Changes to be committed:
 CHANGELOG.md  | 12 +++++-------
 src/version.h |  6 +++---
 2 files changed, 8 insertions(+), 10 deletions(-)

Review the changes above.
Commit these changes? (y/N) y

Step 5: Committing changes...
[release/v1.2.0 abc1234] chore: bump version to 1.2.0
 2 files changed, 8 insertions(+), 10 deletions(-)

Step 6: Pushing release branch...
Enumerating objects: 7, done.
remote: Create a pull request for 'release/v1.2.0' on GitHub by visiting:
remote:   https://github.com/jantielens/esp32-energymon-169lcd/pull/new/release/v1.2.0

=== Release branch created successfully! ===

Next steps:
1. Create PR: release/v1.2.0 → main
   Open: https://github.com/jantielens/esp32-energymon-169lcd/compare/main...release/v1.2.0?expand=1&title=Release%20v1.2.0

2. After PR is reviewed and merged:
   git checkout main
   git pull
   git tag -a v1.2.0 -m "LCD brightness control and stability improvements"
   git push origin v1.2.0

3. GitHub Actions will automatically create the release with binaries
```

#### Step 2: Review Pull Request

1. Open the PR URL provided by the script
2. Review the version bump and changelog changes:
   - ✅ Version numbers correct in `src/version.h`
   - ✅ Changelog section dated properly
   - ✅ All features/fixes documented
3. Wait for CI to validate the build (GitHub Actions runs on PR)
4. Approve and merge the PR

#### Step 3: Tag and Publish

After PR is merged:

```bash
# Switch to main and pull merged changes
git checkout main
git pull

# Create annotated tag
git tag -a v1.2.0 -m "LCD brightness control and stability improvements"

# Push the tag to trigger release workflow
git push origin v1.2.0
```

**What happens automatically (GitHub Actions):**

1. **Build Stage** (5-10 minutes):
   - Installs arduino-cli and dependencies
   - Builds firmware for ESP32 DevKit V1 → `build/esp32/app.ino.bin`
   - Builds firmware for ESP32-C3 Super Mini → `build/esp32c3/app.ino.bin`
   - Generates SHA256 checksums

2. **Release Stage**:
   - Extracts changelog section for v1.2.0 using `tools/extract-changelog.sh`
   - Creates GitHub Release with tag `v1.2.0`
   - Uploads firmware binaries:
     - `esp32-energymon-169lcd-esp32-v1.2.0.bin`
     - `esp32-energymon-169lcd-esp32c3-v1.2.0.bin`
   - Uploads `SHA256SUMS.txt`
   - Sets release notes from CHANGELOG.md

3. **Artifacts** (debug/advanced):
   - `.elf` files with debug symbols
   - Build logs

#### Step 4: Verify Release

Check the release page:
```
https://github.com/jantielens/esp32-energymon-169lcd/releases/tag/v1.2.0
```

**Verify checklist:**
- ✅ Release notes match CHANGELOG.md section
- ✅ All firmware binaries present (esp32, esp32c3)
- ✅ SHA256 checksums file included
- ✅ **NOT** marked as pre-release (should be full release)
- ✅ Tag points to correct commit on `main`

## Release Scenarios

### Scenario 1: Hotfix Release

**Use for**: Critical bug fixes requiring immediate release

```bash
# Create hotfix branch from main
git checkout main
git pull
git checkout -b hotfix/v1.2.1

# Make your fixes
# ... edit files ...

# Test thoroughly
./build.sh
./upload.sh esp32c3
./monitor.sh

# Use release script
./create-release.sh 1.2.1 "Fix critical WiFi reconnection bug"

# Follow PR workflow (Steps 2-4 above)
```

**Alternative (manual edits):**
```bash
# Edit src/version.h: VERSION_PATCH 1
# Edit CHANGELOG.md: Add [1.2.1] section with fix details
git add .
git commit -m "fix: critical WiFi reconnection bug"
git push origin hotfix/v1.2.1
```

### Scenario 2: Pre-Release / Beta Testing

**Use for**: Testing release workflow, beta versions, release candidates

**Tag with hyphen suffix:**
```bash
git tag -a v1.3.0-beta.1 -m "Pre-release v1.3.0-beta.1"
git push origin v1.3.0-beta.1
```

**What happens:**
- Same build process as standard release
- Release **automatically marked as "Pre-release"** on GitHub
- Excluded from "Latest Release" badge
- Users can opt-in to test

**Pre-release naming conventions:**
- `v1.3.0-beta.1`, `v1.3.0-beta.2` - Beta versions (feature-complete, testing)
- `v1.3.0-rc.1`, `v1.3.0-rc.2` - Release candidates (code frozen, final testing)
- `v1.3.0-alpha.1` - Alpha versions (early preview, unstable)

**When to use:**
- Testing new major features with community
- Validating release workflow changes
- Getting feedback before stable release

### Scenario 3: Manual Release (Without Script)

**Use for**: Custom workflows, learning the process

```bash
# 1. Create release branch
git checkout -b release/v2.0.0

# 2. Manually edit src/version.h
#   #define VERSION_MAJOR 2
#   #define VERSION_MINOR 0
#   #define VERSION_PATCH 0

# 3. Manually edit CHANGELOG.md
#   Replace: ## [Unreleased]
#   With:    ## [2.0.0] - 2025-12-14
#   Add new [Unreleased] section at top

# 4. Commit and push
git add src/version.h CHANGELOG.md
git commit -m "chore: bump version to 2.0.0"
git push origin release/v2.0.0

# 5. Create PR, review, and merge

# 6. Tag the release
git checkout main
git pull
git tag -a v2.0.0 -m "Release v2.0.0"
git push origin v2.0.0
```

### Scenario 4: Testing Workflow Changes

**Use for**: Validating changes to `.github/workflows/release.yml`

```bash
# Use a test pre-release tag
git tag -a v0.0.0-test.1 -m "Testing release workflow"
git push origin v0.0.0-test.1

# Watch GitHub Actions
# https://github.com/jantielens/esp32-energymon-169lcd/actions

# Verify build artifacts and release creation

# Clean up test release
# Via GitHub UI: Delete the release manually
# Via CLI:
git push --delete origin v0.0.0-test.1
git tag -d v0.0.0-test.1
```

## Automation

### create-release.sh

**Purpose**: Automate version bump and changelog update.

**Usage:**
```bash
./create-release.sh <version> [release-title]
```

**Examples:**
```bash
# Standard release
./create-release.sh 1.2.0 "LCD brightness control"

# Hotfix
./create-release.sh 1.2.1 "Fix WiFi bug"

# Major version
./create-release.sh 2.0.0 "New MQTT protocol"
```

**What it validates:**
- Version format matches `X.Y.Z` or `X.Y.Z-suffix`
- On `main` branch (warns if not)
- No uncommitted changes
- `src/version.h` exists
- `CHANGELOG.md` exists

**What it modifies:**
- Creates branch `release/vX.Y.Z`
- Updates `src/version.h` version macros
- Moves `[Unreleased]` → `[X.Y.Z] - YYYY-MM-DD` in CHANGELOG.md
- Adds new `[Unreleased]` section
- Commits with message: `chore: bump version to X.Y.Z`

**Error handling:**
- Aborts on validation failures
- Shows diffs before committing
- Requires confirmation before push

### tools/extract-changelog.sh

**Purpose**: Extract version-specific changelog section for GitHub Release notes.

**Usage:**
```bash
./tools/extract-changelog.sh <version>
```

**Example:**
```bash
./tools/extract-changelog.sh 1.2.0
```

**Output:**
```
### Added
- LCD brightness control via web portal
- Configurable power color thresholds

### Changed
- Power screen color determination now uses configurable thresholds

### Fixed
- Display color accuracy for anti-aliased graphics
```

**How it works:**
1. Finds `## [1.2.0]` header in CHANGELOG.md
2. Extracts content until next `## [` or `---` separator
3. Removes empty lines
4. Outputs to stdout (used by GitHub Actions)

**Used by**: `.github/workflows/release.yml` to populate release notes.

### GitHub Actions Workflow

**File**: `.github/workflows/release.yml`

**Trigger**: Push of tags matching `v*.*.*`

**Stages:**

1. **Checkout**: Clone repository with full history
2. **Setup**: Install arduino-cli, Python, dependencies
3. **Build**: Run `./build.sh` for all boards
4. **Extract**: Run `./tools/extract-changelog.sh` for version
5. **Release**: Create GitHub Release with binaries

**Environment Variables:**
- `VERSION`: Extracted from tag (e.g., `1.2.0` from `v1.2.0`)
- `PROJECT_NAME`: From `config.sh`

**Artifacts:**
- Firmware binaries (uploaded to release)
- Build logs (workflow artifacts, 90-day retention)
- Debug symbols (`.elf` files in artifacts)

## Troubleshooting

### Version Format Error

**Error:**
```
Error: Invalid version format
Expected format: X.Y.Z or X.Y.Z-suffix
```

**Solution:**
Use semantic version format:
```bash
./create-release.sh 1.2.0       # ✅ Valid
./create-release.sh v1.2.0      # ❌ Invalid (no 'v' prefix)
./create-release.sh 1.2         # ❌ Invalid (missing patch)
./create-release.sh 1.2.0-beta  # ✅ Valid (with suffix)
```

### Not on Main Branch

**Warning:**
```
Warning: Not on main branch (currently on: feature/new-screen)
Continue anyway? (y/N)
```

**Recommendation:**
```bash
# Merge feature to main first
git checkout main
git pull
git merge feature/new-screen
git push

# Then create release
./create-release.sh 1.3.0
```

### Uncommitted Changes

**Error:**
```
Error: You have uncommitted changes
Please commit or stash your changes before creating a release.
```

**Solution:**
```bash
# Commit changes
git add .
git commit -m "feat: new feature"

# Or stash temporarily
git stash

# Then retry
./create-release.sh 1.2.0
```

### Tag Already Exists

**Error (when pushing tag):**
```
error: tag 'v1.2.0' already exists
```

**Solution:**
```bash
# Check existing tags
git tag -l "v1.2.*"

# Delete local tag
git tag -d v1.2.0

# Delete remote tag (if needed)
git push --delete origin v1.2.0

# Create new tag with correct commit
git tag -a v1.2.0 -m "Release v1.2.0"
git push origin v1.2.0
```

### Changelog Extraction Fails

**Error (in GitHub Actions):**
```
Error: CHANGELOG.md section for version 1.2.0 not found
```

**Cause**: CHANGELOG.md doesn't have `## [1.2.0]` header.

**Solution:**
```bash
# Verify CHANGELOG.md format
grep "## \[1.2.0\]" CHANGELOG.md

# Test extraction locally
./tools/extract-changelog.sh 1.2.0

# If missing, update CHANGELOG.md manually and recommit
```

### Build Fails in GitHub Actions

**Error**: Workflow fails during build stage.

**Debug steps:**

1. **Check workflow logs**: https://github.com/jantielens/esp32-energymon-169lcd/actions
2. **Test build locally**:
   ```bash
   ./clean.sh
   ./build.sh
   ```
3. **Check dependencies**: Verify `arduino-libraries.txt` is up to date
4. **Retry workflow**: Re-run failed jobs in GitHub UI

### Release Not Created

**Issue**: Tag pushed but no release appears.

**Check:**
1. **Workflow triggered?** https://github.com/jantielens/esp32-energymon-169lcd/actions
2. **Tag format correct?** Must match `v*.*.*` pattern
3. **Permissions**: GitHub Actions has release write permissions

**Manual release creation:**
```bash
# If workflow failed, create release manually via GitHub UI
# https://github.com/jantielens/esp32-energymon-169lcd/releases/new
# Select tag: v1.2.0
# Upload binaries from local build/
```

## Related Documentation

- [Building from Source](building-from-source.md) - Build system and scripts
- [Multi-Board Support](multi-board-support.md) - Board variants
- [Web Portal API](web-portal-api.md) - API versioning considerations

---

**Questions?** Check GitHub Actions logs or open an issue on the repository.
