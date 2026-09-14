# Build Plan

Phased roadmap. Each phase ends at a state you can boot, test, and commit. Nothing lands without passing hvprobe and a day of idle soak.

## Phase 1 - Core virtualization (done)

- [x] Per-core VMXON/VMLAUNCH through DPC broadcast (KeGenericCallDpc)
- [x] VMCS bring-up: full host and guest state capture, pin controls zeroed, exception bitmap empty
- [x] Identity EPT over the full guest physical space (RAM as WB, holes as UC, 1GB/2MB pages)
- [x] VPID per vCPU
- [x] MSR bitmap passthrough
- [x] CPUID cache and replay (A-series vectors)
- [x] #UD on all VMX-instruction exits and unauthenticated VMCALL
- [x] CR4.VMXE read shadow
- [x] Trap-next-RDTSC TSC compensation with pre-VMXON calibration
- [x] Clean teardown: VMCALL-triggered exit handler does VMXOFF and returns to guest context in place
- [x] hvprobe usermode checks: hypervisor bit, invalid leaf, CPUID timing ratio, TSC monotonicity, VMX instruction fault behavior

## Phase 2 - Exit-path hardening

- [ ] Exit-path micro-optimization: branchless dispatch, per-vcpu cached VMCS fields, target 1.6-2.0x native CPUID cost on the B5 baseline
- [ ] CR4 VMXE write-then-read closure: emulate the write into the shadow for the writer's view only
- [ ] RDMSR(IA32_TSC) compensation parity with RDTSC (E2)
- [ ] Hypercall interface: versioned register-signature gate, command set for phase 3 control (hook install/revoke, query)
- [ ] Host stack canary and exit-path assertions (silent fail -> controlled unload, never a hang)

## Phase 3 - EPT dual view (hooks)

- [ ] GPA-alias trapless dual-view: target VA -> G0 -> fake HPA (RWX), reader VA -> G1 -> original HPA (RO)
- [ ] INVEPT discipline: per-EPTP single-context invalidation on every alias install/teardown, cross-core IPI
- [ ] No contiguous allocations and no tagged pool structs for hook state (H2 requirement)
- [ ] Write-revoke: any write into a hooked page tears the hook down before the write lands (H3)
- [ ] Self-hiding: read-strip on the HV's own pages, read violation handler spoofs the read (751605 pattern)
- [ ] NMI context fixup: with hooks live, G1's trade flips - NMI-exiting on, RIP fixup of fake-page frames
- [ ] HPET MMIO EPT split + counter virtualization (B6)
- [ ] hvprobe additions: EPT violation count during soak, CLFLUSH cache-hit asymmetry test

## Phase 4 - Coexistence and closure

- [ ] Hyper-V present: either nested-VMCS forwarding (KVM model) or the enlightened-VMCS hook path; decide by what Hyper-V does with VTL1 on target builds
- [ ] Cross-core wall-clock (B3): exit-path cost floor plus CPUID result caching so repeat identical leaves in a storm skip the full handler
- [ ] Optional IA32_TSX_CTRL disable as a build flag, with CPUID leaf 7 cache updated to match (H5)
- [ ] Private host IDT + private host CR3 (Ophion hardening) once hooks make root context worth protecting
- [ ] Descriptor-table self-check in hvprobe (D3)

## Phase 5 - Loader / research forks

- [ ] EFI boot-stage variant research spike (rejected for mainline because of I1 - PCR banks)
- [ ] AMD SVM port evaluation - CPUID interception is clearable there, which halves the B-series work
- [ ] Documentation pass: bring-up notes, exit cost measurements per uarch, VMAware results table

## Testing loop

The iteration environment is a VMware Workstation VM (Windows 11 x64, vhv.enable = TRUE, 2 vCPU, 4GB). Every phase change gets: load, soak, hvprobe, snapshot. Snapshots are named `phase<N>-<state>`; the VM never advances past a failing probe without a revert.
