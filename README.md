l80
===
`l80` is an Intel 8080/Zilog Z80 linker for CP/M-80.

It reads in object files created by
[`a80`](https://github.com/ibara/a80)
and produces executable CP/M-80 binaries from them.

Building
--------
`l80` should build with any
[D](https://dlang.org/)
compiler for any supported platform. I use
[GDC](https://gdcproject.org/)
on
[OpenBSD](https://www.openbsd.org/)
and that works well.

Running
-------
`usage: l80 binary file1.obj [file2.obj ...]`

All object files must end in `.obj` or `.lib`.

The `.com` extension will automatically be appended to
`binary`.

Object format
-------------
`l80` uses the most simple object format I could devise.

Object files are comprised of control codes and data. There
are three control codes:
* `00`: The following byte is literal data.
* `01`: The following bytes are a symbol declaration.
* `02`: The following bytes are a symbol reference.

`l80` uses two passes to generate the final execuatable
binary. The first pass writes all object files and libraries
into a single buffer and then collects all the symbol
declarations and calculates the address of each symbol. The
second pass writes out the executable, replacing references
with the addresses calculated during the first pass.

Libraries are simply collections of object files. They can
be created with the (upcoming!) ar80 tool.

Caveats
-------
`l80` does not recognize nor remove the code of unused
symbols. Doing so is planned.

The order of the object files and libraries can be very
important.

Bugs
----
Probably lots. Test and let me know.

Future
------
`l80` would be able to link MS-DOS `COM` executables as-is,
if only there were an assembler or compiler that used the
`l80` file format...

License
-------
ISC License. See `LICENSE` for details.

Note
----
This `l80` is in no way related to the linker of the same
name produced by Microsoft, also for CP/M-80.

That one uses a very different file format.
