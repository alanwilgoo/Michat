# wudream

Michat is a desktop chat client and its TCP relay/authentication service.

## Repository layout

- `./` — desktop client source, resources, and CMake configuration.
- `server/` — standalone TCP relay/authentication server. See
  [`server/README.md`](server/README.md) for build and deployment steps.
- `third_party/lvgl` — pinned LVGL `v9.2.2` Git submodule.

## First checkout

Fetch the client UI dependency after cloning:

```bash
git submodule update --init --recursive
```

Server account data, generated binaries, logs, and local test assets are
intentionally excluded from version control.
