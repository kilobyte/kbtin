KBtin
=====

Overview
--------

KBtin is a very heavily extended clone of well-known TinTin++.

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
* the ability to run local programs as it was a MUD -- this is the only client you can use with games like adventure, or even programs like mysqlclient
* secondary sessions (ICQ integration, etc)
* UTF-8 and IPv6 support
* native SSL, with certificate retention to avoid MitM attacks

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

Installation from sources
-------------------------

To install KBtin, you need to:

* untar it: `tar xvfJ kbtin-1.0.21.tar.xz`
* On systems where tar doesn't support the J flag, you have to pipe it through xz manually: `xz -d <kbtin-1.0.20.tar.xz|tar xvf -`
* `cd kbtin-1.0.21`
* `./configure`
* `make` (or `gmake` on *BSD)
* as root, `make install`
* or as an ordinary user, `make bin`
