# Findings & Decisions

## Requirements
- Remove the attachment-screenshot wording from the registration page.
- Rework the overlapping registration UI.
- Fix registration and login so matching credentials are accepted.
- Preserve existing chat behavior and protocol semantics while extending authentication only as independent commands.
- Keep all credential storage and truth decisions on the server.

## Visual Findings
- The supplied screenshot shows the registration page footer text: “Terms of use. Privacy policy” and “Theme”.
- User reports the registration controls overlap.

## Research Findings
- `frontend.c` owns the authentication page and performs a local password/confirmation comparison before sending registration.
- `frontend.c` creates the authentication view with fixed absolute positions and fixed decorative blocks sized for a taller screen; this is likely the overlap source.
- The attachment wording does not appear in the authentication logic found so far; the exact label needs focused inspection of the UI constructor.
- `backend.c` exposes separate `backend_login` and `backend_register` calls; their protocol and callback handling require inspection.
- The overlap is confirmed: confirmation input occupies y=393–431 while the registration/login switch buttons begin at y=404.
- Authentication requests are currently serialized as uppercase `LOGIN@user@password` and `REGISTER@user@password`, while server responses are only recognized as uppercase underscore tokens (`LOGIN_OK`, `REGISTER_OK`, etc.).
- The local confirmation check is correct in isolation. The overlapping confirmation field can cause input to go to another control, leading to a false mismatch message.
- The supplied screenshot’s strings are implemented as the authentication footer controls in `frontend.c`; both the terms/privacy label and Theme button were removed.
- Registration mode now has a labelled confirmation field, a divider, a separate mode switch below it, and hides the irrelevant Forgot Password control.
- Visual smoke capture confirms the three registration fields, switch action, and submit button are vertically separated; the supplied footer text is absent.
- Screenshot shows the request passed local validation and the socket write returned success. It is waiting for a backend authentication event, not stuck in the LVGL UI.
- The receiver ignores every server response except the exact uppercase strings `LOGIN_OK`, `LOGIN_FAIL`, `REGISTER_OK`, and `REGISTER_FAIL`. A server using lowercase, delimiter-based replies, or no auth replies will leave the UI indefinitely at “Request sent…”.
- The configured server port is reachable, so this is not a local SDL or basic TCP-connect failure.
- The copied original client contains no account-authentication protocol. The standalone UI added uppercase authentication commands and assumed uppercase reply tokens without a server protocol contract, making the client/server protocol mismatch the likely root cause.
- The supplied server source does implement the exact uppercase authentication protocol expected by the client and returns `LOGIN_OK/FAIL` or `REGISTER_OK/FAIL`.
- A harmless invalid-login probe to the configured server port received no authentication response, despite the source requiring `LOGIN_FAIL`. This proves the running service is not executing this source version (or has not been rebuilt/restarted after the authentication code was added).
- With no `CHAT_LOCAL_PORT` set, two concurrent client processes bound to different OS-assigned local TCP ports (34177 and 41809). The service registered and removed sessions using those same values.
- Authentication tested against an isolated server instance returned `REGISTER_OK` and `LOGIN_OK`. Its temporary `users.txt` data was removed after the test.
- The window title is currently generated in `main.c` before the asynchronous connection runs, using `10086` as a fallback. It therefore does not reflect an automatically assigned port.
- The VS Code `运行 chat_sim（WSLg）` task is a foreground task with the default single-instance limit; repeated invocations can be queued until prior GUI processes exit.
- The task now permits up to ten concurrent instances and creates a new terminal for each one. Client port assignment is posted through the existing thread-safe app-event queue before the UI thread updates the SDL window title.
- The newest screenshot shows the user uses the CMake Tools status-bar launch-target button (`[chat_sim] ▶ [chat_sim]`), which bypasses the configured `运行 chat_sim（WSLg）` task. Its reused CMake launch terminal serializes processes.
- CMake Tools 1.23.52 exposes `cmake.launchBehavior`; its documented schema accepts `reuseTerminal`, `breakAndReuseTerminal`, or `newTerminal`, and defaults to `reuseTerminal`. The workspace now sets `newTerminal`.
- Existing attachments already traverse the independent `emoji`/`sendfile` commands and are delivered through `backend_callbacks_t.on_file` to `frontend_file_received`. The UI currently replaces every received attachment with a System text row, while the sender only receives a status line.
- LVGL image widgets, file-system access, and its bundled LodePNG/BMP/TJPGD decoders were present but disabled. Enabling those components supports PNG/JPG/JPEG/BMP without adding an external library or changing the network protocol.
- A received image is decoded only after transfer completes, and previews are bounded to 5 MB source files, 8192×8192 source dimensions, and 200×150 display dimensions. Unsupported or failed previews retain the original attachment text row.
- The CMake Tools launch terminal runs from `build/bin`, whereas the earlier relative-path guidance assumed the project root. Relative image paths can therefore be decoded from a different directory than expected.
- Server cleanup is intended to call `list_delete(sender->ip, sender->port, head)` after a client socket closes. A stale online entry indicates either cleanup did not run for the active service or concurrent list access exposed a race; the configured logical chat port itself does not keep an OS port bound after the process exits.
- The actual project test file `test_images/b.png` is JPEG data despite its `.png` extension. LVGL's bundled JPEG decoder selects by filename extension, so this specific file cannot be rendered when labelled as PNG even though transfer succeeds. This exactly reproduces the observed text fallback on both endpoints.
- An isolated valid-login test confirms normal server disconnect cleanup: after client `41001` closes, a second authenticated client receives an online list containing only `41002`. Thus a stale row is not an occupied local TCP port; it is most likely an exceptional cleanup case in the running service or a stale UI refresh, and cleanup should remove the exact session node rather than look it up by mutable port fields.
- The PNG decoder was also given an `A:` LVGL file-system prefix that its direct file loader could not open. It now strips that prefix, and the JPEG decoder now validates the byte stream rather than using only its filename extension. A real client capture confirms the formerly failing `b.png` now renders as an image bubble.
- The server now removes the exact disconnected list node rather than locating it by the session's mutable IP/port fields. The rebuilt server was restarted on port 10000 as PID 22630.
- The existing “Send emoji” button currently shares the free-form attachment-path inputs and `file_event` with ordinary files. A dedicated picker can make emoji selection a project-owned UI flow while retaining the existing independent `emoji` network command and its already-verified byte transport.
- The new project-owned folder is `assets/emojis`; its README documents the supported formats, size limit, and optional `MICHAT_EMOJI_DIR` development override. No user images were moved, copied, or changed.
- The chat sidebar now exposes an `Emoji` button rather than a free-form “Send emoji” path action. It opens a modal grid, lists up to 64 supported image files, gives readable images a thumbnail, and keeps an unreadable/progressive JPEG selectable with a clear preview-unavailable label rather than silently omitting it.
- `backend_send_emoji_to` is a separate public client entry point which validates an image filename and delegates only its byte streaming to the existing attachment transport using the independent `emoji` command. A socket-pair smoke test proved the emitted header begins `emoji@127.0.0.1@41001@` and contains the JPEG metadata, never `sendfile`.
- The Native WSL GCC build passed. A temporary LVGL smoke test over the untouched `test_images` directory enumerated all four image cards; validation-only test sources and `/tmp` binaries were removed afterward.
- The receiver did receive and save the user's emoji files (`g.jpeg` and `mao.jpeg` appeared in `build/bin`), proving the transport itself was working. The server attachment header lacked the sender identity, so the client routed the incoming image to the hidden shared inbox rather than the sender's conversation.
- Server attachment headers now include `sender-ip:sender-port` while client parsing keeps compatibility with the former header shape. The event queue now carries command and sender separately, so both image bubbles and regular-attachment fallback rows route to the matching conversation.
- Large-image clipping was caused by setting the LVGL image box before setting its source; source loading refreshed the object to the original pixel dimensions. The code now loads the source, applies a 280x210 aspect-ratio-bounded box, then enables stretch. A 708x708 smoke image produced a 210x210 display object with a reduced scale factor.
- Rebuilt the client and server with warning-as-error checks where applicable. The active rebuilt server listens on port 10000 (PID 16379); clients must reconnect/login after the restart.
- The four current picker assets were all JPEG data; `b.png` had a misleading extension and `j.jpg` used progressive JPEG. The original dimensions reached 3840x2160 and 1600x2328, which is unsuitable for a fixed-size emoji UI and can expose decoder/layout edge cases.
- The originals were moved, unchanged, to `assets/emojis/originals/` with their original filenames and hashes preserved. The root picker directory now contains standard non-interlaced PNG versions: `b.png` (280x158), `g.png` (210x210), `j.png` (280x158), and `mao.png` (144x210).
- A temporary program linked against the project's actual LVGL library successfully decoded all four processed files and confirmed their dimensions do not exceed 280x210. The client build passed afterwards; the temporary test source and binary were removed.
- The corrupted previews were caused by an inconsistent prior decoder edit: bundled LodePNG returns an `lv_draw_buf_t`, but `lv_lodepng.c` treated that pointer as raw RGBA data, allocated a second buffer, and copied from the descriptor itself. This corrupted memory and could crash in `lv_memcpy`.
- The decoder now keeps the bundled LVGL ownership contract: PNG input is read through `lv_fs` with its `A:` path intact, LodePNG returns the LVGL draw buffer, and color conversion operates on `decoded->data`.
- The current root picker assets (`smile.png`, `laugh.png`, `cute.png`, and `g.png`) cover RGBA, palette, and RGB PNG formats. All four opened through the actual LVGL decoder with their expected dimensions.
- Visual smoke captures verified all four picker thumbnails and both received/sent chat bubbles. Updating layout before scrolling the new image row prevents the newest large bubble from remaining partly behind the composer.
- RK3568 deployment inspection: the connected board is an AArch64 Rockchip RK3568 EVB1 running Buildroot 2018.02 with 1.9 GB RAM, 5.1 GB free root storage, Ethernet at 192.168.44.180, `/dev/fb0`, and Rockchip DRM cards. It has SDL2 and Wayland client runtime libraries, but no GCC, CMake, pkg-config, SDL2 development tools, compositor process, or active Wayland socket.
- The desktop `chat_sim` binary is x86-64 and cannot run on the RK3568. The board requires an ARM64 build and a display deployment using a board-supported SDL video driver such as KMSDRM, or an LVGL DRM/framebuffer driver.
- The existing server is listening in WSL at 172.18.178.250:10000. The board cannot reach that WSL NAT address. The Windows Ethernet IP is 192.168.44.98, but it does not currently forward TCP 10000 to WSL.

- The ARM64 target uses the board's `/dev/fb0` through LVGL's native framebuffer driver. This avoids requiring a board compositor or an SDL video-driver match and does not change the desktop target.
- The board package is self-contained at `/opt/michat`: outgoing files go in `send_files/`, emoji assets in `assets/emojis/`, and received files in `received/`.
- The board's framebuffer runtime loader is present and the Goodix capacitive touchscreen is `/dev/input/event6`; the board launcher defaults to that exact event device.
- The compact-keyboard/full-screen-redraw update was deployed after the Ethernet link returned. The installed ARM64 binary hash matches the locally built binary, and the new framebuffer client is running as PID 898.
- Goodix reports both single-touch and multi-touch coordinates as 0..1023 by 0..599, exactly matching the framebuffer. The remaining keyboard issue is an evdev multi-touch state bug: the bundled driver treated only tracking ID 0 as a press.
- At user request, the RK3568 keyboard is returned to LVGL's original default geometry; target-only tap-to-open and confirm/cancel-to-hide behavior remains enabled.
- Windows rejected the TCP 10000 port-forward setup without elevation. The current WSL server remains unchanged; only the host-side relay is pending.
- The verified package was deployed through the serial console to `/opt/michat`. `michat-rk3568` is running as board PID 810 and reports its framebuffer event loop started on `/dev/fb0`.

- Desktop chat rows now have an avatar/content split guarded by `MICHAT_DESKTOP_AVATARS`, which is defined only for `chat_sim`. Incoming messages render a purple peer avatar on the left and outgoing messages a blue “ME” avatar on the right; the RK3568 target retains the former message-row layout exactly.

- Project layout is now `/home/alan/miser` for the server and `/home/alan/michat/lvgl_chat` for the client. The server's credential database is explicitly `/home/alan/miser/users.txt`, so starting its executable from another directory cannot create a second user database. The original data file's SHA-256 was unchanged after the move.
- Both CMake build caches were regenerated from the relocated source: the desktop cache and ARM64 toolchain cache reference `/home/alan/michat/lvgl_chat` only. The running legacy server process retained its moved working directory and continues to listen on port 10000; the newly compiled executable is ready for the next restart.

- RK3568 support was removed from the desktop project: `main_fbdev.c`, `platform/rk3568/`, `build-rk3568/`, and the RK3568 CMake target are gone. The desktop LVGL source list now uses SDL only; the board-only touch keyboard was removed from `frontend.c`. The external `/home/alan/rk3568` cross-toolchain was intentionally retained because it may be shared by other projects.

- Online-list entries now originate at the server as `username@ip:port`. The username is assigned only after `LOGIN_OK`; the client presents it as `username  ip:port` and stores it in the matching IP/port session. Endpoint values remain the internal routing identifiers, so chat, file, and emoji transport semantics are unchanged. Incoming messages without a known online-list session retain the old IP/port fallback.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Keep changes confined to UI/authentication files | Avoid changes to unrelated chat features. |
| Bind an unspecified client to port `0` | The OS selects a distinct available TCP source port per process; the existing legacy `register@...@port@...` handshake can report that selected port to the unchanged server. |
| Replace bundled LVGL with a pinned Git submodule | The initial repository tracked 2,773 LVGL files out of 2,801 total and was about 69.5 MiB in Git objects; a submodule preserves reproducible source while keeping the application repository small. |
| Rewrite uploaded initial history only with confirmation | The terminal push completed after the screenshot, so `origin/main` already contains the large first commit. A force-push is now needed to replace it. |
| Use `--force-with-lease` for the slimming rewrite | It replaced only the exact observed remote tip `75792b2`; GitHub accepted the new lightweight root `073bb16`. |
| Add the server as a source-only subproject | `/home/alan/miser` contains three source files plus executables, an account database, a log, and a test image. Only `server.c`, `list.c`, and `list.h` belong in Git; the live deployment will remain untouched. |
| Make account storage configurable and ignored | The old service hard-codes `/home/alan/miser/users.txt`; the repository version should default to `server/data/users.txt`, ignored by Git, and accept `CHAT_USERS_FILE` for deployment. |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
|       |            |
