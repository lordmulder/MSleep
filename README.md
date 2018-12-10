% ![](img/msleep/banner.jpg)  
MSleep &ndash; Auxiliary tools for supplementing Batch scripts
% by LoRd_MuldeR &lt;<mulder2@gmx>&gt; | <http://muldersoft.com/>


Contents
========

The following tools, 32-Bit and 64-Bit versions, are included in the “MSleep” suite at this time:

msleep
------

Wait (sleep) for the specified amount of time, in milliseconds.

```
Usage:
   msleep.exe <timeout_ms>

Note: Process creation overhead will be measured and compensated.
```

notifywait
----------

Wait until a file is changed. File changes are detected via "archive" bit.

```
Usage:
   notifywait.exe [options] <filename_1> [<filename_2> ... <filename_N>]

Options:
   --clear  unset the "archive" bit *before* monitoring for file changes
   --reset  unset the "archive" bit *after* a file change was detected
   --quiet  do *not* print the file name that changed to standard output
   --debug  turn *on* additional diagnostic output (for testing only!)

Remarks:
   The operating system sets the "archive" bit whenever a file is changed.
   If, initially, the "archive" bit is set, program terminates right away.
   If *multiple* files are given, program terminates on *any* file change.
```

realpath
--------

Convert file name or relative path into fully qualified "canonical" path.

```
Usage:
   realpath.exe [options] <filename_1> [<filename_2> ... <filename_N>]

Options:
   --exists     requires the target file system object to exist
   --file       requires the target path to point to a regular file
   --directory  requires the target path to point to a directory
```


Platform Support
================

This tools have been created to run on Microsoft&reg; Windows XP or later. Some features require Windows Vista or later.


License
=======

**This work is licensed under the [CC0 1.0 Universal License](https://creativecommons.org/publicdomain/zero/1.0/legalcode).**

The person who associated a work with this deed has dedicated the work to the public domain by waiving all of his or her rights to the work worldwide under copyright law, including all related and neighboring rights, to the extent allowed by law.

*You can copy, modify, distribute and perform the work, even for commercial purposes, all without asking permission* 😃

<br>

**e.o.f.**
