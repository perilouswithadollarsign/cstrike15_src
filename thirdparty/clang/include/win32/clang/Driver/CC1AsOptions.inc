/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|*Option Parsing Definitions                                                  *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifndef PREFIX
#error "Define PREFIX prior to including this file!"
#endif

/////////
// Prefixes

#define COMMA ,
PREFIX(prefix_0, {0})
PREFIX(prefix_1, {"-" COMMA 0})
PREFIX(prefix_3, {"-" COMMA "--" COMMA 0})
PREFIX(prefix_2, {"--" COMMA 0})
#undef COMMA

#ifndef OPTION
#error "Define OPTION prior to including this file!"
#endif

/////////
// Groups


//////////
// Options

OPTION(prefix_0, "<input>", INPUT, Input, INVALID, INVALID, DriverOption | CC1Option, 0, 0, 0)
OPTION(prefix_0, "<unknown>", UNKNOWN, Unknown, INVALID, INVALID, 0, 0, 0, 0)
OPTION(prefix_1, "I", I, JoinedOrSeparate, INVALID, INVALID, 0, 0,
       "Add directory to include search path", "<directory>")
OPTION(prefix_1, "L", L, Flag, INVALID, INVALID, 0, 0,
       "Save temporary labels in the symbol table. Note this may change .s semantics, it should almost never be used on compiler generated code!", 0)
OPTION(prefix_1, "dwarf-debug-flags", dwarf_debug_flags, Separate, INVALID, INVALID, 0, 0,
       "The string to embed in the Dwarf debug flags record.", 0)
OPTION(prefix_1, "dwarf-debug-producer", dwarf_debug_producer, Separate, INVALID, INVALID, 0, 0,
       "The string to embed in the Dwarf debug AT_producer record.", 0)
OPTION(prefix_2, "fatal-warnings", fatal_warnings, Flag, INVALID, INVALID, 0, 0,
       "Consider warnings as errors", 0)
OPTION(prefix_1, "fdebug-compilation-dir", fdebug_compilation_dir, Separate, INVALID, INVALID, 0, 0,
       "The compilation directory to embed in the debug info.", 0)
OPTION(prefix_1, "filetype", filetype, Separate, INVALID, INVALID, 0, 0,
       "Specify the output file type ('asm', 'null', or 'obj')", 0)
OPTION(prefix_1, "g", g, Flag, INVALID, INVALID, 0, 0,
       "Generate source level debug information", 0)
OPTION(prefix_3, "help", help, Flag, INVALID, INVALID, 0, 0,
       "Print this help text", 0)
OPTION(prefix_1, "main-file-name", main_file_name, Separate, INVALID, INVALID, 0, 0,
       "Main file name to use for debug info", 0)
OPTION(prefix_1, "mllvm", mllvm, Separate, INVALID, INVALID, 0, 0,
       "Additional arguments to forward to LLVM's option processing", 0)
OPTION(prefix_2, "noexecstack", no_exec_stack, Flag, INVALID, INVALID, 0, 0,
       "Mark the file as not needing an executable stack", 0)
OPTION(prefix_1, "n", n, Flag, INVALID, INVALID, 0, 0,
       "Don't automatically start assembly file with a text section", 0)
OPTION(prefix_1, "output-asm-variant", output_asm_variant, Separate, INVALID, INVALID, 0, 0,
       "Select the asm variant index to use for output", 0)
OPTION(prefix_1, "o", o, Separate, INVALID, INVALID, 0, 0,
       "Specify output file", "<path>")
OPTION(prefix_1, "relax-all", relax_all, Flag, INVALID, INVALID, 0, 0,
       "Relax all fixups (for performance testing)", 0)
OPTION(prefix_1, "show-encoding", show_encoding, Flag, INVALID, INVALID, 0, 0,
       "Show instruction encoding information in transliterate mode", 0)
OPTION(prefix_1, "show-inst", show_inst, Flag, INVALID, INVALID, 0, 0,
       "Show internal instruction representation in transliterate mode", 0)
OPTION(prefix_1, "target-cpu", target_cpu, Separate, INVALID, INVALID, 0, 0,
       "Target a specific cpu type", 0)
OPTION(prefix_1, "target-feature", target_feature, Separate, INVALID, INVALID, 0, 0,
       "Target specific attributes", 0)
OPTION(prefix_1, "triple", triple, Separate, INVALID, INVALID, 0, 0,
       "Specify target triple (e.g. x86_64-pc-linux-gnu)", 0)
OPTION(prefix_3, "version", version, Flag, INVALID, INVALID, 0, 0,
       "Print the assembler version", 0)
OPTION(prefix_1, "v", v, Flag, INVALID, version, 0, 0, 0, 0)
