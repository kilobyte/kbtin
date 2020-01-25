KBtin
=====

1.0.19
------

1.0.19 is a minor bugfix release: a rare crash, an OS X build problem, an 1.0.18 regression in downconverting colors.

In 1.0.18, log-processing tools: ansi2html, ansi2txt, ttyrec2ansi and
pipetty have been removed; they are available as a separate package now.

Note that since 1.0.16 support for non-GNU make has been dropped, please use
gmake on systems that use other makes.

Distributions
-------------

Debian and Ubuntu have KBtin in the regular archives.  So do most other
distributions, although some may haven't been updated in a while.

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

* untar it: `tar xvfJ kbtin-1.0.19.tar.xz`
* On systems where tar doesn't support the J flag, you have to pipe it through xz manually: `xz -d <kbtin-1.0.19.tar.xz|tar xvf -`
* `cd kbtin-1.0.19`
* `./configure`
* `make`
* as root, `make install`
* or as an ordinary user, `make bin`
