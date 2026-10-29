# Multiplayer Escape Room over TCP

A text-based cooperative escape room playable online, written in C with POSIX sockets and pthreads for the Computer Networks course (Reti Informatiche, University of Pisa, A.Y. 2023/2024). A server hosts several concurrent games; players sign up, log in, pick a scenario and explore the rooms together: `look`, `take`, `use` and `drop` objects, solve the riddles that unlock them, chat with teammates, and escape before the countdown ends.

## Architecture
- **Server** (`server_side/`): the main thread reads admin commands (`start <port>`, `stop`, `exit`); `start` spawns a listener thread that accepts TCP connections and a timer thread. Every client gets its own worker thread for the whole (persistent) connection. Shared state is protected by a fixed mutex hierarchy (clients array, games array, per-game, per-client) to avoid deadlocks; limits on players per game and concurrent games are set in `shared/constants.h`.
- **Client** (`client_side/`): one thread reads the user's input, a second one receives server messages; a condition variable implements the request/answer handshake.
- **Protocol** (`shared/`): length-prefixed text messages. Server messages are tagged `ANSWER@` (reply to the last command), `INFO@` (events caused by other players, printed asynchronously) or `END@` (close the session). The console module puts the terminal in raw mode so that incoming messages never garble the line being typed.
- **Game logic** (`server_side/game.c`): locations, objects, riddles and per-game timers. Scenario 0 includes a cooperative trap: when two or more players stand in the corridor, the floor starts collapsing and everybody loses unless someone moves within ten seconds.

## How to run
```
make
./server 4242        # then type: start 4242
./client 4242        # in other terminals, one per player
```
`script.sh` opens one server and two clients in separate GNOME terminals. Accounts are stored in `users.txt` (a `demo demo` account is included). `Documentazione.pdf` is the project report (in Italian).

## Notes
- Requires Linux (POSIX sockets, pthreads, termios). Global variables are defined in the headers, so the Makefile passes `-fcommon` to build with GCC 10 and later.
