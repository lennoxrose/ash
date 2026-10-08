#include "codegen/H/emit/layout.h"
#include "app/H/target.h"
#include "elf/H/elf_writer.h"
#include "elf/H/elf_dynamic.h"
#include "pe/H/pe_writer.h"
#include "pe/H/pe_dll_writer.h"

static int is_linux_shared(void) {
    return kiln_get_target() == KILN_TARGET_LINUX && kiln_get_link_mode() == KILN_LINK_SHARED;
}

uint64_t kiln_code_base(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) {
        // Three different fixed bases depending on exactly what's being
        // compiled: libkilnrt.dll's own code (own fixed base, distinct
        // from any executable's), a --link=shared executable's code
        // (globals stay at the same spot either way, but the import
        // table before it is a different size -- see pe_writer.h), or a
        // plain static executable's code (unchanged since Plan A).
        if (kiln_is_compiling_so()) return KILN_PE_DLL_IMAGE_BASE + KILN_PE_DLL_CODE_START_OFFSET;
        if (kiln_get_link_mode() == KILN_LINK_SHARED) return KILN_PE_IMAGE_BASE + KILN_PE_SHARED_CODE_START_OFFSET;
        return KILN_PE_IMAGE_BASE + KILN_PE_CODE_START_OFFSET;
    }
    if (is_linux_shared()) return KILN_LOAD_BASE + KILN_DYN_CODE_START_OFFSET;
    return KILN_LOAD_BASE + KILN_CODE_START_OFFSET;
}

uint64_t kiln_heap_ptr_addr(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) return KILN_PE_HEAP_PTR_ADDR; // same address either link mode -- globals sit before the (variably-sized) import tables
    if (is_linux_shared()) return KILN_DYN_GLOBALS_ADDR;
    return KILN_GLOBALS_ADDR;
}

uint64_t kiln_heap_limit_addr(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) return KILN_PE_HEAP_LIMIT_ADDR;
    if (is_linux_shared()) return KILN_DYN_GLOBALS_ADDR + 8;
    return KILN_HEAP_LIMIT_ADDR;
}

uint64_t kiln_try_depth_addr(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) return KILN_PE_TRY_DEPTH_ADDR;
    if (is_linux_shared()) return KILN_DYN_GLOBALS_ADDR + 8 + KILN_HEAP_LIMIT_SIZE;
    return KILN_TRY_DEPTH_ADDR;
}

uint64_t kiln_modstate_addr(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) return KILN_PE_MODSTATE_ADDR;
    if (is_linux_shared()) return KILN_DYN_MODSTATE_ADDR;
    return KILN_MODSTATE_ADDR;
}

uint64_t kiln_try_handlers_addr(void) {
    if (kiln_get_target() == KILN_TARGET_WINDOWS) return KILN_PE_TRY_HANDLERS_ADDR;
    if (is_linux_shared()) return KILN_DYN_GLOBALS_ADDR + 8 + KILN_HEAP_LIMIT_SIZE + KILN_TRY_DEPTH_SIZE;
    return KILN_TRY_HANDLERS_ADDR;
}
