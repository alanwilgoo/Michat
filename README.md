# wudream

Michat is a desktop chat client and its TCP relay/authentication service.

## Repository layout

- `client/` — desktop client source, resources, CMake configuration, and
  acceptance documentation.
- `server/` — standalone TCP relay/authentication server. See
  [`server/README.md`](server/README.md) for build and deployment steps.
- `client/third_party/lvgl` — pinned LVGL `v9.2.2` Git submodule.

## First checkout

Fetch the client UI dependency after cloning:

```bash
git submodule update --init --recursive
```

Build the desktop client from its project directory:

```bash
cd client
cmake --preset native-wsl-gcc-debug
cmake --build --preset native-wsl-gcc-debug
```

Server account data, generated binaries, logs, and local test assets are
intentionally excluded from version control.
