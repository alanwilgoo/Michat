# Task Plan: Registration UI and Authentication Fix

## Goal
Remove the registration-page attachment wording, make its layout usable, make registration/login work correctly, and diagnose stalled authentication requests.

## Non-Negotiable Constraints
- Preserve all verified chat, file, emoji, online-list, and point-to-point message behavior.
- Reuse the existing TCP connection, threads, UI framework, data structures, event handling, build setup, and file organization.
- Keep authentication separate from chat protocol semantics and modular for future account features.
- Keep account storage and credential validation exclusively on the server; never hard-code or persist an account database in the client.
- Do not make broad chat-module refactors to add authentication.

## Next Step
Commit and push the verified `server/` subproject while preserving unrelated local client changes.

## Current Phase
Phase 21: Git Repository Slimming

## Phases

### Phase 21: Git Repository Slimming
- [x] Check the in-progress initial upload and remote branch state.
- [x] Replace the vendored `third_party/lvgl` tree with a pinned official LVGL Git submodule.
- [x] Rewrite the uploaded initial history and force-push the smaller first upload safely.
- [x] Verify the remote tree contains 30 tracked entries, with LVGL recorded as the pinned `v9.2.2` gitlink.
- [ ] Initialize the submodule locally and rebuild when the public LVGL clone completes.
- **Status:** in progress

### Phase 22: Versioned Server Subproject
- [x] Copy only server source into a new repository `server/` subproject, preserving the running `/home/alan/miser` deployment.
- [x] Add a reproducible build entry point and document configuration/run steps.
- [x] Move the default account-data location to ignored project-local runtime data, with an environment override.
- [x] Build the new server target and verify ignored credentials/binaries before committing.
- [ ] Commit and push only the server and repository-documentation changes.
- **Status:** in progress

### Phase 15: Project Directory Reorganization
- [x] Move the server project to the independent `/home/alan/miser` directory while preserving server data.
- [x] Rename the desktop project to `/home/alan/michat/lvgl_chat` and update persistent path references.
- [x] Regenerate the relocated desktop and ARM64 build outputs.
- **Status:** complete

### Phase 16: Remove RK3568 Support
- [x] Delete the RK3568 framebuffer entry point, target configuration, launcher, and generated ARM64 package from this project.
- [x] Remove board-only build and touch-keyboard branches while preserving the desktop UI path.
- [x] Reconfigure and rebuild the desktop client.
- **Status:** complete

### Phase 17: Acceptance Summary
- [x] Extract the current client, server, protocol, build, and verification facts.
- [x] Create a structured Markdown acceptance summary with a system-framework diagram.
- [x] Review the report against the current source tree for accuracy and desktop-only scope.
- **Status:** complete

### Phase 18: Username-based Online List and Chat Labels
- [x] Bind the authenticated server-side username to each online client connection.
- [x] Return username plus endpoint in the online-list response while retaining endpoint routing.
- [x] Display usernames in the desktop online list, session labels, titles, and incoming message avatar labels.
- [x] Build both programs and validate the response format on the live service.
- **Status:** complete

### Phase 19: Acceptance Architecture Figure
- [x] Create a standalone vector system-framework figure based on the current desktop/client-server design.
- [x] Validate that the SVG is well formed.
- **Status:** complete

### Phase 20: Graceful Server Shutdown
- [x] Diagnose the non-terminating accept loop and the effect of the background launcher.
- [x] Add SIGINT/SIGTERM shutdown handling without changing protocol behavior.
- [x] Compile and verify the server exits promptly on SIGTERM.
- **Status:** complete

### Phase 1: Discovery
- [x] Locate the registration UI, text, and authentication state flow.
- [x] Identify why credentials are reported as mismatched.
- **Status:** complete

### Phase 2: Implementation
- [x] Correct layout, wording, and registration/login handling.
- **Status:** complete

### Phase 3: Verification
- [x] Build and run focused authentication checks.
- **Status:** complete

### Phase 4: Delivery
- [x] Confirm requirements and report changed files.
- **Status:** complete

### Phase 5: Authentication Response Diagnosis
- [x] Identify the source of the stalled authentication status.
- [x] Obtain the server command/reply format before changing the protocol.
- [x] Verify service registration/login response handling in isolation and on the live port.
- **Status:** complete

### Phase 6: Future Authentication Work
- [x] Apply the non-negotiable client/server separation constraints to every future change.
- [x] Keep future account capabilities as modular authentication commands.
- [x] Replace the fixed default client bind port with operating-system allocation, while retaining the existing chat registration handshake.
- [x] Verify simultaneous clients receive distinct local ports and authentication remains server-authoritative.
- **Status:** complete

### Phase 7: Concurrent VS Code Client Launches
- [x] Remove the run-task single-instance queue so each run starts a separate GUI process.
- [x] Update the window title after connection with the actual OS-assigned local port.
- [x] Build and verify concurrent launch behavior.
- **Status:** complete

### Phase 8: CMake Tools Status-Bar Launch Button
- [x] Identify the launch path used by the right-bottom CMake Tools button.
- [x] Configure it to start each GUI client without serializing subsequent clicks.
- [x] Verify the workspace setting is valid and causes CMake Tools to bypass its reuse-terminal branch.
- **Status:** complete

### Phase 9: Chat Attachment Image Bubbles
- [x] Reuse the existing attachment transfer callback to identify received image files safely.
- [x] Add a bounded, message-bubble image preview without changing any network command.
- [x] Show the sender's selected image locally after successful transfer.
- [x] Verify image decode/rendering and preserve regular file attachment behavior.
- **Status:** complete

### Phase 10: Image Preview and Online Session Corrections
- [x] Make image preview paths resolve consistently from CMake Tools launches.
- [x] Reproduce and correct stale offline entries in the server online list.
- [x] Verify image display and disconnect/list-refresh behavior without changing chat protocol semantics.
- **Status:** complete

### Phase 11: Dedicated Emoji Picker
- [x] Add a documented project-local emoji directory without changing user test files.
- [x] Add an emoji-picker button and image grid to the chat UI.
- [x] Send picker selections through a dedicated emoji client API and the existing `emoji` command.
- [x] Build and perform an isolated picker/send smoke check.
- **Status:** complete

### Phase 12: Emoji Delivery and Image Layout Follow-up
- [x] Trace the complete emoji command path from sender through server to receiver.
- [x] Reproduce and correct the receiving UI/transport failure.
- [x] Ensure large image bubbles show the full scaled image within a bounded display area.
- [x] Build and test both sender and receiver behavior.
- **Status:** complete

### Phase 13: Emoji Asset Normalization
- [x] Inspect the actual emoji assets and identify incompatible encodings/dimensions.
- [x] Preserve original files in a recoverable backup directory.
- [x] Convert root-level picker assets to client-decodable, aspect-ratio-bounded PNG files.
- [x] Verify every converted file with the project's LVGL decoder and rebuild the client.
- **Status:** complete

### Phase 14: Emoji Preview Integrity Repair
- [x] Review current client/server emoji changes and identify incomplete or conflicting edits.
- [x] Reproduce the corrupted image behavior on both sender and receiver.
- [x] Correct the PNG decoder ownership/path mismatch without changing transport semantics.
- [x] Build and verify picker plus sender/receiver emoji rendering with the new PNG assets.
- **Status:** complete

### Phase 15: RK3568 framebuffer deployment
- [ ] Inspect the available ARM64 toolchain and match it to the RK3568 Buildroot runtime.
- [x] Add a dedicated ARM64 toolchain and framebuffer client target, keeping the SDL desktop target unchanged.
- [x] Make client asset, send-file, and received-file paths relocatable under `/opt/michat` on the development board.
- [x] Build and validate an ARM64 executable and runtime package without changing chat protocol semantics.
- [x] Deploy the verified ARM64 package to `/opt/michat` and start the framebuffer client on the RK3568.
- [ ] Expose the existing WSL server on the LAN IP for the RK3568 and validate TCP access from the board.
- [x] Add desktop-only, WeChat-style sender/recipient avatars without changing the embedded client layout or any chat protocol.
- **Status:** in progress

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Preserve existing chat behavior | Scope is registration UI and authentication only. |
| Remove authentication footer controls | The supplied screenshot identifies “Terms of use. Privacy policy” and “Theme” as wording to remove. |
| Separate registration controls vertically | The confirmation input previously overlapped the mode buttons, causing false password mismatch reports. |
| Report authentication timeouts explicitly | A successfully written socket request is not proof that the server recognized it. |
| Do not guess server protocol | Registration is state-changing; exact server commands/replies are required for a safe compatible fix. |
| Build integration checks with warnings enabled | It exposed and corrected an ambiguous attachment receive `else` in the client. |
| Use port zero for the client bind when no local port is configured | The OS atomically chooses an available ephemeral port for each client process, avoiding fixed-port collisions without changing any chat command. |
| Use LVGL's built-in file decoders | PNG/JPEG/BMP previews require no new third-party runtime and stay confined to the desktop UI build. |
| Fall back to the existing attachment message | A non-image, over-5-MB, or undecodable attachment retains its existing save-and-text behavior. |
| Keep LodePNG on LVGL draw buffers and `lv_fs` paths | This matches the bundled LVGL 9.2 decoder contract and avoids treating a draw-buffer descriptor as raw RGBA pixels. |
| Retain the existing server process and port | The RK3568 client must use a LAN-reachable forwarding address, while the current server remains the single authority for authentication and chat routing. |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
|       | 1 |            |
| Mermaid block extraction command | 1 | Shell interpreted Markdown backticks during a verification command; used line-range inspection without backticks instead. The report file was unaffected. |
| First isolated username-list check | 1 | The shell test lost credential variables during command transport; replaced it with a credential-streaming test that never exposes account data. |
| Second isolated username-list check | 2 | The host shell preprocessed `awk` field references; replaced the command with `sed`-based streaming on a new temporary port. |
| SVG renderer availability | 1 | `xmllint`, `rsvg-convert`, and CairoSVG are not installed. Kept the requested vector SVG and validated it with Python's standard XML parser instead. |
| Integration test warning-as-error | 1 | Added explicit attachment receive braces and enabled POSIX declarations in the temporary test. |
| `_DEFAULT_SOURCE` macro redefinition | 2 | Guarded the backend feature macro so direct test builds and normal builds both work. |
| Isolated auth callback assertion | 3 | The separately launched temporary server exited before a stable client run; switch to a single-shell server/client harness with captured logs. |
| Strict standalone UI check warnings | 1 | The temporary test included pre-existing `frontend.c` unused-code warnings; reran the same UI check without converting unrelated warnings to errors. |
| Server restart script port-filter syntax | 1 | A Windows line-ending issue stopped the scripted health check after the old server exited; immediately started the rebuilt binary with a simpler command and verified it listens on port 10000 (PID 22630). |
| Server test-process cleanup | 1 | The host command transport stripped the background PID variable. The shell exited and no test server remained; use a foreground, line-buffered timeout check instead. |
