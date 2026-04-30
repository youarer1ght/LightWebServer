# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make                  # build the server binary
make clean            # remove build artifacts and binary
./server [name] [addr] [port]  # run (default: hostname, INADDR_ANY, 6025)
```

The build uses recursive Makefiles with GCC, C++17, `-O1`. The final binary `server` is linked from `lib/*.o` + `src/server/*.o`.

Type `exit` in the server console to stop.

## Architecture

This is a Linux-only multithreaded HTTP server using **epoll edge-triggered** I/O. One thread per client; the main thread accepts connections.

### Component layers

1. **`include/defs.hpp`** — tunables (buffer sizes, epoll event count, timeout) and hardcoded credentials used by POST /dopost login.
2. **`lib/`** — reusable library compiled into `.o` files:
   - `Message` → `Request` / `Response` — HTTP message model with serialize/parse support.
   - `Receiver` — owns an epoll instance per client socket, non-blocking recv, parses raw bytes into `Request` objects.
   - `Sender` — serializes `Response` into bytes and sends in chunks (handles partial writes).
3. **`src/server/`** — the server binary:
   - `Server` — socket setup (bind/listen), accept loop with thread-per-client, request routing/dispatch, lifecycle (run/stop/join).
   - `main.cpp` — route table definition (URL → File struct), server instantiation, console command loop.

### Concurrency primitives

- **`Map<K,V>`** (`include/Map.hpp`) — thread-safe `std::map` wrapper. The caller must lock externally (via `get_mutex()`), then pass the `unique_lock` to methods. This allows compound operations under a single lock.
- **`Queue<T>`** (`include/Queue.hpp`) — thread-safe `std::queue` with internal locking. Simple push/pop/empty.

### Request flow

1. `main()` creates `Server` with a route table and calls `run()` in a dedicated thread.
2. `Server::run()` loops on `wait_for_client()` — blocking `accept()`.
3. For each connection, `wait_for_client()` allocates a client ID, creates `Receiver`/`Sender`, stores them in `clientinfo_list_`, and spawns a `receive_from_client()` thread.
4. `receive_from_client()` calls `Receiver::get_request()` (epoll-based read + HTTP parse), dispatches based on route table (GET serves static files, POST /dopost checks credentials), builds a `Response`, and sends via `Sender::send_response()`.
5. Client sockets are set non-blocking; `Receiver` uses epoll edge-triggered mode (`EPOLLET`).

### Resource ownership

- `ClientInfo` owns the raw socket fd, closes it on destruction.
- `Receiver`/`Sender` own the socket fd as well — `Receiver::close()` closes it, `Sender` does not. In practice `ClientInfo::~ClientInfo()` handles socket cleanup.
- `Server` owns `clientinfo_list_` (client objects), `client_recv_list_` (thread objects), and the listen socket.
