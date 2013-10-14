linktest
========

This program is not really useful itself.
It serves as testground of resolving symbols of shared objects from within the
program code, without using libraries like libdl.

While the master-branch contains more or less clean code (how clean can code be,
that calls internal functions of libc?), the make-it-small-branch is a rewrite
with sickly focus on size. The code there may be used in a 4k intro, if it
proves to be small enough.
