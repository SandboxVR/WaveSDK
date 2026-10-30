"""Build a version-locked startup VRS policy for the bundled ARM64 runtime.

Only the exact cap60 baseline is accepted.  The original file remains intact
except for three branch instructions, the last RW segment's BSS size, and the
unused PT_NOTE program-header entry reused for an appended RX mapping.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

BASELINE_SHA256 = '7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
STATE_VA = 0xA4FA00
STATE_SIZE = 256
RX_VA = 0xA51000
HEADER_MAGIC = b'SSVRVRS1'
ABI_VERSION = 2
ORIGINAL_RW = (1, 6, 0xA32908, 0xA34908, 0xA34908, 0x7A10, 0x1B010, 0x1000)
ORIGINAL_NOTE = (4, 4, 0x238, 0x238, 0x238, 0xBC, 0xBC, 4)
HOOKS = {
    'constructor_hook': (0x5BDB50, bytes.fromhex('68823039')),
    'unsupported_hook': (0x5C2694, bytes.fromhex('7f823039')),
    'draw_hook': (0x5C6FF0, bytes.fromhex('68827039')),
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(command: list[str]) -> None:
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'Tool failed ({result.returncode}): {command[0]}\n{result.stderr}')


def ndk_bin(ndk: Path) -> Path:
    root = ndk / 'toolchains' / 'llvm' / 'prebuilt'
    names = ('windows-x86_64', 'linux-x86_64', 'darwin-x86_64')
    for name in names:
        candidate = root / name / 'bin'
        if candidate.is_dir():
            return candidate
    raise ValueError(f'No supported NDK LLVM toolchain under {ndk}')


def executable(folder: Path, name: str) -> str:
    candidate = folder / (name + ('.exe' if os.name == 'nt' else ''))
    if not candidate.is_file():
        raise ValueError(f'Missing NDK tool: {candidate}')
    return str(candidate)


def elf_sections(data: bytes) -> dict[str, tuple]:
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    if header[0][:6] != b'\x7fELF\x02\x01' or header[2] != 183:
        raise ValueError('Expected little-endian AArch64 ELF64')
    offset, size, count, names_index = header[6], header[11], header[12], header[13]
    rows = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * size) for i in range(count)]
    names = data[rows[names_index][4]:rows[names_index][4] + rows[names_index][5]]
    return {names[row[0]:names.find(b'\0', row[0])].decode(): row for row in rows}


def elf_symbols(data: bytes) -> dict[str, int]:
    sections = elf_sections(data)
    table, names = sections['.symtab'], sections['.strtab']
    strings = data[names[4]:names[4] + names[5]]
    result = {}
    for offset in range(table[4], table[4] + table[5], table[9]):
        name, _, _, _, value, _ = struct.unpack_from('<IBBHQQ', data, offset)
        if name:
            result[strings[name:strings.find(b'\0', name)].decode()] = value
    return result


def branch(source: int, destination: int) -> bytes:
    delta = destination - source
    if delta % 4 or not -(1 << 27) <= delta < (1 << 27):
        raise ValueError('Trampoline branch is unaligned or out of range')
    return struct.pack('<I', 0x14000000 | ((delta // 4) & 0x3FFFFFF))


def patch_runtime(data: bytes, ndk: Path, build_dir: Path) -> tuple[bytes, dict]:
    if sha(data) != BASELINE_SHA256:
        raise ValueError('Unknown runtime SHA-256; refusing version-dependent patch')
    for address, expected in HOOKS.values():
        if data[address:address + 4] != expected:
            raise ValueError(f'Unexpected original instruction at {address:#x}')
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    phoff, phsize, phcount = header[5], header[9], header[10]
    if phsize != 56 or phcount != 9:
        raise ValueError('Unexpected baseline program-header layout')
    rows = [struct.unpack_from('<IIQQQQQQ', data, phoff + i * phsize) for i in range(phcount)]
    if rows[3] != ORIGINAL_RW or rows[8] != ORIGINAL_NOTE:
        raise ValueError('Unexpected RW or NOTE segment; refusing layout change')
    if ORIGINAL_RW[3] + ORIGINAL_RW[6] != 0xA4F918:
        raise AssertionError('Incorrect BSS baseline')

    build_dir.mkdir(parents=True, exist_ok=True)
    toolchain = ndk_bin(ndk)
    source = Path(__file__).with_name('native_hook.S')
    obj = build_dir / 'native_hook.o'
    linked = build_dir / 'native_hook.elf'
    script = build_dir / 'native_hook.ld'
    script.write_text(
        f'STATE = {STATE_VA:#x}; GETENV_PLT = 0x9cfca0; '
        'CONSTRUCTOR_RETURN = 0x5bdb54; UNSUPPORTED_RETURN = 0x5c2698; DRAW_RETURN = 0x5c6ff4;\n'
        f'SECTIONS {{ . = {RX_VA:#x}; .text : {{ *(.text.vrs) }} '
        '/DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) } }\n', encoding='ascii')
    run([executable(toolchain, 'clang'), '--target=aarch64-linux-android29', '-c',
         '-x', 'assembler-with-cpp', str(source), '-o', str(obj)])
    run([executable(toolchain, 'ld.lld'), '-T', str(script), '--build-id=none',
         '--no-undefined', str(obj), '-o', str(linked)])
    elf = linked.read_bytes()
    symbols = elf_symbols(elf)
    text = elf_sections(elf)['.text']
    code = elf[text[4]:text[4] + text[5]]
    if text[3] != RX_VA or code[:8] != HEADER_MAGIC:
        raise ValueError('Unexpected linked trampoline header')
    if struct.unpack_from('<IIQ', code, 8) != (ABI_VERSION, STATE_SIZE, STATE_VA):
        raise ValueError('Unexpected linked trampoline ABI')
    if '.rela.text' in elf_sections(elf) or '.rela.dyn' in elf_sections(elf):
        raise ValueError('Unexpected runtime relocation in trampoline')

    output = bytearray(data)
    changes = []

    def change(offset: int, replacement: bytes, purpose: str) -> None:
        original = bytes(output[offset:offset + len(replacement)])
        output[offset:offset + len(replacement)] = replacement
        changes.append({'file_offset': hex(offset), 'size': len(replacement),
                        'before_hex': original.hex(), 'after_hex': replacement.hex(),
                        'purpose': purpose})

    for name, (address, _) in HOOKS.items():
        change(address, branch(address, symbols[name]), name)
    new_memory_size = STATE_VA + STATE_SIZE - ORIGINAL_RW[3]
    change(phoff + 3 * phsize + 40, struct.pack('<Q', new_memory_size), 'extend RW BSS only')
    rx_offset = (len(data) + 4095) & ~4095
    new_rx = (1, 5, rx_offset, RX_VA, RX_VA, len(code), len(code), 4096)
    change(phoff + 8 * phsize, struct.pack('<IIQQQQQQ', *new_rx), 'reuse PT_NOTE entry for RX trampoline')
    output.extend(bytes(rx_offset - len(output)))
    output.extend(code)

    # Inverse check proves no original byte changed outside the declared ranges.
    inverse = bytearray(output[:len(data)])
    for item in changes:
        offset = int(item['file_offset'], 16)
        inverse[offset:offset + item['size']] = bytes.fromhex(item['before_hex'])
    if bytes(inverse) != data:
        raise AssertionError('Undeclared original-byte change')
    manifest = {
        'schema_version': 1, 'status': 'experimental; offline validation only',
        'baseline_sha256': sha(data), 'runtime_sha256': sha(output),
        'baseline_size': len(data), 'runtime_size': len(output),
        'abi_version': ABI_VERSION, 'state_va': hex(STATE_VA), 'state_size': STATE_SIZE,
        'rx_va': hex(RX_VA), 'rx_file_offset': hex(rx_offset), 'rx_size': len(code),
        'rx_sha256': sha(code), 'native_hook_source_sha256': sha(source.read_bytes()),
        'hooks': {name: {'patch_va': hex(address), 'trampoline_va': hex(symbols[name])}
                  for name, (address, _) in HOOKS.items()},
        'changes': changes,
        'appended_range': {'file_offset': hex(len(data)), 'size': len(output) - len(data)},
        'preserved': ['all original bytes outside listed patch ranges',
                      'original section table and note bytes',
                      'cap60 and prior native changes'],
        'permissions': 'original RX/RW mappings preserved; new RX mapping; no RWX',
        'validation_limits': ['No Android loader or headset execution is implied by building.',
                              'Only this version-locked ARM64 runtime is supported.'],
    }
    (build_dir / 'native_hook.bin').write_bytes(code)
    (build_dir / 'native_hook_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return bytes(output), manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Output must differ from input')
    build = args.build_dir or Path(tempfile.mkdtemp(prefix='wave-vrs-build-'))
    output, manifest = patch_runtime(args.input.read_bytes(), args.ndk, build)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
