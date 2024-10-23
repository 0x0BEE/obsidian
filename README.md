# Obsidian

## Overview

Obsidian is an implementation of the Minecraft server protocol that aims to support client versions *alpha 1.0.17_4* 
all the way up to the Netty net code rewrite. Obsidian is written in modern C++ (C++20) and makes use of coroutines
and *io_uring* for asynchronous networking. The server is in early development and not at all suited for production 
use. 

## Requirements

Obsidian is only supported on operating systems running a modern Linux kernel (at least 6.0). Development officially
targets and supports the latest stable release of *Debian GNU/Linux*.

## Reporting issues

Please report issues via the [GitHub issue tracker](https://github.com/0x0BEE/obsidian/issues).

## Copyright

Obsidian is licensed under the GPL v3.0 license. For more information see [LICENSE](LICENSE).
