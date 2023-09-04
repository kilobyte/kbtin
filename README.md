KBtin
=====

KBtin is a very heavily extended clone of well-known TinTin++.

If you're new to MUDding, I would really recommend using one of modern
clients, such as Mudlet instead.  The TinTin++ language is awful to use;
the codebase in ancient; user-friendliness is an unthing.

On the other hand, it's one of last remaining text-mode clients, which
enables you eg. to run it on a box in the same datacenter as the game
server -- for juicy 0.1ms ping and thus reaction time -- while still
giving you adequate interactivity despite you living on the other side
of the world, hobbled by that pesky speed of light.  You can use all
the usual Unix tools like `mosh`, `screen`/`tmux`, etc...

The features include:
* editing keys, input box
* scrollback
* status line
* keybindings ("#bind F9 {do drop pasty from cloak,eat pasty}")
* aliases, actions (triggers), substitutions, highlights
* shortest-path graph traversing aliases ("mt>rh")
* guaranteed actions (a line being split between two packets is not a problem)
* lists processing ("#foreach {$friends} {tell $0 [XX] $message}")
* TELNET protocol support
* MCCP compression
* the ability to run local programs as it was a MUD -- this is the only client you can use with games like adventure, or even programs like mysqlclient
* secondary sessions (ICQ integration, etc)
* UTF-8 and IPv6 support
* native SSL, with certificate retention to avoid MitM attacks
* fractional arithmetic

KBtin has been ported to the following systems:

* Linux
* SunOS<sup>long untested</sup>
* MacOS X
* {Free,Open,Net}BSD
* Tru64 Unix<sup>long untested</sup>
* HP-UX<sup>long untested</sup>
* IRIX<sup>long untested</sup>
* SCO OpenServer<sup>long untested</sup>
* Solaris<sup>long untested</sup>

Dependencies required to build:

* cmake
* perl
* zlib (libz-dev) [OPTIONAL]
* gnutls (libgnutls28-dev) [OPTIONAL]
* hyperscan/vectorscan [OPTIONAL]
* valgrind-dev [OPTIONAL]
