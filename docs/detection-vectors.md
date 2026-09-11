# Detection Vector Catalog

Research notes on how anti-cheat vendors detect Intel VT-x hypervisors, and how this base answers each one. Compiled from public anti-cheat research communities (UnknownCheats anti-cheat research forum, ~150 hypervisor-related threads), anti-cheat devirtualization dumps, and published detection tooling (hvdetecc, VMAware, checkhv).

Attribution is per-thread evidence, not vendor documentation. "Unattributed" means the technique exists in public detection tooling but no vendor has been caught shipping it yet.

## A. CPUID

### A1. Leaf 1 ECX bit 31 - hypervisor present bit
The classic. Read CPUID.01H, check ECX[31].
Used by: VGK/vgc, EA Javelin, most detection suites.
Answer: CPUID is intercepted and answered from a cache snapshot taken before VMXON. On bare metal the bit is 0, so the guest reads 0. No bit is ever cleared at runtime - the answer is replayed, never filtered, which keeps every other leaf consistent with it.

### A2. Leaf 0x40000000+ vendor signatures
The guest reads the hypervisor vendor string at the 0x40000000 range and matches it against known signatures (Hyper-V, VMware, KVM, Xen, DBVM...).
Used by: EA Javelin (CPUIDBackdoor), EAC.
Answer: pre-VMXON cache again. On a machine with no other hypervisor, bare metal returns no vendor string, and we return none either. Some vendors *disable* their VM checks when they see a benign vendor like Hyper-V - masquerading is a valid strategy but it contradicts A1 on real hardware, so this base plays the honest machine instead.

### A3. Invalid leaf behavior
Bare metal CPUs echo the max-leaf result for out-of-range leaves. Most hypervisors return zeros or garbage, and the per-leaf variation is a signature.
Used by: EA Javelin (CPUIDBrand), detection suites.
Answer: the cache covers the entire 0x40000000..0x4FFFFFFF range, the gap between max standard leaf and 0x80000000, and everything past max extended leaf. Every invalid leaf replays the same bytes bare metal produced.

### A4. Cross-leaf consistency
Contradictions between leaves: AVX2 without XSAVE, SMEP in leaf 7 but not in CR4, hypervisor bit cleared but 0x40000000 claiming a hypervisor, malformed Hyper-V signature responses.
Used by: EA Javelin (five AntiHV CPUID categories), PhantomHvDetectionLab.
Answer: single snapshot, replayed verbatim. Consistency falls out of the design instead of being patched in.

### A5. Instruction-emulation diffs
AC executes CPUID leaf 0x40000001, hypervisor intercepts and emulates, diff from bare-metal echo.
Used by: EA Javelin (Emulation).
Answer: same as A3.

## B. Timing

### B1. CPUID vs FYL2XP1 profiling
Time CPUID (a VM-exit under us) against a non-exiting x87 instruction, using QPC or RDTSC. Native CPUID costs ~80-200 cycles; a naive hypervisor exit costs 1500+.
Used by: Valorant-era detection suites, EAC (devirtualized shellcode shows 100-iteration CPUID/RDTSC benchmark loops).
Answer: the trap-next-RDTSC model. TSC exiting is disarmed by default. When a CPUID exit happens, the handler records the exit TSC and arms RDTSC exiting for the next RDTSC only. That read returns `exit_tsc + calibrated_native_cpuid_cost`, then disarms. TSC_OFFSET is never touched. Intervening exits (interrupts, NMIs) disarm the trap rather than corrupt it. The calibration runs pre-VMXON: minimum of 200 fenced samples of the real CPUID cost on the same machine.

### B2. Double-read TSC jump
`CPUID -> delay -> RDTSC -> RDTSC` - if only the first read is compensated and the second jumps back to real time, the gap between reads is the tell.
Used by: PhantomHvDetectionLab.
Answer: only the single RDTSC following a CPUID exit is synthesized, and it is synthesized forward by exactly the native CPUID cost - so the second, uncompensated read is *behind* the first by a small amount, same as bare metal where the second read is later than the first. The pair never goes backwards.

### B3. Cross-core wall-clock observation
A worker core holds an atomic flag across a CPUID; a leader core on another CPU samples the flag with real wall-clock time. Per-core TSC faking cannot fix this because the observation is on another core.
Used by: EAC (lock inc/cpuid/lock dec pattern in devirtualized Rust driver - observed in dumps, not confirmed live).
Answer: partially open. The only real fix is making the exit path itself cheap enough that the flag window approaches native. This base keeps the exit surface minimal (see C-series) which shrinks the window, but the leader-core check is the hardest open problem on Intel. Tracked in PLAN.md phase 4.

### B4. Racing threads
One thread forces VM-exits, another counts TSC. Same family as B3.
Used by: modern EAC, per community reporting.
Answer: same as B3. Exit frequency is the lever - this base exits on CPUID only.

### B5. VM-exit instruction cost baselines
Detection libraries calibrate Hyper-V exit costs and compare statistically.
Used by: VMAware ("timing anomalies" check; AMD passes easily, Intel is the hard case).
Answer: B1 mitigation plus a small exit surface. Community floor on Intel is 1.6-2.0x native on the exit-causing instruction; this base targets that range by keeping the CPUID path branch-minimal.

### B6. HPET / LAPIC-timer MMIO wall clock
RDTSC can be compensated; HPET and the local APIC timer counters are MMIO reads this base does not touch, so a detector comparing HPET progression against TSC progression during a CPUID sees the discrepancy.
Unattributed (open research vector, flagged by the Ophion author).
Answer: open. Would require an EPT split of the HPET MMIO page, which is phase 3 material. Tracked in PLAN.md.

## C. Exit surface and TLB

### C1. VM-exit storms / TLB reset
EAC floods exit-causing instructions; without VPID every exit flushes guest TLB entries, the storm causes measurable TLB/cache reset and can hard-freeze the system.
Used by: EAC.
Answer: VPID enabled with a unique ID per vCPU, so exits never flush guest TLB state. Secondary proc controls set accordingly.

### C2. Exit-count-induced scheduler starvation
Frequent VMMCALLs (per-read memory access) starve DPCs and stutter the machine.
Used by: not a detection - a self-inflicted stability failure that looks like one.
Answer: the base exits on CPUID and the transient RDTSC only. The memory-access API is a hypercall interface meant for low-frequency control, not per-read traffic. Phase 2 replaces hot reads with EPT remapping instead of exits.

### C3. Interrupt-window exit flood
If INTERRUPT_WINDOW_EXITING is left set, VMRESUME re-exits instantly, looping all cores into a silent hang.
Self-inflicted.
Answer: pin-based controls are zero. No interrupt window exiting, no external-interrupt exiting, no NMI exiting, no preemption timer. Interrupts and NMIs deliver to the guest directly, adding zero latency - which also kills the timing side channel they would otherwise create.

## D. Control registers and descriptors

### D1. CR4.VMXE visibility
Guest reads CR4, bit 13 set = VMX active.
Used by: everything.
Answer: CR4 guest/host mask covers VMXE only; the read shadow returns CR4 with the bit clear. Real CR4 in the VMCS keeps it set so the processor stays in VMX operation.

### D2. CR write probes
EAC writes invalid CR values at startup expecting #GP; a hypervisor that mishandles the CR-access exit spins the core into a deadloop and CLOCK_WATCHDOG_TIMEOUT.
Used by: EAC.
Answer: CR0/CR3 masks are zero (no exits on normal traffic). CR4 exits only on VMXE writes; the handler merges the write into the shadow, never writes real CR4, advances RIP, and never injects an unexpected exception. The probe gets the same #GP-free behavior bare metal gives for legal writes and a shadow-consistent read back.

### D3. SGDT/SIDT/SLDT/STR desync
A relocated or desynced GDT/IDT (host copy differing from guest copy) shows in descriptor reads.
Used by: EAC (idt/gdt checks), Denuvo.
Answer: the base never relocates guest descriptors and does not use descriptor-table exiting. Host IDT is read at bring-up and stored in the VMCS but the guest's view is untouched, so there is nothing to desync. (Isolation of the VMX-root IDT - the Ophion hardening - is phase 4 material.)

### D4. VMX instruction fault behavior
Guest executes VMREAD/VMLAUNCH/etc. In non-root these exit; on bare metal (VMXE=0) they #UD. The exit handler that does not inject #UD is caught.
Used by: EAC ("vmx instruction" and "vmread" search strings), detection suites.
Answer: all ten VMX-instruction exit reasons are handled identically: inject #UD, no RIP advance (fault semantics). Identical to the bare metal answer for CPL0 and CPL3.

### D5. VMCALL from ring 3
Same family: ring-3 VMCALL should #UD/#GP; an unimplemented hypercall path that crashes or behaves differently is a tell.
Used by: EAC, EA Javelin.
Answer: VMCALL exits are answered with #UD unless the register signature matches our hypercall gate from CPL0. Mismatched signature = #UD, byte-for-byte bare metal.

## E. MSRs

### E1. MSR read probes
Reads of VMX capability MSRs (0x480-0x493), FEATURE_CONTROL (0x3A), EFER, PAT, DEBUGCTL, TSC_AUX; fault behavior on unsupported MSRs.
Used by: EAC (readmsr/vmread/cpuid/rdtsc search string set), BE.
Answer: full MSR bitmaps, all zeros - every RDMSR/WRMSR passes through to real hardware. Values are real, faults are real, timing is native. FEATURE_CONTROL is left untouched: bit states (lock + VMX-outside-SMX) are identical to a machine with VMX enabled at boot, which is the majority of real machines.

### E2. RDMSR(0x10) as a TSC read
RDMSR of IA32_TSC goes through the same TSC path and must be covered by the compensation.
Used by: detection suites.
Answer: MSR bitmap intercepts IA32_TSC reads only when the RDTSC trap is armed, applies the same compensation as RDTSC, then disarms. (The bitmap bit is toggled with the same lifecycle as RDTSC exiting.)

## F. Exceptions and debug

### F1. DR6 semantics after MOV SS + CPUID
The DBVM killer: data breakpoint + TF, MOV SS blocks the #DB and latches the B0 condition, CPUID forces the exit; a handler that recomputes pending debug exceptions wrong returns DR6 with BS instead of BS|B0.
Used by: Hyperion (Roblox), VMAware.
Answer: the exception bitmap is empty, so guest #DB never exits to us - the hardware delivers it with native semantics. We do not intercept debug exceptions and never reconstruct DR6, so the B0 latch survives by construction.

### F2. MOV-DR exiting and DR isolation
Some bases exit on DR access and software-manage DR0-3/DR6, which breaks the advanced-RIP pending exception recompute (hardware will not recheck breakpoints after re-entry).
Used by: not directly - the failure mode is self-inflicted and detectable by F1-style probes.
Answer: MOV-DR exiting is off. Debug registers are guest-owned, zero exits, zero semantics to break.

## G. NMI / interrupts

### G1. NMI callback stack walking
EAC registers KeRegisterNmiCallback handlers that snapshot the interrupted RIP, stack, CR3 and walk frames; a hypervisor that stalls NMI delivery or shows host RIPs in the frame is caught.
Used by: EAC (NmiCallbackRoutine, reversed), BE also fires NMIs.
Answer: NMI exiting is off - NMIs deliver to the guest in-guest with zero added latency, and the frames the callback walks are genuine guest frames. The cost: we cannot inspect or fix up NMI context (needed once EPT hooks exist and fake pages could appear in a frame). That trade is deliberate for the base - latency-less correctness first, fixups in phase 3 when there is something to hide.

### G2. Interrupt latency
Any exit-path latency on clock interrupts shows up as scheduling jitter and CLOCK_WATCHDOG_TIMEOUT under load.
Self-inflicted; also measurable by ACs.
Answer: pin-based controls zero. Interrupts never exit. The only exits are CPUID and the armed RDTSC.

## H. Memory / EPT

### H1. EPT hook timing
Hooked-page execution latency, violation counts, MTF exit frequency - all measurable.
Used by: EAC, EA Javelin (20ms RDTSCP-guarded scans).
Answer: the base ships with a pure identity map, all pages RWX, no violations, no MTF. Nothing to detect. Dual-view hooks (phase 3) will use the GPA-aliasing trapless design specifically because of this vector.

### H2. Fake-page PFN correlation
Private pages that are ~99% identical to image-backed pages, orphan PFNs from contiguous allocations, pool scans for hook-state structs with plaintext PFN pointers.
Used by: EAC/BE (A/B evidence: hooks on the protected module = fast ban, arbitrary other page = none).
Answer: no fake pages exist in the base. Phase 3's design constraints (no contiguous allocations for fakes, no pool-tagged state structs, read-strip self-hiding) are recorded in PLAN.md as build requirements, not afterthoughts.

### H3. Write-then-execute integrity probe
Write a byte to a hooked page and execute it; a split-view hook that does not reflect writes into the execute view runs stale bytes.
Used by: EAC-class.
Answer: n/a in the base (no hooks). Phase 3 requirement: any write to a hooked page revokes the hook instantly.

### H4. EPT range over 512GB
On systems where MMIO sits above 512GB (Core Ultra), an EPT that only covers the RAM ranges below it hangs or triple-faults on GPU perf-state queries (misattributed to the ARL029 erratum).
Self-inflicted.
Answer: the EPT builder maps the entire guest physical address space from zero to MAXPHYADDR, not just the RAM ranges - MMIO holes included (as UC). Covered in ept.c.

### H5. TSX transaction abort on exit
RTM transaction + forced VM-exit aborts the transaction; bare metal survives it. The AC checks the handshake.
Used by: EAC.
Answer: this base disables nothing, but does not intercept any TSX instruction - the only forced exits (CPUID) are the same ones the check has to use. Full closure via IA32_TSX_CTRL RTM_DISABLE is listed as a config option in PLAN.md phase 4 because it changes CPUID leaf 7 output and must be cached consistently.

## I. Boot / platform

### I1. TPM measured boot / PCR banks
With Secure Boot + TPM, a .efi-stage hypervisor changes the measurement chain and the PCR banks say so.
Used by: VGK.
Answer: out of scope for the driver-stage base; a boot-stage variant is a phase 5 item and would need SMM or measurement-chain work. Documented because "just make it an EFI payload" is the reflex answer and it is wrong under TPM.

## J. Known-weak spots in this base (honest list)

- B3/B4 cross-core wall-clock: mitigated only by exit-path cost, not defeated.
- B6 HPET/APIC MMIO wall clock: unmitigated until phase 3.
- CR4 write-then-read of VMXE: a write of VMXE=1 reads back 0 (shadow). Bare metal would read back 1. This is a deliberate trade (real VMXE must stay set); closing it needs write emulation that shadows the set bit for the writer only - noted in PLAN.md phase 2.
- Hyper-V/HVCI coexistence: not supported. If Hyper-V is active, VMXON fails cleanly. Nested handling is phase 4.
- NMI fixups: none, by design, until hooks exist (G1 trade).

## Sources

Primary: UnknownCheats anti-cheat research forum, threads on hypervisor detection, EAC/BE/VGK/Javelin devirtualizations, EPT hook detection research, and the public detection tools (hvdetecc, VMAware, PhantomHvDetectionLab). Specific threads are not linked per file; the catalog above is cross-referenced against ~150 threads collected Nov 2025 - the Ophion writeup, the DBVM detection series, the EA Javelin AntiHV analysis, and the EAC devirtualized driver dumps were the largest single contributors.
