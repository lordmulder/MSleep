Auxiliary tools for batch scripting,
created by LoRd_MuldeR <mulder2@gmx.de>

This work is licensed under the CC0 1.0 Universal License.
To view a copy of the license, visit:
https://creativecommons.org/publicdomain/zero/1.0/legalcode

-----------------------------------------------------------------------------

msleep

Wait (sleep) for the specified amount of time, in milliseconds.
Process creation overhead will be measured and compensated.

Usage:
   msleep.exe <timeout_ms>

-----------------------------------------------------------------------------

file change watcher

Wait until the file has changed. File changes are detected via "archive" bit.
If initially the "archive" bit is already set, program terminates immeditely.

Usage:
   watch.exe [--clear] <file_name>

With option "--clear" the archive bit is cleared after a change was detected!
