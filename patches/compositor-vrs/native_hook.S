/* Version-locked Wave 6.2.0-u09 ARM64 compositor startup policy.
 * Linked at a fixed virtual address; no new dynamic dependencies or relocations.
 * The original renderer store/load instructions execute inside each trampoline.
 */
.extern STATE
.extern GETENV_PLT
.extern CONSTRUCTOR_RETURN
.extern UNSUPPORTED_RETURN
.extern DRAW_RETURN

.section .text.vrs,"ax",%progbits
.balign 16
.global vrs_header
vrs_header:
    .ascii "SSVRVRS1"
    .word 2
    .word 256
    .quad STATE
    .quad constructor_hook
    .quad unsupported_hook
    .quad draw_hook
    .quad hooks_end
    .quad 0

/* x18 is Android's platform register: save/restore it, never use it as scratch.
 * Preserve every caller-saved GPR and NZCV. The constructor calls libc, so it
 * also preserves all 128 bits of all SIMD registers. The other hooks make no
 * calls and only save the SIMD registers included in the common frame.
 * Callee-saved
 * registers are only read.  x8's saved value is deliberately replaced by the
 * instruction result where the original instruction writes x8.
 */
.macro SAVE full=0
    .if \full
    sub sp, sp, #688
    .else
    sub sp, sp, #304
    .endif
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x30, [sp, #144]
    stp q0, q1, [sp, #160]
    stp q2, q3, [sp, #192]
    stp q4, q5, [sp, #224]
    stp q6, q7, [sp, #256]
    .if \full
    stp q8, q9, [sp, #288]
    stp q10, q11, [sp, #320]
    stp q12, q13, [sp, #352]
    stp q14, q15, [sp, #384]
    stp q16, q17, [sp, #416]
    stp q18, q19, [sp, #448]
    stp q20, q21, [sp, #480]
    stp q22, q23, [sp, #512]
    stp q24, q25, [sp, #544]
    stp q26, q27, [sp, #576]
    stp q28, q29, [sp, #608]
    stp q30, q31, [sp, #640]
    .endif
    mrs x9, nzcv
    .if \full
    str x9, [sp, #672]
    .else
    str x9, [sp, #288]
    .endif
.endm

.macro RESTORE full=0
    .if \full
    ldr x9, [sp, #672]
    .else
    ldr x9, [sp, #288]
    .endif
    msr nzcv, x9
    ldp q0, q1, [sp, #160]
    ldp q2, q3, [sp, #192]
    ldp q4, q5, [sp, #224]
    ldp q6, q7, [sp, #256]
    .if \full
    ldp q8, q9, [sp, #288]
    ldp q10, q11, [sp, #320]
    ldp q12, q13, [sp, #352]
    ldp q14, q15, [sp, #384]
    ldp q16, q17, [sp, #416]
    ldp q18, q19, [sp, #448]
    ldp q20, q21, [sp, #480]
    ldp q22, q23, [sp, #512]
    ldp q24, q25, [sp, #544]
    ldp q26, q27, [sp, #576]
    ldp q28, q29, [sp, #608]
    ldp q30, q31, [sp, #640]
    .endif
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x30, [sp, #144]
    .if \full
    add sp, sp, #688
    .else
    add sp, sp, #304
    .endif
.endm

.macro STATE_ADDRESS reg
    adrp \reg, STATE
    add \reg, \reg, :lo12:STATE
.endm

/* All diagnostic writers share this lock and sequence.  The DMB after the
 * odd sequence prevents later field stores becoming visible before it. The
 * final release-store publishes every field before the next even sequence.
 * No external call may occur between PUBLISH_BEGIN and PUBLISH_END.
 * x9 and x15 are intentionally preserved for constructor policy metadata.
 */
.macro PUBLISH_BEGIN
    add x13, x12, #72
.Lpublish_lock\@:
    ldaxr w10, [x13]
    cbnz w10, .Lpublish_busy\@
    mov w10, #1
    stlxr w11, w10, [x13]
    cbnz w11, .Lpublish_lock\@
    add x14, x12, #76
    ldr w10, [x14]
    add w10, w10, #1
    stlr w10, [x14]
    dmb ish
    b .Lpublish_acquired\@
.Lpublish_busy\@:
    clrex
    b .Lpublish_lock\@
.Lpublish_acquired\@:
.endm

.macro PUBLISH_END
    add x14, x12, #76
    ldr w10, [x14]
    add w10, w10, #1
    stlr w10, [x14]
    add x13, x12, #72
    stlr wzr, [x13]
.endm

.balign 16
.global constructor_hook
constructor_hook:
    SAVE 1
    STATE_ADDRESS x12
.Lpolicy_retry:
    ldar w8, [x12]
    tbnz w8, #31, .Lpolicy_already_sealed
    and w9, w8, #3
    cbnz w9, .Lexplicit_policy

    /* Unset only: resolve the process environment at renderer construction.
     * An API setter winning the subsequent CAS takes precedence over this read.
     */
    adr x0, environment_key
    bl GETENV_PLT
    mov w9, #0
    mov w10, #1
    mov w11, #0
    mov w14, #0
    cbz x0, .Lenvironment_ready
    adr x1, off_text
    mov x2, x0
.Lcompare_off:
    ldrb w3, [x1], #1
    ldrb w4, [x2], #1
    cmp w3, w4
    b.ne .Ltry_default
    cbnz w3, .Lcompare_off
    mov w9, #1
    b .Lenvironment_accepted
.Ltry_default:
    adr x1, default_text
    mov x2, x0
.Lcompare_default:
    ldrb w3, [x1], #1
    ldrb w4, [x2], #1
    cmp w3, w4
    b.ne .Lenvironment_invalid
    cbnz w3, .Lcompare_default
    mov w9, #2
.Lenvironment_accepted:
    mov w10, #2
    mov w11, #2
    b .Lenvironment_ready
.Lenvironment_invalid:
    mov w10, #3
    mov w14, #1
.Lenvironment_ready:
    mov w15, #0
    STATE_ADDRESS x12
    b .Lseal_compare
.Lexplicit_policy:
    mov w15, w8
    mov w10, #4
    mov w11, #1
    mov w14, #0
.Lseal_compare:
    ldaxr w8, [x12]
    cmp w8, w15
    b.ne .Lseal_changed
    orr w9, w9, #0x80000000
    stlxr w16, w9, [x12]
    cbnz w16, .Lpolicy_retry
    /* Retain the first winner's source/status until protected publication. */
    orr w15, w10, w11, lsl #8
    orr w15, w15, w14, lsl #16
    mov w8, w9
    b .Lpolicy_sealed
.Lseal_changed:
    clrex
    b .Lpolicy_retry
.Lpolicy_already_sealed:
    /* A concurrent first winner may still be publishing its metadata. */
    add x13, x12, #4
    ldar w10, [x13]
    cbz w10, .Lpolicy_retry
    mov w15, #0
.Lpolicy_sealed:
    and w9, w8, #3
    PUBLISH_BEGIN
    cbz w15, .Lmetadata_published
    ubfx w10, w15, #8, #8
    str w10, [x12, #8]
    ubfx w10, w15, #16, #8
    str w10, [x12, #12]
    and w10, w15, #0xff
    add x14, x12, #4
    stlr w10, [x14]
.Lmetadata_published:
    add x14, x12, #48
    stlr xzr, [x14]
    ldr x8, [sp, #64]
    str w8, [x12, #20]
    cmp w9, #1
    csel w8, wzr, w8, eq
    str w8, [x12, #24]
    str x8, [sp, #64]
    strb w8, [x19, #3104]  /* Original constructor instruction. */
    /* Opaque identities are compared by the getter, never dereferenced. */
    add x14, x12, #56
    stlr xzr, [x14]
    add x14, x12, #64
    stlr xzr, [x14]
    add x13, x12, #16
.Lconstructor_count:
    ldaxr w9, [x13]
    add w9, w9, #1
    stlxr w10, w9, [x13]
    cbnz w10, .Lconstructor_count
    add x14, x12, #48
    stlr x19, [x14]
    PUBLISH_END
    RESTORE 1
    b CONSTRUCTOR_RETURN

.balign 16
.global unsupported_hook
unsupported_hook:
    SAVE
    STATE_ADDRESS x12
    PUBLISH_BEGIN
    strb wzr, [x19, #3104] /* Original unsupported-extension instruction. */
    str wzr, [x12, #24]
    add x14, x12, #64
    stlr x19, [x14]
    add x13, x12, #28
.Lunsupported_count:
    ldaxr w9, [x13]
    add w9, w9, #1
    stlxr w10, w9, [x13]
    cbnz w10, .Lunsupported_count
    PUBLISH_END
    RESTORE
    b UNSUPPORTED_RETURN

.balign 16
.global draw_hook
draw_hook:
    SAVE
    STATE_ADDRESS x12
    PUBLISH_BEGIN
    ldrb w8, [x19, #3104] /* Original draw-gate instruction. */
    str x8, [sp, #64]
    str w8, [x12, #40]
    add x14, x12, #56
    stlr x19, [x14]
    add x13, x12, #32
.Ldraw_count:
    ldaxr x9, [x13]
    add x9, x9, #1
    stlxr w10, x9, [x13]
    cbnz w10, .Ldraw_count
    PUBLISH_END
    RESTORE
    b DRAW_RETURN

.balign 8
environment_key:
    .asciz "SSVR_WAVE_COMPOSITOR_VRS_V1"
off_text:
    .asciz "Off"
default_text:
    .asciz "DeviceDefault"
.balign 16
.global hooks_end
hooks_end:
