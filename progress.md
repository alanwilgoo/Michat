# Progress Log

## Session: 2026-09-10 — Repository slimming

- **Status:** in progress
- The initial Git commit contains 2,801 files, including 2,773 under `third_party/lvgl`; its loose Git objects total about 69.5 MiB.
- `build/` (about 15 MiB) and `.vscode/` are already ignored and were not committed.
- User requested reorganizing before the first successful GitHub upload. The chosen direction is a pinned official LVGL submodule, retaining self-contained reproducibility through `git submodule update --init --recursive`.
- Verification found the terminal upload completed after the screenshot: `origin/main` is commit `75792b2`. No slimming rewrite has been applied; replacing that history requires an explicit force-push decision.
- User approved the history rewrite. A backup branch `backup/before-lvgl-submodule` was created locally; the vendored LVGL tree was replaced by a gitlink to official LVGL `v9.2.2` commit `7f07a129e8d77f4984fff8e623fd5be18ff42e74` and `.gitmodules` records the source URL/branch.
- A new root commit `073bb16` was created with 30 tracked entries and force-pushed with an exact lease. GitHub accepted `75792b2...073bb16 slim-main -> main (forced update)`, so the oversized tree is absent from remote `main` history.
- The source directory remains usable after `git submodule update --init --recursive`; the local submodule working tree is intentionally absent until that public dependency clone is run.

## Session: 2026-09-10 — Server repository integration

- **Status:** in progress
- Inspected `/home/alan/miser`: source files are `server.c`, `list.c`, and `list.h`; executables, `users.txt`, `log`, and `emoji_1` are runtime/generated data and will not be copied into the repository.
- The current server source uses a hard-coded account path. The repository subproject will use ignored `server/data/users.txt` by default and `CHAT_USERS_FILE` for an explicit deployment path, leaving the running external service untouched.
- The Makefile builds `build/michat-server` successfully with warning flags. A bounded username copy was corrected so the build is warning-free. The first test cleanup command lost its background PID during command transport; no test process remained.
- Final verification: `make` produced `server/build/michat-server`, a timed run announced port 10005, and both the binary and `server/data/users.txt` matched the server-local ignore rules. The root README now documents the client/server/submodule layout.
- Committed and pushed `38ce980 Add versioned server subproject` to `origin/main`. Only source, build instructions, documentation, and an empty runtime-data placeholder were included; the external `/home/alan/miser` deployment remains unchanged.

## Session: 2026-09-08

### Phase 1: Discovery
- **Status:** complete
- Actions taken:
  - Recorded the requested UI and authentication fixes.
  - Captured the supplied screenshot’s visible footer text.
  - Located authentication UI/event handling in `frontend.c` and identified absolute-positioned registration layout as the likely overlap source.
  - Confirmed confirmation field/button overlap and inspected the backend authentication protocol.
  - Removed the authentication footer wording/theme control and reorganized login/registration controls so they do not overlap.
  - Rebuilt `chat_sim` successfully with the native WSL GCC preset.
  - Ran a dummy-SDL authentication smoke test and visually checked the captured 960×640 registration page.
  - Diagnosed the reported “Request sent…” stall as a missing/unrecognized authentication response after the socket send succeeds.
  - Confirmed the configured server port is reachable and that the original copied client contains no login/register protocol reference.
  - Inspected the supplied server source and probed the live port with an invalid login; the live service produced no expected reply.
  - First integration-test compile exposed an ambiguous client attachment `else`; corrected it before retrying.
  - Second strict test compile exposed a feature-macro redefinition; guarded the backend definition before the final retry.
  - Third test attempt did not receive an auth callback because the detached temporary server had exited; using a single-process harness next.
- Files created/modified:
  - task_plan.md
  - findings.md
  - progress.md

## Test Results
| Test | Expected | Actual | Status |
|------|----------|--------|--------|
| Registration/login callbacks | Equal confirmation invokes registration; same credentials invoke login | Passed | ✓ |
| Registration layout | Confirmation field and mode button do not overlap | Passed; visual capture checked | ✓ |
| Application smoke run | Build and create an LVGL frame with SDL dummy driver | Passed; 960×640 frame captured | ✓ |

### Authentication response diagnosis
- **Status:** complete
- The UI shows “Request sent…” only after writing to the connected socket succeeds.
- The client waits indefinitely because it accepts only four exact uppercase response strings, while the original client has no authentication protocol and no server source is available in this workspace.
- The supplied server source matches those four response strings, but its old binary was still running. Rebuilding and restarting it restored immediate responses; an isolated protocol check returned `REGISTER_OK`, `LOGIN_OK`, and `LOGIN_FAIL` as expected.

### Project constraints
- Recorded the user’s highest-priority compatibility and frontend/backend separation constraints for all subsequent work.

### Automatic client ports
- **Status:** complete
- Changed the client connection setup so an absent `CHAT_LOCAL_PORT` binds to TCP port `0`, asks the OS for an available port, obtains the selected port after connecting, and supplies it through the existing legacy session-registration command.
- Explicit `CHAT_LOCAL_PORT` values remain supported for manual compatibility testing.
- Verified `backend.c` with `-Wall -Wextra -Werror`, rebuilt `chat_sim` through the Native WSL GCC preset, and ran two simultaneous client processes against an isolated server. The OS assigned distinct ports 34177 and 41809; the server observed and cleaned up both corresponding sessions.
- In the same isolated test, server-owned registration and login returned `REGISTER_OK` and `LOGIN_OK`. Test account data and the temporary service were removed afterward.

### Concurrent launch diagnosis
- **Status:** complete
- The supplied screenshot confirmed that the app was still displaying the pre-connection fallback title `Michat - Client 10086`, and repeated VS Code run requests were queued behind the active foreground task.
- Updated the VS Code run task to allow ten concurrent instances in separate terminals. Added a thread-safe local-port event so the title changes from `Michat - Connecting...` to `Michat - Client <actual-port>` after connection.
- Parsed the task JSON successfully, rebuilt `chat_sim`, and ran two concurrent instances against an isolated server. The clients reported ports 45105 and 39125; the server observed the exact same two ports.

### CMake Tools launch-button diagnosis
- **Status:** complete
- The new screenshot identifies a separate CMake Tools status-bar launch path, so the task-level concurrency configuration cannot affect the button the user is pressing.
- Inspected the installed CMake Tools 1.23.52 schema and implementation, then configured `cmake.launchBehavior` to `newTerminal` in the workspace settings. This maps directly to the user’s bottom-right launch-target button.
- Validated the workspace JSON and confirmed from the installed extension implementation that `newTerminal` bypasses the branch that reuses a launch terminal. The desktop automation surface did not expose a VS Code window, so the final visible check requires reloading the user’s VS Code window.

### Chat image bubble work
- **Status:** complete
- Enabled LVGL's existing stdio file-system driver and bundled PNG, JPEG, and BMP decoders, then added only the related LVGL source groups to the existing static library build.
- Added bounded chat-image bubbles for recognized PNG/JPG/JPEG/BMP attachments after a successful send and after the existing receive callback. Existing attachment commands, TCP transfer, server handling, and regular-file fallback behavior are unchanged.
- Rebuilt `chat_sim` successfully. A temporary decoder check read the bundled PNG as 50×50, and a temporary UI check simulated the existing received-emoji callback and verified it created a 50×50 LVGL image bubble. Both temporary checks and binaries were removed.

### Image preview and online-list follow-up
- **Status:** complete
- The reported no-preview/stale-port behavior is being reproduced against the actual CMake Tools working-directory model and an isolated service before making targeted corrections.
- A real `chat_sim` run against an isolated attachment stream captured the same fallback UI. Inspection proved the transferred bytes were unchanged but the supplied `b.png` file is actually a JPEG. This is an image-content/extension mismatch, not a send failure.
- Verified normal disconnect cleanup with two isolated valid accounts: client `41001` disconnected, and the next `getlist` response contained only client `41002`. The test service and account data were deleted.
- Updated the enabled LVGL PNG/JPEG decoders to use the actual bytes and valid local path under the configured `A:` file system. A real `chat_sim` screenshot then showed the formerly failing `b.png` attachment as an image bubble with the status `Image received`.
- Replaced server disconnect cleanup with exact-node removal, rebuilt the client and server with warnings as errors where supported, and restarted the active server. Port 10000 is listening through the rebuilt service (PID 22630). All temporary image/online-list test data was removed.

### RK3568 cross-build result
- **Status:** package complete; LAN relay requires Windows administrator elevation.
- Rebuilt the unchanged desktop target successfully, then built `michat-rk3568` as an ARM64 ELF. It needs only the board-standard `libpthread.so.0`, `libm.so.6`, and `libc.so.6`; the confirmed `/lib/ld-linux-aarch64.so.1` loader is compatible.
- Packaged the board tree as `build-rk3568/michat-rk3568.tar.gz`. The launcher defaults to the verified Goodix touch device `/dev/input/event6`, while every data path is on the board under `/opt/michat`.
- The attempted host relay was rejected solely because `netsh` needs administrator elevation. No server source, server binary, or server configuration was changed.

### RK3568 touch UI follow-up
- Added target-only compact keyboard behavior: no automatic focus, a 170px keyboard, and explicit hide handling for keyboard cancel/confirm actions.
- Added an initial whole-screen LVGL redraw for the framebuffer target to clear retained pixels from the board's prior framebuffer content.
- Both desktop and ARM64 targets rebuilt successfully. The board's active client was restored as PID 867 after serial-console recovery; the fixed ARM64 archive is ready locally but has not replaced the board binary because the board Ethernet link is down and raw serial transfer triggers the Rockchip FIQ debugger.

## Error Log
| Error | Resolution |
|-------|------------|
|       |            |

### Dedicated emoji picker
- **Status:** complete
- Added `assets/emojis/` with usage documentation. The chat sidebar now opens a dedicated picker backed by that folder; ordinary text composition and ordinary file sending are unchanged.
- The picker uses a dedicated `backend_send_emoji_to` API and the pre-existing independent `emoji` command. The standard attachment byte stream is reused only as transport, so no text-message or ordinary-file protocol semantics were changed.
- Native WSL GCC build, four-item picker enumeration, and independent-command transport checks all passed. Temporary test harnesses were removed.

### Emoji delivery and image layout follow-up
- **Status:** complete
- The receiver was saving transferred files but the UI had no sender identity and therefore routed incoming attachment bubbles to the hidden shared inbox. The server now forwards sender identity in attachment headers; the updated client carries it through backend callbacks and the main-thread event queue into the correct chat session, with legacy header compatibility retained.
- Image source loading now precedes setting the bounded display box and stretch mode. Images preserve their complete aspect ratio inside a 280x210 maximum bubble; source-file intake remains limited to 5 MB.
- Client/server builds and isolated attachment-route/image-fit checks passed. The rebuilt server is listening on port 10000; existing clients need to reconnect after the server restart.

### Emoji asset normalization
- **Status:** complete
- Converted the four current picker assets to non-interlaced PNG files with aspect ratio preserved and a 280x210 maximum display size. This avoids progressive-JPEG and misleading-extension issues in the client decoder.
- Originals are recoverable in `assets/emojis/originals/`; root-level processed images are the only files the picker scans. The asset README now documents the normalized format and backup arrangement.
- Verified all four final assets through the current LVGL decoder and rebuilt `chat_sim` successfully. Temporary verification files were removed.

### Emoji preview integrity repair
- **Status:** complete
- Reproduced the blank/corrupted image bubble and traced a crash to `lv_lodepng.c` copying from an `lv_draw_buf_t` descriptor as if it were raw RGBA bytes.
- Restored the bundled LVGL 9.2 ownership model and unified PNG reads on LVGL's `A:` virtual-drive path. Removed the conflicting host-file/path-stripping approach.
- Added a targeted image-row layout update so a newly added bounded image bubble is fully scrolled above the composer.
- Rebuilt `chat_sim` and verified `smile.png` (160x160 RGBA), `laugh.png` (240x240 RGBA), `cute.png` (136x128 palette), and `g.png` (210x210 RGB) with the actual LVGL decoder.
- Dummy-SDL visual checks confirmed complete picker thumbnails and clear sender/receiver image bubbles. The temporary smoke harness and outputs were removed after verification.

### RK3568 deployment
- **Status:** in progress
- Connected to the board through USB-SERIAL CH340 on COM5 at 1500000 baud and performed read-only environment checks.
- Confirmed RK3568/Buildroot/AArch64 hardware, DRM and framebuffer devices, sufficient memory/storage, and Ethernet connectivity. Confirmed SDL2 runtime exists but the board lacks a native toolchain and visible Wayland/X11 session.
- Confirmed the WSL server is currently listening on port 10000 but is inaccessible from the board because WSL's NAT address is not routed from the Ethernet LAN. Next: inspect the available cross-toolchain/sysroot and implement the ARM64 deployment build.

### Desktop chat avatars
- **Status:** complete
- Added desktop-only WeChat-style message rows: received messages are grouped beside a left-side peer avatar, and sent messages beside a right-side `ME` avatar. The user name remains above each message bubble, while text, image/emoji preview, routing, and file-transfer behavior are unchanged.
- The desktop target defines `MICHAT_DESKTOP_AVATARS`; framebuffer/RK3568 builds do not, preserving their existing layout. Reconfigured and rebuilt `chat_sim` successfully.

### Project Directory Reorganization
- **Status:** complete
- Moved the server project to `/home/alan/miser` and the client project to `/home/alan/michat/lvgl_chat`; existing `users.txt` moved intact (matching SHA-256 before and after).
- Server credential storage now uses `/home/alan/miser/users.txt`, independent of the shell working directory. Built the replacement server and verified it can listen on an isolated port. The active port-10000 process was deliberately left running to preserve active client connections; it remains healthy and its working directory now resolves to `/home/alan/miser`.
- Regenerated and rebuilt both `build/bin/chat_sim` (x86-64 desktop) and `build-rk3568/bin/michat-rk3568` (ARM64) from the new project directory. Their CMake caches contain no old source path.

### RK3568 cleanup
- **Status:** complete
- Removed the project-local ARM64 build package (`build-rk3568/`), framebuffer entry point (`main_fbdev.c`), RK3568 platform configuration/launcher (`platform/rk3568/`), and the matching CMake target. Also removed the framebuffer-specific on-screen keyboard branches from the shared frontend.
- Reconfigured and rebuilt the desktop `chat_sim` target successfully after cleanup. The external 757 MB `/home/alan/rk3568` toolchain remains untouched because it is outside this project and may be shared.

### Acceptance summary
- **Status:** complete
- Created and source-checked `Michat_项目验收总结.md` for teacher acceptance. It covers the requested design objectives, functional description, design scheme, Mermaid system framework, implementation process, test points, reflections, and source-file mapping, based on the current desktop client and independent server.
- Verified all required headings, the Mermaid framework diagram, current `REGISTER`/`LOGIN` protocol facts, and the absence of removed-board implementation content from the report.

### Username-based online list and chat labels
- **Status:** complete
- Added a server-side username field to each online connection. A successful login binds the validated username, and an online-list item is now sent as `username@ip:port`.
- The desktop client renders that item as `username  ip:port`, but continues to use IP/port for target selection and message routing. The same stored name is used in the session list, session title, and incoming message/avatar label; legacy or unknown senders fall back to IP/port.
- Rebuilt `chat_sim` and the server with warnings treated as errors. Replaced and restarted the live port-10000 server; a live, credential-redacted protocol check confirmed authenticated username-list output.

### Acceptance architecture figure
- **Status:** complete
- Created `Michat_系统框架图.svg`, a standalone vector diagram suitable for inclusion in the acceptance summary or direct printing. It depicts the current LVGL/SDL2 client modules, TCP/event flow, and independent server authentication, online-list, and routing modules.
- Validated the SVG as well-formed XML. The environment has no SVG-to-PNG utility installed, so the lossless SVG is retained as the delivered format.

### Graceful server shutdown
- **Status:** complete
- The server's `accept()` loop had no stop condition. Added a SIGINT/SIGTERM handler that sets a stop flag and closes the listening socket to wake the blocking accept call; SIGPIPE is ignored so a broken client connection is handled by normal send error paths rather than terminating the process.
- Built with warnings treated as errors, verified graceful SIGTERM shutdown on isolated port 10004, then replaced and restarted the live port-10000 service. The current live server listens as PID 23653.
