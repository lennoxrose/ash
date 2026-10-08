#ifndef KILN_LAYOUT_H
#define KILN_LAYOUT_H
#include <stdint.h>

// Every place in codegen that needs "the absolute address of code offset
// N" or "the absolute address of a globals-region field" used to hardcode
// KILN_LOAD_BASE+KILN_CODE_START_OFFSET / KILN_TRY_DEPTH_ADDR / etc.
// directly from elf/H/elf_writer.h. Now that kiln can target two different
// executable formats with two different fixed base addresses (see
// elf/H/elf_writer.h vs pe/H/pe_writer.h), those hardcodes would each need
// their own target check -- so instead every call site goes through one
// of these, and the target check lives here exactly once.
uint64_t kiln_modstate_addr(void);     // start of the module-state slot table (16 bytes per slot)
uint64_t kiln_code_base(void);       // add a code offset to get an absolute address
uint64_t kiln_heap_ptr_addr(void);
uint64_t kiln_try_depth_addr(void);
uint64_t kiln_try_handlers_addr(void);

#endif
