"""Exercise the actual linked AArch64 trampolines with Unicorn 2.1.4.

Usage: python test_native_hook.py --runtime cap60.so --ndk /path/to/ndk
Install the test-only dependency with: python -m pip install unicorn==2.1.4
This verifies CPU control flow and memory effects, not Android/GPU execution.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import tempfile
import unittest

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm64_const as reg

import patch_runtime as patch

SEALED = 0x80000000
RENDERER = 0x20000000
SECOND_RENDERER = 0x20010000
STACK = 0x10000000
ENVIRONMENT = 0x30000000
GPRS = [getattr(reg, f'UC_ARM64_REG_X{i}') for i in range(31)]
SIMD = [getattr(reg, f'UC_ARM64_REG_Q{i}') for i in range(32)]
RETURNS = {'constructor_hook': 0x5BDB54, 'unsupported_hook': 0x5C2698, 'draw_hook': 0x5C6FF4}


class Machine:
    def __init__(self, runtime: bytes, manifest: dict, environment: str | None = None):
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.uc.mem_map(0, 0xA52000)
        header = struct.unpack_from('<16sHHIQQQIHHHHHH', runtime)
        for i in range(header[10]):
            row = struct.unpack_from('<IIQQQQQQ', runtime, header[5] + i * header[9])
            if row[0] == 1:
                self.uc.mem_write(row[3], runtime[row[2]:row[2] + row[5]])
        for address, size in ((STACK, 0x10000), (RENDERER, 0x20000), (ENVIRONMENT, 0x10000)):
            self.uc.mem_map(address, size)
        self.environment = environment
        self.getenv_calls = 0
        self.on_getenv = None
        self.on_instruction = None
        self.manifest = manifest
        self.stop = 0
        self.uc.hook_add(UC_HOOK_CODE, self._code)

    def _code(self, uc: Uc, address: int, size: int, _):
        if self.on_instruction:
            self.on_instruction(self, address)
        if address == self.stop:
            uc.emu_stop()
            return
        if address != 0x9CFCA0:
            return
        key = bytes(uc.mem_read(uc.reg_read(GPRS[0]), 27)).split(b'\0')[0]
        assert key == b'SSVR_WAVE_COMPOSITOR_VRS_V1', key
        self.getenv_calls += 1
        if self.on_getenv:
            self.on_getenv(self)
        return_pc = uc.reg_read(reg.UC_ARM64_REG_LR)
        # Deliberately clobber every SIMD bit, caller-saved GPR and NZCV to prove
        # preservation, including q8-q15 upper halves and q16-q31.
        for i in range(18):
            uc.reg_write(GPRS[i], 0xCA11000000000000 + i)
        for i, register in enumerate(SIMD):
            uc.reg_write(register, ((1 << 128) - 1) ^ i)
        uc.reg_write(reg.UC_ARM64_REG_NZCV, 0x50000000)
        if self.environment is None:
            uc.reg_write(GPRS[0], 0)
        else:
            uc.mem_write(ENVIRONMENT, self.environment.encode('ascii') + b'\0')
            uc.reg_write(GPRS[0], ENVIRONMENT)
        uc.reg_write(reg.UC_ARM64_REG_PC, return_pc)

    def state32(self, offset: int) -> int:
        return struct.unpack('<I', self.uc.mem_read(patch.STATE_VA + offset, 4))[0]

    def state64(self, offset: int) -> int:
        return struct.unpack('<Q', self.uc.mem_read(patch.STATE_VA + offset, 8))[0]

    def write32(self, offset: int, value: int) -> None:
        self.uc.mem_write(patch.STATE_VA + offset, struct.pack('<I', value))

    def run_hook(self, name: str, input_valid: int = 1, renderer: int = RENDERER) -> dict:
        expected = {}
        for i, register in enumerate(GPRS):
            value = 0x1234000000000000 + i * 0x101
            if i == 19:
                value = renderer
            if i == 8:
                value = input_valid
            self.uc.reg_write(register, value)
            expected[register] = value
        for i, register in enumerate(SIMD):
            value = (0x1122334455667788 << 64) | (0x8877665544332211 ^ i)
            self.uc.reg_write(register, value)
            expected[register] = value
        self.uc.reg_write(reg.UC_ARM64_REG_SP, STACK + 0xFFF0)
        self.uc.reg_write(reg.UC_ARM64_REG_NZCV, 0xA0000000)
        expected[reg.UC_ARM64_REG_SP] = STACK + 0xFFF0
        expected[reg.UC_ARM64_REG_NZCV] = 0xA0000000
        self.stop = RETURNS[name]
        start = int(self.manifest['hooks'][name]['patch_va'], 16)
        self.uc.emu_start(start, 0, count=10000)
        if self.uc.reg_read(reg.UC_ARM64_REG_PC) != self.stop:
            raise AssertionError('Trampoline did not reach the original continuation')
        if name in ('constructor_hook', 'draw_hook'):
            expected[GPRS[8]] = self.uc.mem_read(renderer + 3104, 1)[0]
        for register, value in expected.items():
            actual = self.uc.reg_read(register)
            if actual != value:
                raise AssertionError(f'Register {register}: {actual:#x} != {value:#x}')
        return expected


class NativeHookTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = OPTIONS.runtime.read_bytes()
        cls.build = Path(tempfile.mkdtemp(prefix='wave-vrs-native-tests-'))
        cls.runtime, cls.manifest = patch.patch_runtime(cls.baseline, OPTIONS.ndk, cls.build / 'first')

    def machine(self, environment=None):
        return Machine(self.runtime, self.manifest, environment)

    def test_off_forces_cached_field_and_original_log_skip(self):
        m = self.machine('Off')
        m.run_hook('constructor_hook')
        self.assertEqual(m.state32(0), SEALED | 1)
        self.assertEqual(m.state32(4), 2)
        self.assertEqual(m.state32(8), 2)
        self.assertEqual(m.state32(20), 1)
        self.assertEqual(m.state32(24), 0)
        self.assertEqual(m.uc.mem_read(RENDERER + 3104, 1), b'\0')
        m.stop = 0
        m.uc.emu_start(RETURNS['constructor_hook'], 0, count=1)
        self.assertEqual(m.uc.reg_read(reg.UC_ARM64_REG_PC), 0x5BDB74)

    def test_default_preserves_native_validity_and_original_log_path(self):
        for environment in (None, 'DeviceDefault'):
            for valid in (0, 1):
                with self.subTest(environment=environment, valid=valid):
                    m = self.machine(environment)
                    m.run_hook('constructor_hook', valid)
                    self.assertEqual(m.state32(24), valid)
                    self.assertEqual(m.state32(20), valid)
                    self.assertEqual(m.getenv_calls, 1)
                    m.stop = 0
                    m.uc.emu_start(RETURNS['constructor_hook'], 0, count=1)
                    self.assertEqual(m.uc.reg_read(reg.UC_ARM64_REG_PC), 0x5BDB58 if valid else 0x5BDB74)

    def test_explicit_api_policy_bypasses_environment(self):
        for policy, environment, effective in ((1, 'DeviceDefault', 0), (2, 'Off', 1)):
            with self.subTest(policy=policy):
                m = self.machine(environment)
                m.write32(0, policy)
                m.run_hook('constructor_hook')
                self.assertEqual(m.state32(0), SEALED | policy)
                self.assertEqual(m.state32(4), 4)
                self.assertEqual(m.state32(8), 1)
                self.assertEqual(m.state32(24), effective)
                self.assertEqual(m.getenv_calls, 0)

    def test_invalid_environment_is_reported_and_preserves_device_default(self):
        for environment in ('', 'off', 'OFF', ' Off', 'DeviceDefaultExtra', '1'):
            with self.subTest(environment=environment):
                m = self.machine(environment)
                m.run_hook('constructor_hook')
                self.assertEqual(m.state32(0), SEALED)
                self.assertEqual(m.state32(4), 3)
                self.assertEqual(m.state32(12), 1)
                self.assertEqual(m.state32(24), 1)

    def test_api_winning_environment_read_race_is_honored(self):
        m = self.machine('DeviceDefault')
        m.on_getenv = lambda machine: machine.write32(0, 1)
        m.run_hook('constructor_hook')
        self.assertEqual(m.state32(0), SEALED | 1)
        self.assertEqual(m.state32(4), 4)
        self.assertEqual(m.state32(24), 0)
        self.assertEqual(m.getenv_calls, 1)

    def test_policy_is_sealed_across_recreation_and_clears_old_tokens(self):
        m = self.machine('Off')
        m.run_hook('constructor_hook')
        m.run_hook('draw_hook')
        m.run_hook('unsupported_hook')
        self.assertEqual(m.state64(56), RENDERER)
        self.assertEqual(m.state64(64), RENDERER)
        m.environment = 'DeviceDefault'
        # Same allocator address reused: both prior observation tokens must clear.
        m.run_hook('constructor_hook', renderer=RENDERER)
        self.assertEqual(m.getenv_calls, 1)
        self.assertEqual(m.state32(16), 2)
        self.assertEqual(m.state32(24), 0)
        self.assertEqual(m.state64(48), RENDERER)
        self.assertEqual(m.state64(56), 0)
        self.assertEqual(m.state64(64), 0)
        self.assertEqual(m.state32(72), 0)

    def test_draw_and_unsupported_record_actual_renderer_field(self):
        m = self.machine('DeviceDefault')
        m.run_hook('constructor_hook')
        m.run_hook('draw_hook')
        self.assertEqual(m.state64(32), 1)
        self.assertEqual(m.state32(40), 1)
        self.assertEqual(m.state64(56), RENDERER)
        m.run_hook('unsupported_hook')
        self.assertEqual(m.state32(28), 1)
        self.assertEqual(m.state32(24), 0)
        self.assertEqual(m.state64(64), RENDERER)
        m.run_hook('draw_hook')
        self.assertEqual(m.state64(32), 2)
        self.assertEqual(m.state32(40), 0)

    def test_original_resource_and_draw_branches_use_the_forced_cache(self):
        for environment, enabled in (('Off', 0), ('DeviceDefault', 1)):
            with self.subTest(environment=environment):
                m = self.machine(environment)
                m.run_hook('constructor_hook')
                # Unmodified original GPU resource gate: LDRB; CBZ.
                m.stop = 0
                m.uc.emu_start(0x5C23CC, 0, count=2)
                self.assertEqual(m.uc.reg_read(reg.UC_ARM64_REG_PC),
                                 0x5C23D4 if enabled else 0x5C25D0)
                m.run_hook('draw_hook')
                # Original draw predicate also includes a separate stack guard.
                # Keep that guard false so the cache alone selects the branch.
                m.uc.reg_write(reg.UC_ARM64_REG_SP, STACK + 0xF000)
                m.uc.mem_write(STACK + 0xF000 + 56, bytes(4))
                m.stop = 0
                m.uc.emu_start(RETURNS['draw_hook'], 0, count=6)
                self.assertEqual(m.uc.reg_read(reg.UC_ARM64_REG_PC),
                                 0x5C70C8 if enabled else 0x5C700C)

    def test_old_renderer_draw_token_does_not_match_new_renderer(self):
        m = self.machine('DeviceDefault')
        m.run_hook('constructor_hook')
        m.run_hook('constructor_hook', renderer=SECOND_RENDERER)
        m.run_hook('draw_hook', renderer=RENDERER)
        self.assertNotEqual(m.state64(48), m.state64(56))
        m.run_hook('draw_hook', renderer=SECOND_RENDERER)
        self.assertEqual(m.state64(48), m.state64(56))

    def test_every_diagnostic_change_is_inside_shared_odd_sequence(self):
        for name in RETURNS:
            with self.subTest(hook=name):
                m = self.machine('DeviceDefault')
                if name != 'constructor_hook':
                    m.run_hook('constructor_hook')
                previous = bytes(m.uc.mem_read(patch.STATE_VA + 4, 68))
                changed = []
                sequences = []

                def observe(machine, address):
                    nonlocal previous
                    current = bytes(machine.uc.mem_read(patch.STATE_VA + 4, 68))
                    sequence = machine.state32(76)
                    sequences.append(sequence)
                    if current != previous:
                        self.assertEqual(sequence & 1, 1, f'unprotected change at {address:#x}')
                        self.assertEqual(machine.state32(72), 1)
                        changed.append(address)
                    previous = current

                m.on_instruction = observe
                before = m.state32(76)
                m.run_hook(name)
                self.assertTrue(changed)
                self.assertIn(before + 1, sequences)
                self.assertEqual(m.state32(76), before + 2)
                self.assertEqual(m.state32(72), 0)

    def test_old_renderer_mid_draw_publication_is_rejected_by_sequence(self):
        m = self.machine('DeviceDefault')
        m.run_hook('constructor_hook', input_valid=1, renderer=RENDERER)
        m.run_hook('constructor_hook', input_valid=0, renderer=SECOND_RENDERER)
        m.run_hook('draw_hook', renderer=SECOND_RENDERER)
        reader_start = m.state32(76)
        self.assertEqual(m.state32(40), 0)
        self.assertEqual(m.state64(48), m.state64(56))
        torn_windows = []

        def reader_snapshot(machine, address):
            before = machine.state32(76)
            latest, draw = machine.state64(48), machine.state64(56)
            flag = machine.state32(40)
            after = machine.state32(76)
            # This exact window fooled the old double-token protocol: the old
            # renderer has written flag=1 while the token still names the new
            # renderer whose actual cached byte is zero.
            if latest == draw == SECOND_RENDERER and flag == 1:
                torn_windows.append(address)
                self.assertTrue(before & 1)
                self.assertFalse(before == after and not (before & 1))
                self.assertFalse(reader_start == after and not (reader_start & 1))

        m.on_instruction = reader_snapshot
        m.run_hook('draw_hook', renderer=RENDERER)
        self.assertTrue(torn_windows, 'Test did not visit the adversarial publication window')
        self.assertEqual(m.state32(76), reader_start + 2)
        self.assertNotEqual(m.state64(48), m.state64(56))

    def test_all_publishers_wait_for_the_same_writer_lock(self):
        for name in RETURNS:
            with self.subTest(hook=name):
                m = self.machine('DeviceDefault')
                m.run_hook('constructor_hook')
                baseline = bytes(m.uc.mem_read(patch.STATE_VA + 4, 68))
                m.write32(72, 1)
                lock_reads = 0
                released = False

                def other_writer_release(machine, address):
                    nonlocal lock_reads, released
                    if released:
                        return
                    self.assertEqual(bytes(machine.uc.mem_read(patch.STATE_VA + 4, 68)), baseline)
                    # LDAXR w10, [x13], with x13 selecting the shared lock.
                    if (machine.uc.reg_read(GPRS[13]) == patch.STATE_VA + 72
                            and bytes(machine.uc.mem_read(address, 4)) == bytes.fromhex('aafd5f88')):
                        lock_reads += 1
                        if lock_reads == 5:
                            machine.write32(72, 0)
                            released = True

                m.on_instruction = other_writer_release
                before = m.state32(76)
                m.run_hook(name)
                self.assertTrue(released)
                self.assertEqual(lock_reads, 5)
                self.assertEqual(m.state32(76), before + 2)
                self.assertEqual(m.state32(72), 0)

    def test_publication_sequence_wraps_from_odd_max_to_even_zero(self):
        for name in RETURNS:
            with self.subTest(hook=name):
                m = self.machine('DeviceDefault')
                m.run_hook('constructor_hook')
                m.write32(76, 0xFFFFFFFE)
                seen = []
                m.on_instruction = lambda machine, address: seen.append(machine.state32(76))
                m.run_hook(name)
                self.assertIn(0xFFFFFFFF, seen)
                self.assertEqual(m.state32(76), 0)
                self.assertEqual(m.state32(72), 0)

    def test_sealed_constructor_waits_for_first_policy_metadata(self):
        m = self.machine('Off')
        m.write32(0, SEALED | 1)
        waiting_reads = 0
        ready = False

        def publish_first_winner_metadata(machine, address):
            nonlocal waiting_reads, ready
            if ready:
                return
            self.assertEqual(machine.state32(16), 0)
            # LDAR w10, [x13] from envStatus: the already-sealed constructor
            # must wait instead of publishing a renderer with unchecked source.
            if (machine.uc.reg_read(GPRS[13]) == patch.STATE_VA + 4
                    and bytes(machine.uc.mem_read(address, 4)) == bytes.fromhex('aafddf88')):
                waiting_reads += 1
                if waiting_reads == 5:
                    machine.write32(76, 1)
                    machine.write32(8, 1)
                    machine.write32(4, 4)
                    machine.write32(76, 2)
                    ready = True

        m.on_instruction = publish_first_winner_metadata
        m.run_hook('constructor_hook')
        self.assertTrue(ready)
        self.assertEqual(m.getenv_calls, 0)
        self.assertEqual(m.state32(4), 4)
        self.assertEqual(m.state32(8), 1)
        self.assertEqual(m.state32(16), 1)
        self.assertEqual(m.state32(24), 0)
        self.assertEqual(m.state32(76), 4)

    def test_byte_preservation_layout_and_reproducible_output(self):
        rx = int(self.manifest['rx_file_offset'], 16)
        self.assertEqual(self.runtime[rx:rx + 8], b'SSVRVRS1')
        self.assertEqual(struct.unpack_from('<IIQ', self.runtime, rx + 8),
                         (2, 256, 0xA4FA00))
        inverse = bytearray(self.runtime[:len(self.baseline)])
        for change in self.manifest['changes']:
            offset = int(change['file_offset'], 16)
            inverse[offset:offset + change['size']] = bytes.fromhex(change['before_hex'])
        self.assertEqual(bytes(inverse), self.baseline)
        self.assertEqual(self.runtime[0xA329A4:0xA329A8], bytes.fromhex('8fc2753d'))
        h = struct.unpack_from('<16sHHIQQQIHHHHHH', self.runtime)
        loads = []
        for i in range(h[10]):
            row = struct.unpack_from('<IIQQQQQQ', self.runtime, h[5] + i * h[9])
            if row[0] != 1:
                continue
            self.assertNotEqual(row[1] & 7, 7)
            self.assertEqual(row[2] % row[7], row[3] % row[7])
            self.assertLessEqual(row[5], row[6])
            loads.append(row)
        for first, second in zip(loads, loads[1:]):
            self.assertLessEqual(first[3] + first[6], second[3])
        again, _ = patch.patch_runtime(self.baseline, OPTIONS.ndk, self.build / 'second')
        self.assertEqual(again, self.runtime)

    def test_unknown_hash_and_opcode_fail_closed(self):
        mutated = bytearray(self.baseline)
        mutated[0x5BDB50] ^= 1
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            patch.patch_runtime(bytes(mutated), OPTIONS.ndk, self.build / 'rejected')
        old_hash = patch.BASELINE_SHA256
        try:
            patch.BASELINE_SHA256 = patch.sha(mutated)
            with self.assertRaisesRegex(ValueError, 'instruction'):
                patch.patch_runtime(bytes(mutated), OPTIONS.ndk, self.build / 'rejected-opcode')
        finally:
            patch.BASELINE_SHA256 = old_hash


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True, help='Unpatched exact cap60 runtime')
    parser.add_argument('--ndk', type=Path, required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
