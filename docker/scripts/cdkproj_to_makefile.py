#!/usr/bin/env python3
"""
cdkproj_to_makefile.py — Convert txw4002a.cdkproj to a Linux GNU Makefile.

Usage:
    python3 cdkproj_to_makefile.py <project.cdkproj> <output_dir>

The generated Makefile (Makefile.linux) uses the csky-elfabiv2-* Wine wrappers
installed by install-cdk.sh and expects to be invoked from PROJECT_DIR.
"""

import sys
import os
import xml.etree.ElementTree as ET


def error(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def el_text(parent, tag, default=""):
    """Return stripped text of the first matching child element, or default."""
    el = parent.find(tag) if parent is not None else None
    return el.text.strip() if el is not None and el.text else default


def find_el(root, tag):
    """Find the first element with *tag* anywhere in *root*."""
    return root.find(".//" + tag)


def parse_defines(define_str):
    return " ".join(f"-D{d}" for d in define_str.split() if d)


def parse_include_paths(include_str):
    """
    Convert semicolon-separated CDK include paths to -I flags.
    $(ProjectPath) → $(PROJECT_DIR)
    """
    result = []
    for p in include_str.split(";"):
        p = p.strip().replace("$(ProjectPath)", "$(PROJECT_DIR)").replace("\\", "/")
        if p:
            result.append(f"-I{p}")
    return " ".join(result)


def parse_lib_names(lib_name_str):
    return [n.strip() for n in lib_name_str.split(";") if n.strip()]


OPTIM_MAP = {
    "Optimize size (-Os)": "-Os",
    "Optimize (-O1)": "-O1",
    "Optimize more (-O2)": "-O2",
    "Optimize most (-O3)": "-O3",
    "No optimization (-O0)": "-O0",
}

DEBUG_MAP = {
    "Default (-g)": "-g",
    "gdwarf2": "-gdwarf-2",
    "gdwarf3": "-gdwarf-3",
    "None": "",
}


def extract_sources(root):
    """Return all compilable source files (.c / .S / .s) from the project tree."""
    sources = []
    for el in root.findall(".//File"):
        name = el.get("Name", "").replace("\\", "/")
        ext = os.path.splitext(name)[1].lower()
        if ext in (".c", ".s"):
            sources.append(name)
    return sources


def safe_obj_name(src_path):
    """
    Derive a unique, flat object-file stem from a source path.
    e.g. ../csky/libs/libc/malloc.c → csky_libs_libc_malloc
    """
    # Strip leading ../
    p = src_path.lstrip("./").lstrip("../")
    p = p.replace("..", "").replace("/", "_").replace("\\", "_")
    stem = os.path.splitext(p)[0].strip("_")
    return stem


def generate_makefile(cdkproj_path):
    tree = ET.parse(cdkproj_path)
    root = tree.getroot()

    # Locate the first BuildConfig element
    bc = find_el(root, "BuildConfig")
    if bc is None:
        bc = root

    compiler_el = bc.find("Compiler")
    asm_el      = bc.find("Asm")
    linker_el   = bc.find("Linker")
    target_el   = bc.find(".//Target")
    output_el   = bc.find(".//Output")

    # ── Compiler ──────────────────────────────────────────────────────────
    c_defs    = el_text(compiler_el, "Define")
    c_incs    = el_text(compiler_el, "IncludePath")
    c_optim   = OPTIM_MAP.get(el_text(compiler_el, "Optim"), "-Os")
    c_debug   = DEBUG_MAP.get(el_text(compiler_el, "DebugLevel"), "-g")
    c_other   = el_text(compiler_el, "OtherFlags")
    c_warn    = "-Wall" if el_text(compiler_el, "AllWarn") == "yes" else ""

    # ── Assembler ─────────────────────────────────────────────────────────
    asm_defs  = el_text(asm_el, "Define")
    asm_incs  = el_text(asm_el, "IncludePath")
    asm_debug = DEBUG_MAP.get(el_text(asm_el, "DebugLevel"), "-gdwarf-2")

    # ── Linker ────────────────────────────────────────────────────────────
    ld_file   = (el_text(linker_el, "LDFile")
                 .replace("$(ProjectPath)", "$(PROJECT_DIR)")
                 .replace("\\", "/"))
    lib_names = el_text(linker_el, "LibName")
    lib_path  = (el_text(linker_el, "LibPath")
                 .replace("$(ProjectPath)", "$(PROJECT_DIR)")
                 .replace("\\", "/"))
    ld_other  = (el_text(linker_el, "OtherFlags")
                 .replace("-Wl,-Map=project.map",
                           "-Wl,-Map=$(LST_DIR)/txw4002a.map"))
    ld_gc     = el_text(linker_el, "Garbage", "no") == "yes"

    libs = parse_lib_names(lib_names)

    # ── Target/CPU ────────────────────────────────────────────────────────
    cpu        = el_text(target_el, "CPU", "ck803")
    endian     = el_text(target_el, "Endian", "little")
    hard_float = el_text(target_el, "UseHardFloat", "no")

    # NOTE: Wine-wrapped GCC 6.3.0 (csky-elfabiv2) does not support:
    #  - `-mabiv2` (redundant; abiv2 is implicit in toolchain name "csky-elfabiv2")
    #  - `-mno-hard-float` (not recognized by this GCC version)
    # Keep only: -mcpu=ck803 -mlittle-endian
    cpu_flags = f"-mcpu={cpu}"
    if endian == "little":
        cpu_flags += " -mlittle-endian"

    # ── Source files ──────────────────────────────────────────────────────
    sources = extract_sources(root)

    # Assign unique object file names (flat Obj/ directory)
    obj_entries = []   # list of (obj_path, src_path, is_asm)
    seen_stems = {}
    for src in sources:
        stem = safe_obj_name(src)
        # Resolve duplicates by appending a counter
        if stem in seen_stems:
            seen_stems[stem] += 1
            stem = f"{stem}_{seen_stems[stem]}"
        else:
            seen_stems[stem] = 0
        obj = f"$(OBJ_DIR)/{stem}.o"
        is_asm = src.lower().endswith(".s")
        obj_entries.append((obj, src, is_asm))

    # ── Build output ──────────────────────────────────────────────────────
    lines = []
    L = lines.append

    L("# Auto-generated from txw4002a.cdkproj by cdkproj_to_makefile.py")
    L("# Do NOT edit — regenerate from the .cdkproj source instead.")
    L("")
    L("# ── Toolchain (wine-wrapped wrappers installed by install-cdk.sh) ───────")
    L("PREFIX   ?= csky-elfabiv2-")
    L("CC        = $(PREFIX)gcc")
    L("AS        = $(PREFIX)gcc -x assembler-with-cpp")
    L("LD        = $(PREFIX)gcc")
    L("AR        = $(PREFIX)ar")
    L("OBJCOPY   = $(PREFIX)objcopy")
    L("OBJDUMP   = $(PREFIX)objdump")
    L("SIZE      = $(PREFIX)size")
    L("")
    L("# ── Project layout ──────────────────────────────────────────────────────")
    L("PROJECT_DIR ?= $(CURDIR)")
    L("OBJ_DIR     ?= $(PROJECT_DIR)/Obj")
    L("LST_DIR     ?= $(PROJECT_DIR)/Lst")
    L("# Absolute paths for Wine linker (relative paths don't work with Wine ld.exe)")
    L("PROJECT_DIR_ABS ?= $(shell cd $(PROJECT_DIR) 2>/dev/null && pwd || echo $(PROJECT_DIR))")
    L("")
    L("# ── CPU / ABI flags ─────────────────────────────────────────────────────")
    L(f"CPU_FLAGS = {cpu_flags}")
    L("")
    L("# ── C compiler flags ────────────────────────────────────────────────────")
    L(f"CFLAGS  = $(CPU_FLAGS) {c_optim} {c_debug} {c_warn} {c_other}".rstrip())
    L(f"CFLAGS += {parse_defines(c_defs)}")
    L(f"CFLAGS += {parse_include_paths(c_incs)}")
    L("")
    L("# ── Assembler flags ─────────────────────────────────────────────────────")
    L(f"ASFLAGS  = $(CPU_FLAGS) {asm_debug}")
    L(f"ASFLAGS += {parse_defines(asm_defs)}")
    L(f"ASFLAGS += {parse_include_paths(asm_incs)}")
    L("")
    L("# ── Linker flags ────────────────────────────────────────────────────────")
    ld_gc_flag = "-Wl,--gc-sections" if ld_gc else ""
    lib_flags  = " ".join(f"-l{l}" for l in libs)
    # Convert relative lib path to absolute for Wine linker compatibility
    lib_path_abs = lib_path.replace("$(PROJECT_DIR)", "$(PROJECT_DIR_ABS)")
    L(f"LDFLAGS  = $(CPU_FLAGS) -T $(PROJECT_DIR_ABS)/utilities/gcc_csky.ld")
    L(f"LDFLAGS += -L{lib_path_abs} {lib_flags}")
    L(f"LDFLAGS += {ld_gc_flag} {ld_other}".rstrip())
    L("")
    L("# ── Targets ─────────────────────────────────────────────────────────────")
    L("TARGET = $(OBJ_DIR)/txw4002a.elf")
    L("HEX    = $(OBJ_DIR)/txw4002a.ihex")
    L("")
    L(".PHONY: all clean")
    L("")
    L("all: $(OBJ_DIR) $(LST_DIR) $(TARGET) $(HEX)")
    L("\t$(SIZE) $(TARGET)")
    L("")
    L("$(OBJ_DIR) $(LST_DIR):")
    L("\tmkdir -p $@")
    L("")
    L("$(TARGET): $(OBJS)")
    L("\t$(LD) $(LDFLAGS) -o $@ $^")
    L("")
    L("$(HEX): $(TARGET)")
    L("\t$(OBJCOPY) -O ihex $< $@")
    L("")
    L("clean:")
    L("\trm -rf $(OBJ_DIR) $(LST_DIR)")
    L("")

    # ── Per-file object rules ─────────────────────────────────────────────
    L("# ── Object files ────────────────────────────────────────────────────────")
    L("OBJS = \\")
    for i, (obj, _, _) in enumerate(obj_entries):
        trail = " \\" if i < len(obj_entries) - 1 else ""
        L(f"    {obj}{trail}")
    L("")

    for obj, src, is_asm in obj_entries:
        # Normalise source path for Makefile
        src_mk = src.replace("\\", "/").replace("$(ProjectPath)", "$(PROJECT_DIR)")
        L(f"{obj}: {src_mk} | $(OBJ_DIR)")
        if is_asm:
            L(f"\t$(AS) $(ASFLAGS) -c -o $@ $<")
        else:
            L(f"\t$(CC) $(CFLAGS) -c -o $@ $<")
        L("")

    return "\n".join(lines)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <project.cdkproj> <output_dir>", file=sys.stderr)
        sys.exit(1)

    cdkproj_path = sys.argv[1]
    output_dir   = sys.argv[2]

    if not os.path.isfile(cdkproj_path):
        error(f"project file not found: {cdkproj_path}")

    content = generate_makefile(cdkproj_path)

    os.makedirs(output_dir, exist_ok=True)
    out_path = os.path.join(output_dir, "Makefile.linux")
    with open(out_path, "w") as fh:
        fh.write(content)

    print(f"Generated: {out_path}")
