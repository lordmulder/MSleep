Auxiliary tools for supplementing batch scripting,
created by LoRd_MuldeR <mulder2@gmx.de>

This work is licensed under the CC0 1.0 Universal License.
To view a copy of the license, visit:
https://creativecommons.org/publicdomain/zero/1.0/legalcode

-----------------------------------------------------------------------------

msleep
Wait (sleep) for the specified amount of time, in milliseconds.

Usage:
   msleep.exe <timeout_ms>

Note: Process creation overhead will be measured and compensated.

-----------------------------------------------------------------------------

notifywait
Wait until a file is changed. File changes are detected via "archive" bit.

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

-----------------------------------------------------------------------------

realpath
Convert file name or relative path into fully qualified "canonical" path.

Usage:
   realpath.exe [options] <filename>

Options:
   --exists     check whether the target file system object exists
   --file       check whether the path points to a file
   --directory  check whether the path points to a directory
