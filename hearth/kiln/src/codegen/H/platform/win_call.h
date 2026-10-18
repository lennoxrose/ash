#ifndef KILN_WIN_CALL_H
#define KILN_WIN_CALL_H
#include "codegen/H/emit/emit.h"
#include "pe/H/pe_writer.h"

// Windows x64 calling convention -- the caller-side sequence for calling
// an imported kernel32.dll function. New territory for this codebase:
// unlike kiln's own internal `fn` convention (stack-push based, see
// parser/C/statements/parser_control.c), this has to match a REAL external ABI, with
// rules kiln has never had to satisfy before:
//   - first four integer/pointer args in RCX, RDX, R8, R9
//   - a mandatory 32-byte "shadow space" the CALLEE is allowed to spill
//     its register args into, reserved by the CALLER even if the callee
//     never uses it
//   - RSP must be 16-byte aligned at the `call` instruction
//   - 5th+ args go on the stack, above the shadow space
//
// RSP alignment is the sharp edge: kiln's codegen never tracks RSP parity
// (arbitrary push/pop sequences leave it unpredictable at any given
// point, which was fine when the only "call-like" thing was a raw Linux
// `syscall` -- syscalls have no alignment requirement at all). So rather
// than trying to prove alignment holds at every call site,
// win_call_begin ALIGNS DYNAMICALLY at runtime (`and rsp, -16`) and saves
// the pre-align RSP to a fixed scratch slot; win_call_end restores that
// exact saved value, so the surrounding codegen's own RSP bookkeeping is
// completely undisturbed regardless of what it was before the call.
//
// Argument marshalling note: if a source value already lives in one of
// RCX/RDX/R8/R9 from earlier in this same sequence, load higher-indexed
// args before lower ones (or route through a scratch register like RAX/
// RSI first) -- win_call_arg_* does not detect or resolve register
// clobbers between argument slots.
void win_call_begin(CodeBuf *code, int stack_arg_count);
void win_call_arg_reg(CodeBuf *code, int index, Reg src);        // index 0-3 -> RCX/RDX/R8/R9
void win_call_arg_imm64(CodeBuf *code, int index, uint64_t imm);
void win_call_stack_arg_reg(CodeBuf *code, int stack_index, Reg src);  // stack_index 0-based, for args 5+
void win_call_stack_arg_imm64(CodeBuf *code, int stack_index, uint64_t imm);
// Calls through `which`'s IAT slot -- see pe_writer.h for why this is a
// direct absolute-address load+call rather than RIP-relative addressing.
// Result comes back in RAX, standard Windows x64 return convention.
void win_call_import(CodeBuf *code, PeImport which);
void win_call_end(CodeBuf *code);

// --link=shared: calls into libkilnrt.dll instead of a local routine.
// See win_call.c for why this needs its own primitive rather than
// reusing win_call_import.
void win_call_runtime_import(CodeBuf *code, RuntimeImport which);

#endif
