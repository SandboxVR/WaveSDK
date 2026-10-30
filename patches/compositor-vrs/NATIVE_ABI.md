The ARM64 runtime patch applies only to cap60 SHA-256
`7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a`.
All addresses below are ELF virtual addresses; add the runtime's load bias.

The new RX mapping begins at `0xa51000`. Its immutable header contains eight
ASCII bytes `SSVRVRS1`, uint32 ABI version `2` at `+8`, uint32 state size `256`
at `+12`, and uint64 state virtual address `0xa4fa00` at `+16`. Header uint64
values at `+24`, `+32`, `+40`, and `+48` contain constructor, unsupported-extension,
draw, and end virtual addresses for offline validation. They are virtual
addresses, not relocated process pointers. Validate the magic, version, size
and address before reading or writing state.
ABI 2 adds the shared publication sequence. Reject earlier development ABI 1
modules, whose permanently zero reserved sequence would not protect snapshots.

The state occupies extended zero-initialized BSS in the final RW mapping.
Naturally aligned fields are accessed atomically by the private JNI companion.
The first constructor seals policy with LDAXR/STLXR; JNI setters must use an
acquire/release compare-and-swap and reject a word with bit 31 set.

| Offset | Type | Meaning |
| --- | --- | --- |
| `0x00` | atomic uint32 | Policy: low mode `0` unset/default, `1` Off, `2` explicit DeviceDefault; bit 31 sealed. |
| `0x04` | atomic uint32 | Environment result: `0` unchecked, `1` absent, `2` accepted, `3` invalid, `4` bypassed by explicit policy. |
| `0x08` | atomic uint32 | Policy source: `0` implicit default, `1` explicit private API, `2` environment. |
| `0x0c` | atomic uint32 | Error bits: bit 0 means invalid environment value. |
| `0x10` | atomic uint32 | Aggregate constructor count, wrapping at 2^32. |
| `0x14` | atomic uint32 | Original parsed ShadingRateUtils validity byte in the latest constructor. |
| `0x18` | atomic uint32 | Actual cached enabled byte written by the latest constructor; also cleared by the unsupported-extension hook. |
| `0x1c` | atomic uint32 | Aggregate unsupported-extension fallback count, wrapping at 2^32. |
| `0x20` | atomic uint64 | Aggregate draw-gate observation count, wrapping at 2^64. |
| `0x28` | atomic uint32 | Actual enabled byte read at the last observed draw gate. |
| `0x30` | atomic uint64 | Latest constructor renderer identity, opaque; zero during constructor diagnostic publication. |
| `0x38` | atomic uint64 | Last draw renderer identity, opaque; cleared on each constructor publication. |
| `0x40` | atomic uint64 | Last unsupported-extension renderer identity, opaque; cleared on each constructor publication. |
| `0x48` | atomic uint32 | Internal shared diagnostic writer lock. Companion must not modify it. |
| `0x4c` | atomic uint32 | Publication sequence: odd while a diagnostic writer is active, even after publication; wraps modulo 2^32. |
| remainder | reserved | Leave zero. |

Only when policy is unset does the first renderer constructor read
`SSVR_WAVE_COMPOSITOR_VRS_V1`. Exact strings `Off` and `DeviceDefault` are
accepted. An absent value preserves native behavior and seals mode 0. An invalid
value sets the error bit, preserves native behavior and seals mode 0. An
explicit API policy winning a race with the environment read takes precedence.
Once sealed, later renderer constructions reuse policy without reading the
environment. No library-load constructor or frame loop reads the environment.
The first sealing constructor publishes environment result/source/error under
the diagnostic writer protocol. Another constructor that sees sealed policy
with environment result zero waits for that metadata instead of publishing an
apparently complete renderer with an unchecked policy source.

The constructor hook runs at `0x5bdb50`, after native config parsing and before
the renderer enabled byte is written. Off forces that byte to zero;
DeviceDefault preserves the original parsed byte. The following original CBZ
therefore also skips the native enable log for Off. Unmodified gates at
`0x5c23cc` and `0x5c6ff0` use this cached byte for VRS GPU resources and draw
selection. The native unsupported-extension zero store at `0x5c2694` remains
effective. Resume and recreation use the same sealed policy.

All constructor, draw and unsupported-extension diagnostic publication is
serialized by the same short writer lock. Each writer publishes an odd sequence,
executes a DMB ISH before changing fields, updates observations and tokens, then
release-publishes the next even sequence and unlocks. No external call occurs
while the lock is held. Constructor publication invalidates the latest identity
to zero, writes policy metadata when first sealing, writes input/effective fields
and the actual renderer cache, clears both observation identities, increments
constructor count, and release-publishes the new identity. Draw reads the actual
renderer cache and publishes the observed byte, identity and count while the
sequence is odd. Unsupported fallback likewise writes the actual native cache
to zero and publishes its observations while odd. Counters are aggregate and
are never reset.

A reader must acquire-read sequence before reading any observations, reject an
odd value, read the atomic fields and identities, issue an acquire fence, and
acquire-read sequence again. Accept observations only when both sequence values
are identical and even. Check policy before/after too: JNI setters and the
constructor's seal CAS necessarily operate outside the observation sequence.
Report pending if sequence/policy is unstable or latest identity is zero, and
attribute a draw/unsupported observation to the latest renderer only when its
corresponding nonzero identity matches. Double-reading identities alone cannot
prevent an old renderer's new flag appearing alongside a previous renderer
token during publication. Never dereference or expose these opaque identities.
A raw `lastEffective == 1` describes startup eligibility; even a draw-gate
observation does not prove an actual GPU shading-rate call.

The constructor preserves every GPR, NZCV and all 128 bits of q0-q31 across the
libc getenv call, except x8 intentionally returns the filtered original byte.
The other two hooks make no calls, preserve their scratch registers and NZCV,
and replay the replaced instruction. No RWX mapping, runtime relocation or
additional library dependency is introduced. Three original instructions,
one RW BSS-size header field and one repurposed PT_NOTE program-header entry
are the only original file ranges changed. The original note contents and
section table remain byte-identical. The builder records exact reversible
ranges and rejects any unknown baseline or opcode.

Offline verification uses the actual assembled AArch64 instructions with
Unicorn 2.1.4. Run `test_native_hook.py --runtime <unpatched-cap60.so> --ndk <ndk>`.
It checks register preservation, startup modes, invalid values, an API/environment
race, recreation, identity invalidation, draw/fallback observations, original
resource/draw branch selection, ELF mappings, byte preservation and reproducible
output. Instruction-level adversarial snapshots cover an old renderer writing
its draw flag before its identity, shared-lock contention for every writer,
odd/even sequence wrap and first-policy metadata publication. Emulation checks
CPU effects and instruction ordering; it does not simulate ARM weak memory
visibility, Android loader acceptance or GPU execution;
those require device validation of the packaged runtime.
