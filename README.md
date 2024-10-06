Obsidian
========

Obsidian is a fast Minecraft server emulator that seeks to re-implement the 
Minecraft server protocol as it existed before the Netty rewrite. This
roughly covers the range of versions `alpha 1.0.14` through `release 1.6.4`.
The server is written from scratch in C and uses `io_uring` for fast 
asynchronous network I/O. As a result, the server only runs on modern versions
of Linux.

Obsidian is Free Software and is license under the GPL-3.0. See `LICENSE` for 
additional information.
