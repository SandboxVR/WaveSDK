"""Execute shipped Wave ARM64 predictor/getter offline with explicit OS stubs.

This models selected object fields; it is not a compositor or headset simulation.
Original ELF instructions execute under Unicorn. No application/runtime is deployed.
"""
import argparse
import hashlib
import io
import json
import math
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

PROFILES = {
    'b4e0e369b53e1ca016a33ddacc2af31400b134f05372532b0b02153c70c3a72b': {
        'version':'6.2.0-u25', 'getter':0x5E8C8C, 'estimator':0x5E8AE0,
        'cap':0xA33F44, 'cap_got':0xA212A8, 'clock':0x9CDEA0,
        'vsync':0x611160, 'override':0x9CF480, 'predict_override':0x9CFB80,
        'log':0x9CCB40, 'stack_fail':0x9CCB30},
    '7abd01d50ac948b38a41a55cef5a461b018ed29e1856d8198aa5deeb6db29d18': {
        'version':'6.2.0-u09', 'getter':0x5E8F60, 'estimator':0x5E8DB4,
        'cap':0xA349A4, 'cap_got':0xA21D00, 'clock':0x9CE920,
        'vsync':0x611434, 'override':0x9CFF00, 'predict_override':0x9D0600,
        'log':0x9CD5C0, 'stack_fail':0x9CD5B0},
}
STACK = 0x10000000
OBJECT = 0x11000000
VTABLE = 0x11010000
CONFIG = 0x11020000
TWS_OBJECT = 0x11030000
TLS = 0x11040000
STOP = 0x12000000
REFRESH_STUB = STOP + 0x100
NOW_NS = 123_456_789_000


class Replay:
    def __init__(self, path, original_reference=None):
        blob = path.read_bytes()
        self.sha = hashlib.sha256(blob).hexdigest()
        identity = self.sha
        if original_reference is not None:
            reference = original_reference.read_bytes()
            identity = hashlib.sha256(reference).hexdigest()
            if identity not in PROFILES:
                raise ValueError('Unknown reference runtime')
            elf_ref = ELFFile(io.BytesIO(reference))
            cap_address = PROFILES[identity]['cap']
            matching = [s for s in elf_ref.iter_segments() if s['p_type']=='PT_LOAD'
                        and s['p_vaddr'] <= cap_address < s['p_vaddr']+s['p_filesz']]
            assert len(matching) == 1
            segment = matching[0]
            offset = segment['p_offset'] + cap_address - segment['p_vaddr']
            assert reference[offset:offset+4] == struct.pack('<f',.05)
            expected = reference[:offset]+struct.pack('<f',.06)+reference[offset+4:]
            assert blob == expected, 'Candidate differs beyond approved cap initializer'
        if identity not in PROFILES:
            raise ValueError("Unexpected runtime identity; addresses are version-specific")
        self.profile = PROFILES[identity]
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        elf = ELFFile(io.BytesIO(blob))
        segments = [p for p in elf.iter_segments() if p['p_type'] == 'PT_LOAD']
        end = max(p['p_vaddr'] + p['p_memsz'] for p in segments)
        self.uc.mem_map(0, (end + 4095) & ~4095)
        for segment in segments:
            self.uc.mem_write(segment['p_vaddr'], segment.data())
        self.uc.mem_map(STACK, 0x10000)
        self.uc.mem_map(OBJECT, 0x50000)
        self.uc.mem_map(STOP, 0x1000)
        self.uc.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)
        self.uc.reg_write(UC_ARM64_REG_TPIDR_EL0, TLS)
        self.write_u64(TLS + 40, 0x1234567887654321)
        self.write_u64(OBJECT, VTABLE)
        self.write_u64(VTABLE + 264, self.profile['estimator'])
        self.write_u64(VTABLE + 56, REFRESH_STUB)
        self.write_u64(OBJECT + 440, CONFIG)
        self.write_u64(OBJECT + 2584, TWS_OBJECT)
        self.write_u64(self.profile['cap_got'], self.profile['cap'])
        self.uc.hook_add(UC_HOOK_CODE, self.hook)
        self.warnings = 0
        self.phase = 0.0
        self.override_ms = None
        self.hz = 75

    def write_u64(self, address, value):
        self.uc.mem_write(address, struct.pack('<Q', value))

    def write_float(self, address, value):
        self.uc.mem_write(address, struct.pack('<f', value))

    def ret(self, value=None):
        if value is not None:
            self.uc.reg_write(UC_ARM64_REG_X0, value)
        self.uc.reg_write(UC_ARM64_REG_PC, self.uc.reg_read(UC_ARM64_REG_LR))

    def hook(self, uc, address, size, _):
        if address == REFRESH_STUB:  # virtual GetRefreshRate(), float result in s0
            uc.reg_write(UC_ARM64_REG_S0, struct.unpack('<I', struct.pack('<f',self.hz))[0])
            self.ret()
        elif address == self.profile['clock']:  # clock_gettime(CLOCK_MONOTONIC)
            dest = uc.reg_read(UC_ARM64_REG_X1)
            uc.mem_write(dest, struct.pack('<qq', *divmod(NOW_NS, 1_000_000_000)))
            self.ret(0)
        elif address == self.profile['vsync']:  # elapsed phase output of vsync query
            self.write_float(uc.reg_read(UC_ARM64_REG_X1), self.phase)
            self.write_u64(uc.reg_read(UC_ARM64_REG_X2), 0)
            self.ret(0)
        elif address == self.profile['override']:  # vrcore_enableOverridePredict_debug()
            self.ret(int(self.override_ms is not None))
        elif address == self.profile['predict_override']:  # vrcore_predictOfGetpose_debug()
            self.ret(self.override_ms)
        elif address == self.profile['log']:  # __android_log_print
            self.warnings += 1
            self.ret(0)
        elif address == self.profile['stack_fail']:
            raise RuntimeError('Stack canary failure')

    def run(self, hz, phase_fraction, tws, cap_ms=50, extra_period=True,
            config_path=0, configured_periods=None, override_ms=None, dynamic_refresh=None):
        self.phase = phase_fraction / hz
        self.hz = hz
        self.warnings = 0
        self.override_ms = override_ms
        self.write_float(OBJECT + 2636, 1 / hz)
        if cap_ms is not None:
            self.write_float(self.profile['cap'], cap_ms / 1000)
        if configured_periods is not None:
            self.write_float(self.profile['cap'] + 4, configured_periods)
        if dynamic_refresh is not None:
            self.uc.mem_write(self.profile['cap'] - 4, bytes([int(dynamic_refresh)]))
        self.uc.mem_write(OBJECT + 2397, bytes([int(extra_period)]))
        self.uc.mem_write(TWS_OBJECT + 8628, bytes([int(tws)]))
        self.uc.mem_write(CONFIG, bytes([config_path]))
        self.uc.reg_write(UC_ARM64_REG_SP, STACK + 0xFF00)
        self.uc.reg_write(UC_ARM64_REG_X0, OBJECT)
        self.uc.reg_write(UC_ARM64_REG_LR, STOP)
        self.uc.emu_start(self.profile['getter'], STOP, count=1000)
        if self.uc.reg_read(UC_ARM64_REG_PC) != STOP:
            raise RuntimeError('Instruction budget exceeded')
        value = self.uc.reg_read(UC_ARM64_REG_X0)
        return {'timestamp_ns': value, 'horizon_ms': (value-NOW_NS)/1e6 if value else None,
                'warnings': self.warnings}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    replay = Replay(args.runtime)
    cases = []
    for cap in (50, 60):
        for hz in (60, 72, 75, 90, 120):
            for tws in (False, True):
                readings = [replay.run(hz, i/1000, tws, cap) for i in range(1000)]
                valid = [r['horizon_ms'] for r in readings if r['timestamp_ns']]
                cases.append({'hz':hz, 'tws':tws, 'cap_ms':cap, 'phase_samples':1000,
                              'zero_results':sum(not r['timestamp_ns'] for r in readings),
                              'warnings':sum(r['warnings'] for r in readings),
                              'valid_min_ms':min(valid) if valid else None,
                              'valid_max_ms':max(valid) if valid else None})
    # A proposed cap adjustment must preserve all sub-50ms outcomes exactly.
    identical = 0
    restored = 0
    max_model_error_ns = 0
    for hz in (75, 90):
        for tws in (False, True):
            for phase in range(1000):
                old = replay.run(hz, phase/1000, tws, 50)
                new = replay.run(hz, phase/1000, tws, 60)
                if old['timestamp_ns']:
                    assert old == new, (hz, tws, phase, old, new)
                    identical += 1
                else:
                    assert new['timestamp_ns'] and new['warnings'] == 0
                    restored += 1
                model_ns = NOW_NS + round(((4 if tws else 3) - phase/1000)/hz*1e9)
                max_model_error_ns = max(max_model_error_ns, abs(new['timestamp_ns']-model_ns))
                assert abs(new['timestamp_ns']-model_ns) <= 20, (hz,tws,phase,new,model_ns)
    # The stock dynamic-refresh flag is1. Its getter path and the object's
    # period path must agree when both report the same display rate.
    timing_control_checks = 0
    for hz in (75,90):
        for tws in (False,True):
            for index in range(1000):
                a = replay.run(hz,index/1000,tws,60,dynamic_refresh=True)
                b = replay.run(hz,index/1000,tws,60,dynamic_refresh=False)
                assert a == b
                timing_control_checks += 1
    # Keep the sentinel/error path for out-of-range explicit overrides.
    for cap in (50, 60):
        for milliseconds in (int(cap), int(cap)+10, 100):
            r = replay.run(75, 0.5, False, cap, override_ms=milliseconds)
            assert r['timestamp_ns'] == 0 and r['warnings'] == 1
    output = {
        'kind':'offline ARM64 execution with mocked external callbacks and constructed runtime state',
        'runtime_sha256':replay.sha, 'runtime_version':replay.profile['version'],
        'getter_address':hex(replay.profile['getter']), 'estimator_address':hex(replay.profile['estimator']),
        'memory_only_candidate':'gMaxPosePredictionTime 50ms -> 60ms',
        'cases':cases,
        'checks':{'unchanged_valid_75_90_results':identical, 'restored_75_90_results':restored,
                  'max_model_rounding_error_ns':max_model_error_ns, 'asserted_model_error_bound_ns':20,
                  'dynamic_refresh_vs_stored_period_checks':timing_control_checks,
                  'out_of_range_rejection':'passed'},
        'limitations':['Constructed object flags and vsync phase are not observed live state.',
                      'Clock, vsync query, debug settings, logging, and virtual dispatch setup are stubbed.',
                      'No compositor, display, tracking, GPU, Android loader, or full runtime initialization executes.',
                      'A nonzero prediction is not proof of correct display timing or smoother streaming.',
                      '60ms is a test candidate for 75/90Hz, not a general low-refresh solution.']}
    args.output.write_text(json.dumps(output, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
