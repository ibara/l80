/**
 * Copyright (c) 2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

import std.stdio;
import std.file;
import std.conv;
import std.string;
import std.algorithm;

/**
 * All object files together in one array.
 */
private ubyte[] rawobjs;

/**
 * Final binary.
 */
private ubyte[] binary;

/**
 * Address counter.
 * Starts at 100h, because CP/M.
 */
private size_t addr = 0x100;

/**
 * Struct holding name and matching calculated addresses.
 * collect.addr is a size_t to detect address overflow.
 */
struct collect
{
    string name;        /// Symbol name
    size_t addr;        /// Symbol address
};

/**
 * Collection of addresses.
 */
private collect[] collection;

/**
 * Error messages.
 */
private int error(string msg)
{
    stderr.writeln("l80: error: " ~ msg);
    return 1;
}

/**
 * Check if symbol exists in the collection.
 */
private bool dupname(string name)
{
    for (size_t i = 0; i < collection.length; i++) {
        if (name == collection[i].name)
            return true;
    }

    return false;
}

/**
 * Pass 1: Collect symbol names.
 */
private int collect1()
{
    for (size_t i = 0; i < rawobjs.length; i++) {
        if (rawobjs[i] == '\0') {
            /* Skip control byte.  */
            ++i;

            /* Increment address counter.  */
            ++addr;

            /* Binary has a maximum size of 65,280 bytes (FF00h).  */
            if (addr > 65280)
                return error("final binary exceeds 65,280 bytes");
        } else if (rawobjs[i] == '\001') {
            string name;

            /* Skip control byte.  */
            ++i;

            /* Get symbol name, put in collect struct.  */
            while (rawobjs[i] != '\001') {
                name ~= rawobjs[i++];
                if (i == rawobjs.length)
                    return error("unterminated symbol");
            }

            /* Make sure there is actually a symbol here.  */
            if (name.empty)
                return error("symbol marker with no symbol inside");

            /* Make sure name isn't already in the collection.  */
            if (dupname(name) == true)
                return error("duplicate symbol: " ~ name);

            /* Add to collection table.  */
            collect newcollect = { name, addr };
            collection ~= newcollect;
        } else if (rawobjs[i] == '\002') {
            /* Skip control byte.  */
            ++i;

            /* Ignore the references during pass 1.  */
            while (rawobjs[i] != '\002') {
                ++i;
                if (i == rawobjs.length)
                    return error("unterminated symbol reference");
            }

            /* Increment address counter.  */
            addr += 2;

            /* Binary has a maximum size of 65,280 bytes (FF00h).  */
            if (addr > 65280)
                return error("final binary exceeds 65,280 bytes");
        } else {
            /* This should never happen.  */
            return error("unknown control byte: " ~ to!string(rawobjs[i]));
        }
    }

    return 0;
}

/**
 * Pass 2: Write out final binary.
 */
private int process2()
{
    for (size_t i = 0; i < rawobjs.length; i++) {
        if (rawobjs[i] == '\0') {
            /* Skip control byte.  */
            ++i;

            /* Write byte to final binary.  */
            binary ~= rawobjs[i];
        } else if (rawobjs[i] == '\001') {
            /* Skip control byte.  */
            ++i;

            /* Ignore the declarations during pass 2.  */
            while (rawobjs[i] != '\001') {
                ++i;
                if (i == rawobjs.length)
                    return error("unterminated symbol");
            }
        } else if (rawobjs[i] == '\002') {
            bool found = false;
            string name;

            /* Skip control byte.  */
            ++i;

            /* Get symbol reference.  */
            while (rawobjs[i] != '\002') {
                name ~= rawobjs[i];
                ++i;

                if (i == rawobjs.length)
                    return error("unterminated symbol reference");
            }

            /* Make sure there is actually a symbol here.  */
            if (name.empty)
                return error("symbol marker with no symbol inside");

            /* Output symbol.  */
            for (size_t j = 0; j < collection.length; j++) {
                if (name == collection[j].name) {
                    binary ~= cast(ubyte)(collection[j].addr & 0xff);
                    binary ~= cast(ubyte)((collection[j].addr >> 8) & 0xff);
                    found = true;
                    break;
                }
            }

            /* If symbol was not found, error out.  */
            if (found == false)
                return error("undefined reference: " ~ name);
        } else {
            /* This should never happen.  */
            return error("unknown control byte: " ~ to!string(rawobjs[i]));
        }
    }

    return 0;
}

/**
 * After all code is emitted, write it out to a file.
 */
private void fileWrite(string outfile)
{
    import std.file : write;

    write(outfile, binary);
}

int main(string[] args)
{
    int ret;

    /* Make sure we have the right number of arguments.  */
    if (args.length < 3) {
        stderr.writeln("usage: l80 file.com file1.obj [file2.obj ...]");
        return 1;
    }

    /* Write all object files into a single buffer.  */
    foreach (size_t i; 2 .. args.length) {
        auto objsplit = args[i].findSplit(".obj");

        /* Objects must end in .obj or .lib.  */
        if (objsplit[1].empty) {
            auto libsplit = args[i].findSplit(".lib");
            if (libsplit[1].empty)
                return error("object files must end in \".obj\" or \".lib\"");
        }

        rawobjs ~= cast(ubyte[])read(args[i]);
    }

    ret = collect1();
    if (ret == 0) {
        ret = process2();
        if (ret == 0)
            fileWrite(args[1]);
    }

    return ret;
}
