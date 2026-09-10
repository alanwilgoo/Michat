# Michat server

This is the TCP relay and authentication server for the desktop client in the
repository root.

## Build and run

```bash
cd server
make
make run
```

The server listens on TCP port `10000` by default. Override it with
`CHAT_SERVER_PORT`, for example:

```bash
CHAT_SERVER_PORT=10001 make run
```

## Account data

Local accounts are written to `data/users.txt`. That file is deliberately
ignored by Git because it contains credentials. Use `CHAT_USERS_FILE` to set a
different absolute or relative data file in a deployment:

```bash
CHAT_USERS_FILE=/var/lib/michat/users.txt make run
```

Ensure the parent directory exists and is writable by the process. Do not
commit account data, compiled binaries, logs, or received test files.
