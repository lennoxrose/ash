"""Runs a kiln-produced PE32+ console program under the Unicorn x86-64 emulator.
kernel32 is faked in Python against the real filesystem; the program's own code
(including its Windows file-operation paths) is executed for real."""
import sys, os, struct, pefile
from unicorn import *
from unicorn.x86_const import *

path = sys.argv[1]
pe = pefile.PE(path)
base = pe.OPTIONAL_HEADER.ImageBase
mu = Uc(UC_ARCH_X86, UC_MODE_64)

def align(n, a=0x1000): return (n + a - 1) // a * a

image_size = align(pe.OPTIONAL_HEADER.SizeOfImage)
mu.mem_map(base, image_size + 0x1000)
data = open(path, 'rb').read()
mu.mem_write(base, data[:pe.OPTIONAL_HEADER.SizeOfHeaders])
for s in pe.sections:
    mu.mem_write(base + s.VirtualAddress, s.get_data())

STUBS = 0x7ff000000000
mu.mem_map(STUBS, 0x10000)
mu.mem_write(STUBS, b'\xc3' * 0x10000)
stub_names = {}
for i, entry in enumerate(pe.DIRECTORY_ENTRY_IMPORT):
    for imp in entry.imports:
        addr = STUBS + 16 * len(stub_names)
        stub_names[addr] = imp.name.decode()
        mu.mem_write(imp.address - base + base if imp.address >= base else base + imp.address, struct.pack('<Q', addr))

STACK = 0x7fff00000000
mu.mem_map(STACK, 0x400000)
HEAPS = [0x100000000]
mu.reg_write(UC_X86_REG_RSP, STACK + 0x400000 - 0x1000)

handles = {}
next_handle = [0x100]
out = sys.stdout.buffer
cmdline = base + image_size
mu.mem_write(cmdline, b'prog.exe\0')

def cstr(addr):
    b = bytearray()
    while True:
        c = mu.mem_read(addr, 1)[0]
        if c == 0: return bytes(b).decode()
        b.append(c)
        addr += 1

def arg(n):
    regs = [UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9]
    if n < 4: return mu.reg_read(regs[n])
    rsp = mu.reg_read(UC_X86_REG_RSP)
    return struct.unpack('<Q', mu.mem_read(rsp + 8 + 8 * n, 8))[0]  # return addr, then shadow(32) = args 1-4 slots

def stack_arg(n):
    rsp = mu.reg_read(UC_X86_REG_RSP)
    return struct.unpack('<Q', mu.mem_read(rsp + 8 + 8 * n, 8))[0]

def ret(v): mu.reg_write(UC_X86_REG_RAX, v & 0xFFFFFFFFFFFFFFFF)
INVALID = 0xFFFFFFFFFFFFFFFF
calls = []

def alloc(size):
    start = HEAPS[0]
    size = align(size, 0x1000)
    mu.mem_map(start, size)
    HEAPS[0] += size
    return start

def new_handle(obj):
    h = next_handle[0]; next_handle[0] += 4
    handles[h] = obj
    return h

def to_posix(p): return p.replace('\\', '/')

def hook(uc, address, size, user):
    if not (STUBS <= address < STUBS + 0x10000): return
    name = stub_names.get(address)
    if name is None: return
    calls.append(name)
    if name == 'ExitProcess':
        sys.stdout.flush(); out.flush()
        os._exit(arg(0) & 0xFF)
    elif name == 'GetStdHandle': ret({0xFFFFFFF6: 1, 0xFFFFFFF5: 2, 0xFFFFFFF4: 3}.get(arg(0) & 0xFFFFFFFF, INVALID))
    elif name == 'WriteFile':
        h, buf, n, written = arg(0), arg(1), arg(2), arg(3)
        d = bytes(uc.mem_read(buf, n))
        if h == 2 or h == 3: sys.stderr.buffer.write(d); sys.stderr.flush()
        elif h == 1 or h == 0x1: out.write(d); out.flush()
        else:
            f = handles.get(h)
            if f is None: ret(0); return
            f['file'].write(d)
        if written: uc.mem_write(written, struct.pack('<I', n))
        ret(1)
    elif name == 'ReadFile':
        h, buf, n, got = arg(0), arg(1), arg(2), arg(3)
        f = handles.get(h)
        d = f['file'].read(n) if f else b''
        uc.mem_write(buf, d)
        if got: uc.mem_write(got, struct.pack('<I', len(d)))
        ret(1 if f else 0)
    elif name == 'CreateFileA':
        p, access, share, sa, disp = to_posix(cstr(arg(0))), arg(1), arg(2), arg(3), stack_arg(4)
        try:
            if disp == 3: f = open(p, 'rb')                      # OPEN_EXISTING
            elif disp == 2: f = open(p, 'wb')                    # CREATE_ALWAYS
            elif disp == 4: f = open(p, 'ab')                    # OPEN_ALWAYS
            else: ret(INVALID); return
            ret(new_handle({'file': f}))
        except OSError:
            ret(INVALID)
    elif name == 'CloseHandle':
        f = handles.pop(arg(0), None)
        if f and 'file' in f: f['file'].close()
        ret(1)
    elif name == 'GetFileSize':
        f = handles.get(arg(0)); ret(os.fstat(f['file'].fileno()).st_size if f else INVALID)
    elif name == 'VirtualAlloc': ret(alloc(arg(1)))
    elif name == 'GetCommandLineA': ret(cmdline)
    elif name == 'MoveFileA':
        try: os.rename(to_posix(cstr(arg(0))), to_posix(cstr(arg(1)))); ret(1)
        except OSError: ret(0)
    elif name == 'DeleteFileA':
        try: os.remove(to_posix(cstr(arg(0)))); ret(1)
        except OSError: ret(0)
    elif name == 'CreateDirectoryA':
        try: os.mkdir(to_posix(cstr(arg(0)))); ret(1)
        except OSError: ret(0)
    elif name == 'FindFirstFileA':
        pattern = to_posix(cstr(arg(0)))
        assert pattern.endswith('/*'), pattern
        try: names = ['.', '..'] + sorted(os.listdir(pattern[:-2]))
        except OSError: ret(INVALID); return
        h = new_handle({'names': names, 'i': 0})
        fill_find(uc, arg(1), h); ret(h)
    elif name == 'FindNextFileA':
        ret(1 if fill_find(uc, arg(1), arg(0)) else 0)
    elif name == 'FindClose':
        handles.pop(arg(0), None); ret(1)
    else:
        raise RuntimeError('unimplemented kernel32 call: ' + name)

def fill_find(uc, buf, h):
    st = handles[h]
    if st['i'] >= len(st['names']): return False
    nm = st['names'][st['i']].encode(); st['i'] += 1
    uc.mem_write(buf, b'\0' * 320)
    uc.mem_write(buf + 44, nm + b'\0')
    return True

mu.hook_add(UC_HOOK_CODE, hook, begin=STUBS, end=STUBS + 0x10000)
entry = base + pe.OPTIONAL_HEADER.AddressOfEntryPoint
try:
    mu.emu_start(entry, 0, count=2_000_000_000)
except UcError as e:
    sys.stdout.flush(); out.flush()
    print('EMULATION ERROR:', e, 'at rip', hex(mu.reg_read(UC_X86_REG_RIP)), file=sys.stderr)
    sys.exit(99)
sys.stdout.flush(); out.flush()
