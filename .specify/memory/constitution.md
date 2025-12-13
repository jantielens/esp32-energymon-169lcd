<!--
SYNC IMPACT REPORT - Constitution v1.0.0
========================================
Version Change: INITIAL → 1.0.0 (Initial ratification)
Principles Established:
  - I. Hardware-Driven Design (NEW)
  - II. Configuration-First User Experience (NEW)
  - III. Build Automation & Multi-Board Support (NEW)
  - IV. Embedded Systems Best Practices (NEW)
  - V. Semantic Versioning & Documentation (NEW)

Templates Requiring Updates:
  ✅ .specify/templates/plan-template.md - Updated Technical Context section
  ✅ .specify/templates/spec-template.md - Compatible with acceptance testing approach
  ✅ .specify/templates/tasks-template.md - No changes needed (generic task structure)
  ✅ .specify/templates/agent-file-template.md - No changes needed (generic structure)
  ✅ .specify/templates/checklist-template.md - No changes needed (generic checklist)

Follow-up Actions:
  - All constitution principles are now concrete and testable
  - Project-specific constraints documented in Hardware & Performance Standards section
  - Development workflow aligned with existing build/release process
-->

# ESP32 Energy Monitor Constitution

## Core Principles

### I. Hardware-Driven Design

**All features MUST respect hardware constraints and capabilities:**

- Physical display dimensions (240×280 pixels) determine UI layout decisions
- MQTT subscription design driven by external data sources (Home Assistant topics)
- Power measurement accuracy constrained by upstream sensor precision
- Memory budgets (heap, PSRAM, flash) MUST be validated during implementation
- GPIO pin assignments MUST be documented per board variant
- SPI/I2C clock speeds MUST be tested and validated on target hardware

**Rationale**: Embedded systems cannot abstract away hardware. Design decisions that ignore physical constraints lead to non-functional products. Every feature specification must include hardware compatibility verification.

### II. Configuration-First User Experience

**The device MUST be usable without USB serial access after initial flash:**

- First-boot MUST present captive portal AP (192.168.4.1) when WiFi unconfigured
- All user-facing settings MUST be exposed via web portal (Network, Home, Firmware pages)
- Configuration persistence via NVS MUST survive power loss and firmware updates
- REST API (`/api/*`) MUST provide programmatic access to all configuration
- Web portal MUST work on mobile devices (responsive design with <768px breakpoint)
- Error states MUST be visible to end users (connection failures, invalid config, etc.)

**Rationale**: IoT devices deployed in homes cannot require developer tools for configuration. Users must be able to set WiFi credentials, MQTT brokers, and custom settings through a simple web interface accessible from their phone.

### III. Build Automation & Multi-Board Support

**Build scripts MUST support headless CI/CD and multi-board compilation:**

- All builds MUST use `arduino-cli` installed locally to `./bin/` (no system dependencies)
- Board variants defined in `config.sh` `FQBN_TARGETS` array MUST compile in parallel
- Board-specific configuration via `src/boards/[board-name]/board_overrides.h` pattern
- GitHub Actions workflows MUST use identical build scripts as local development
- Firmware binaries MUST be named `[board-name].bin` and placed in `build/[board-name]/`
- Library dependencies in `arduino-libraries.txt` MUST be version-pinned
- `./build.sh` exit code MUST be non-zero on compilation failure

**Rationale**: Multi-board ESP32 projects require consistent build behavior across local machines and CI. Manual compilation steps or platform-specific tooling breaks automation and reproducibility.

### IV. Embedded Systems Best Practices

**Code MUST follow Arduino/ESP32 platform constraints:**

- No dynamic memory allocation in interrupt handlers or LVGL callbacks
- Blocking operations (network I/O, file I/O) MUST NOT occur in display flush callbacks
- WiFi reconnection MUST use non-blocking retry with exponential backoff (5s minimum)
- LVGL tick MUST be called every 10ms for 60 FPS target
- Serial logging MUST use consistent baud rate (115200) across all boards
- Watchdog timers MUST be configured to recover from infinite loops
- NVS namespace collisions MUST be prevented via unique prefixes per component

**Rationale**: Embedded firmware failures are difficult to debug remotely. Following platform-specific best practices prevents common failure modes like watchdog resets, display stuttering, and memory corruption.

### V. Semantic Versioning & Documentation

**Version numbers and changelogs MUST track breaking changes:**

- `src/version.h` defines MAJOR.MINOR.PATCH per Semantic Versioning 2.0.0
- MAJOR bump: Breaking changes to web API, NVS schema, or hardware requirements
- MINOR bump: New features (new screens, MQTT topics, configuration fields)
- PATCH bump: Bug fixes, performance improvements, documentation updates
- `CHANGELOG.md` MUST follow Keep a Changelog format with dated releases
- GitHub releases MUST include firmware binaries for all supported boards
- Breaking changes MUST include migration guide in `CHANGELOG.md`

**Rationale**: Over-the-air firmware updates require users to understand compatibility. Semantic versioning signals when a firmware update requires reconfiguration or hardware changes.

## Hardware & Performance Standards

**Display Performance:**
- LVGL rendering MUST maintain 60 FPS average (measured via built-in FPS counter)
- Display buffer sizes: 48KB heap allocation, 24KB layer buffer (validated on ESP32/ESP32-C3)
- SPI clock: 60 MHz for ST7789V2 (validated stable, no visual artifacts)

**MQTT Performance:**
- Packet buffer: 512 bytes minimum for JSON grid topic parsing
- Reconnection backoff: 5 seconds (prevents broker rate limiting)
- Topic update latency: <2 seconds from MQTT publish to display update

**Flash Memory:**
- Partition schemes: ESP32 uses `min_spiffs`, ESP32-C3 uses `huge_app`
- Target flash usage: <85% to allow OTA updates (requires 2× firmware size)
- LVGL font/image assets MUST use PROGMEM for flash storage

**Power Consumption:**
- Display backlight PWM MUST be user-configurable (web portal)
- WiFi power save mode allowed when MQTT update interval >10 seconds
- Deep sleep NOT required (AC-powered use case assumed)

## Development Workflow

**Code Review Requirements:**
- All changes via pull requests (no direct commits to `main`)
- PR MUST pass GitHub Actions build for all board targets
- Breaking changes require CHANGELOG.md update before merge
- New configuration fields require web portal UI + REST API + NVS persistence

**Testing Strategy:**
- Build verification: `./build.sh` exit 0 for all boards
- Manual testing on physical hardware required for display/MQTT features
- Configuration persistence: Test via power cycle after NVS write
- WiFi recovery: Test by power cycling router during MQTT connection

**Release Process:**
1. Update `src/version.h` with new version number
2. Update `CHANGELOG.md` with release notes and date
3. Create Git tag `v[MAJOR].[MINOR].[PATCH]`
4. Push tag to trigger GitHub Actions release workflow
5. Verify firmware binaries attached to GitHub release

## Governance

**This constitution supersedes all other development practices.**

All pull requests MUST verify compliance with:
- Hardware-driven design: Feature specs include pin assignments and memory budgets
- Configuration-first UX: No features requiring serial console for end users
- Build automation: Changes compile successfully on all board targets
- Embedded best practices: No blocking operations in callbacks, proper watchdog handling
- Semantic versioning: Version bumps match change type, CHANGELOG.md updated

**Amendments to this constitution require:**
1. Documented rationale for change
2. Migration plan for existing codebases (if applicable)
3. Update to `.specify/templates/*` and `.github/copilot-instructions.md`
4. Version bump (MAJOR for removed principles, MINOR for additions, PATCH for clarifications)

**Runtime development guidance** is maintained in `.github/copilot-instructions.md` and provides implementation details for the principles defined here.

**Version**: 1.0.0 | **Ratified**: 2025-12-13 | **Last Amended**: 2025-12-13
