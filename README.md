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

Exit status:
   0 - Timeout expired normally
   1 - Failed with error
   2 - Interrupted by user

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

Exit status:
   0 - File change was detected
   1 - Failed with error
   2 - Interrupted by user

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

Exit status:
   0 - Path converted successfully
   1 - Failed with error
   2 - Interrupted by user
```

waitpid
-------

Wait (sleep) until the specified processes all have terminated.

```
Usage:
   waitpid.exe [options] <PID_1> [<PID_2> ... <PID_n>]

Options:
   --waitone   exit as soon as *any* of the specified processes terminates
   --shutdown  power off the machine, as soon as the processes have terminated
   --timeout   exit as soon as the timeout (default: 30 sec) has expired
   --pedantic  abort with error, if a specified process can *not* be opened
   --quiet     do *not* print any diagnostic messages; errors are shown anyway

Environment:
   WAITPID_TIMEOUT  timeout in millisonds, only if `--timeout` is specified

Exit status:
   0 - Processes have terminated normally
   1 - Failed with error
   2 - Aborted because the timeout has expired
   3 - Interrupted by user
   
```


Platform Support
================

This tools have been created to run on Microsoft&reg; Windows XP or later. Some features require Windows Vista or later.


Source Code
===========

Official source code mirrors:

* `git clone https://github.com/lordmulder/MSleep.git` [[Browse](https://github.com/lordmulder/MSleep)]

* `git clone https://muldersoft@bitbucket.org/muldersoft/msleep.git` [[Browse](https://bitbucket.org/muldersoft/msleep)]


License
=======

**This work is licensed under the [CC0 1.0 Universal License](https://creativecommons.org/publicdomain/zero/1.0/legalcode).**

The person who associated a work with this deed has dedicated the work to the public domain by waiving all of his or her rights to the work worldwide under copyright law, including all related and neighboring rights, to the extent allowed by law.

*You can copy, modify, distribute and perform the work, even for commercial purposes, all without asking permission* 😃

<br>

**e.o.f.**
