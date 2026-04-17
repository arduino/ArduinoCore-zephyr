#!/usr/bin/env python

# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

import argparse
import hashlib
import itertools
import os
import re
import struct
import sys
import textwrap
import traceback

from elftools.construct.macros import UNInt32, UNInt64
from elftools.common.exceptions import ELFError
from elftools.common.utils import parse_cstring_from_stream, struct_parse
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


NativePtr = None

def get_str_at(elf, addr):
    for section in elf.iter_sections():
        if section['sh_type'] == 'SHT_NOBITS' or addr < section['sh_addr'] or addr >= section['sh_addr'] + section['sh_size']:
            continue

        file_offset = section['sh_offset'] + addr - section['sh_addr']
        return parse_cstring_from_stream(elf.stream, file_offset).decode('utf-8', errors='replace')

    return None

def get_ptr_at(elf, addr):
    for section in elf.iter_sections():
        if section['sh_type'] == 'SHT_NOBITS' or addr < section['sh_addr'] or addr >= section['sh_addr'] + section['sh_size']:
            continue

        file_offset = section['sh_offset'] + addr - section['sh_addr']
        return struct_parse(NativePtr, elf.stream, file_offset)

    return None

def get_all_syms(elf):
    syms = {}
    for section in elf.iter_sections():
        if not isinstance(section, SymbolTableSection):
            continue
        for symbol in section.iter_symbols():
            syms[symbol.name] = symbol

    return syms

def get_llext_syms(elf):
    syms = {}
    for section in elf.iter_sections():
        if not isinstance(section, SymbolTableSection):
            continue
        for symbol in section.iter_symbols():
            if symbol.name.startswith("__llext_sym_"):
                llext_sym_addr = symbol['st_value']
                sym_name = get_str_at(elf, get_ptr_at(elf, llext_sym_addr))
                sym_value = get_ptr_at(elf, llext_sym_addr + NativePtr.length)
                if sym_value:
                    syms[sym_name] = sym_value

    return syms

def main():
    global NativePtr

    # parse the command-line arguments and invoke ReadElf
    argparser = argparse.ArgumentParser(
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description="Extract symbols from ELF files",
            epilog=textwrap.dedent(f'''\
            SYMBOL DEFINITION
            -----------------
            Each <sym> describes symbol names to extract. This can be an exact symbol
            name, a regular expression, or a rename expression. Also, the symbol can be
            dereferenced, so that the pointer value stored at the symbol address is
            exported, or the size of the symbol can be exported as well.
            The exact rules are as follows:

             - if <sym> starts with a slash ('/'), the rest is treated as a regexp. All
               symbols matching the regexp will be extracted. Can be combined with
               dereferencing, but not renames.
             - if <sym> does not start with a slash, but contains an equals sign ('='),
               it is treated as a rename expression, where the part before the equals
               is the symbol name to extract, and the part after the equals is
               the new name to use in the output.
             - if the first char of <sym> is an asterisk ('*'), the symbol is
               dereferenced, i.e. the pointer value stored at the symbol address is
               exported instead of the symbol address itself.
             - if the first char of <sym> is a plus ('+'), in addition to itself, a
               second symbol called '<sym>_size' is defined with the size of the
               current symbol.

            For example, the symbol definition:

              *sketch_base_addr=__sketch_start

            will export the value stored at the address of the 'sketch_base_addr' symbol
            as '__sketch_start', while

              /__device_dts_ord_.*

            will export all symbols starting with '__device_dts_ord_' as-is. Also,

              +kheap_llext_heap

            will export the value of the 'kheap_llext_heap' symbol and its size in a
            separate 'kheap_llext_heap_size' symbol.
            '''))
    argparser.add_argument('-v', '--verbose',
            action='store_true',
            help='Write the source of the symbol definition as a comment')
    argparser.add_argument('-L', '--llext',
            action='store_true',
            help='Extract symbols from the __llext_sym_* symbols')
    argparser.add_argument('-F', '--funcs',
            action='store_true',
            help='Extract all public functions')
    argparser.add_argument('-R', '--regexp',
            action='append',
			default=[],
            help='Extract all public symbols matching regexp')
    argparser.add_argument('file',
            help='ELF file to parse')
    argparser.add_argument('syms', nargs='*',
            help='Symbols to export')

    args = argparser.parse_intermixed_args()

    exact_syms = set()
    regex_syms = set()
    deref_syms = set()
    sized_syms = set()
    rename_map = {}
    for sym in args.syms:
        sym_class = None
        if sym[0] == '/':
            # Regexp
            sym = f"^{sym[1:]}$"
            sym_class = "regexp"
        elif '=' in sym:
            # Rename expression
            sym, new_sym = sym.split('=')
            sym_class = "rename"
        else:
            # Exact symbol
            sym_class = "exact"

        if sym[0] == '*':
            # Dereference symbol
            sym = sym[1:]
            deref_syms.add(sym)
        elif sym[0] == '+':
            # Store size as well
            sym = sym[1:]
            sized_syms.add(sym)

        if sym_class == "regexp":
            regex_syms.add(sym)
        else:
            exact_syms.add(sym)
            if sym_class == "rename":
                rename_map[sym] = new_sym

    with open(args.file, 'rb') as file:
        try:
            elf_sha = hashlib.sha256(file.read()).hexdigest()
            elf = ELFFile(file)
        except ELFError as ex:
            sys.stdout.flush()
            sys.stderr.write('ELF error: %s\n' % ex)
            traceback.print_exc()
            sys.exit(1)

        if elf.elfclass == 32:
            NativePtr = UNInt32("ptr")
        elif elf.elfclass == 64:
            NativePtr = UNInt64("ptr")

        all_syms = get_all_syms(elf)
        out_syms = {}
        fail = False

        for name, sym in all_syms.items():
            value = None
            comment = []
            if name in exact_syms or any(re.match(r, name) for r in regex_syms):
                comment = "cmd_line"
                value = sym['st_value']
            elif args.funcs and (sym['st_info']['type'] == 'STT_FUNC'
                            and sym['st_info']['bind'] == 'STB_GLOBAL'):
                comment = "public_fn"
                value = sym['st_value']
            elif args.llext and name.startswith("__llext_sym_"):
                comment = "llext_sym"
                llext_sym_addr = sym['st_value']
                name = get_str_at(elf, get_ptr_at(elf, llext_sym_addr))
                value = get_ptr_at(elf, llext_sym_addr + NativePtr.length)
            elif args.regexp and any(map(lambda x: re.match(x, name), args.regexp)):
                comment = "regexp_sym"
                value = sym['st_value']

            if name in deref_syms:
                value = get_ptr_at(elf, value)
            if name in rename_map:
                name = rename_map[name]

            if value is None:
                continue

            if name in out_syms:
                if out_syms[name][0] != value:
                    sys.stderr.write(
                        f"Warning: duplicate symbol {name} with different values: "
                        f"{out_syms[name][0]:#010x} vs {value:#010x}\n")
                    fail = True
                out_syms[name][1].append(comment)
            else:
                out_syms[name] = (value, [comment])

            if name in sized_syms:
                out_syms[name + "_size"] = (sym['st_size'], [f"size of {name}"])

        if not out_syms:
            sys.stderr.write("Warning: No symbols found matching the criteria.\n")

        if fail:
            sys.exit(1)

        print(f"""
/*
 * Automatically generated by {os.path.basename(sys.argv[0])}, do not edit!
 *
 * Source: {args.file}
 * SHA256: {elf_sha}
 */
""")
        sym_comment = nul_comment = ""
        for name, (value, comments) in sorted(out_syms.items(), key=lambda x: x[0]):
            if args.verbose:
                comment = ', '.join(sorted(comments))
                sym_comment = f"/* {comment} */"
                nul_comment = f" ({comment})"
            if value:
                print(f"PROVIDE({name} = {value:#010x});{sym_comment}")
            else:
                print(f"/* NULL {name}{nul_comment} */")

#-------------------------------------------------------------------------------
if __name__ == '__main__':
    main()
