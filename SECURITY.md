# Security policy

## Supported versions

Only the latest release, which tracks one WPILib alpha at a time, receives fixes.

## Reporting a vulnerability

Please do **not** open a public issue. Use GitHub's private reporting:
<https://github.com/6107/cpy6107/security/advisories/new> (repository **Security** tab, *Report a
vulnerability*).

Include the affected version, how to reproduce, and the impact. We aim to acknowledge a report
within 7 days and will tell you what we plan to do and when. This is a volunteer student-run project,
so those times are goals, not guarantees.

## Scope

cpy6107 is robot-side library code. Examples of what to report: memory-safety problems in the C++ or
bindings, unsafe handling of data coming from the network or driver station, and problems in the
release workflow or its dependencies. Bugs in WPILib or robotpy themselves belong upstream.
