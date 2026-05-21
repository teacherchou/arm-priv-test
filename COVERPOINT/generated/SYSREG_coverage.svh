// Generated from arm-priv-test/NORM/SYSREG.yaml
// This is an EDA-facing functional coverage skeleton.
// Replace numeric sample IDs with DUT/trace adapter fields when integrating RTL.
`ifndef _SYSREG_COVERAGE_SVH_
`define _SYSREG_COVERAGE_SVH_

// Rule ID mapping
localparam int unsigned SYSREG_RULE_001 = 1; // SYSREG-RULE-001
localparam int unsigned SYSREG_RULE_002 = 2; // SYSREG-RULE-002
localparam int unsigned SYSREG_RULE_003 = 3; // SYSREG-RULE-003

// CoverPoint ID mapping
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP = 1; // cp_sysreg_el0_el1_read_trap
localparam int unsigned CP_SYSREG_EL0_EL1_WRITE_TRAP = 2; // cp_sysreg_el0_el1_write_trap
localparam int unsigned CP_SYSREG_EL0_MAINTENANCE_TRAP = 3; // cp_sysreg_el0_maintenance_trap
localparam int unsigned CP_SYSREG_EL0_ERET_TRAP = 4; // cp_sysreg_el0_eret_trap
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP = 5; // cp_sysreg_el1_read_no_trap
localparam int unsigned CP_SYSREG_EL1_RW_NO_TRAP = 6; // cp_sysreg_el1_rw_no_trap
localparam int unsigned CP_SYSREG_EL1_MAINTENANCE_NO_TRAP = 7; // cp_sysreg_el1_maintenance_no_trap

// Bin ID mapping
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__SCTLR_EL1_READ = 1; // cp_sysreg_el0_el1_read_trap__sctlr_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__VBAR_EL1_READ = 2; // cp_sysreg_el0_el1_read_trap__vbar_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__TTBR0_EL1_READ = 3; // cp_sysreg_el0_el1_read_trap__ttbr0_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__TCR_EL1_READ = 4; // cp_sysreg_el0_el1_read_trap__tcr_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__ESR_EL1_READ = 5; // cp_sysreg_el0_el1_read_trap__esr_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__FAR_EL1_READ = 6; // cp_sysreg_el0_el1_read_trap__far_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_READ_TRAP__MAIR_EL1_READ = 7; // cp_sysreg_el0_el1_read_trap__mair_el1_read
localparam int unsigned CP_SYSREG_EL0_EL1_WRITE_TRAP__SCTLR_EL1_WRITE = 8; // cp_sysreg_el0_el1_write_trap__sctlr_el1_write
localparam int unsigned CP_SYSREG_EL0_EL1_WRITE_TRAP__VBAR_EL1_WRITE = 9; // cp_sysreg_el0_el1_write_trap__vbar_el1_write
localparam int unsigned CP_SYSREG_EL0_MAINTENANCE_TRAP__TLBI_VMALLE1 = 10; // cp_sysreg_el0_maintenance_trap__tlbi_vmalle1
localparam int unsigned CP_SYSREG_EL0_MAINTENANCE_TRAP__IC_IALLU = 11; // cp_sysreg_el0_maintenance_trap__ic_iallu
localparam int unsigned CP_SYSREG_EL0_ERET_TRAP__ERET_EL0_TRAP = 12; // cp_sysreg_el0_eret_trap__eret_el0_trap
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP__SCTLR_EL1_READ = 13; // cp_sysreg_el1_read_no_trap__sctlr_el1_read
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP__TTBR0_EL1_READ = 14; // cp_sysreg_el1_read_no_trap__ttbr0_el1_read
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP__TCR_EL1_READ = 15; // cp_sysreg_el1_read_no_trap__tcr_el1_read
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP__MAIR_EL1_READ = 16; // cp_sysreg_el1_read_no_trap__mair_el1_read
localparam int unsigned CP_SYSREG_EL1_READ_NO_TRAP__DAIF_READ = 17; // cp_sysreg_el1_read_no_trap__daif_read
localparam int unsigned CP_SYSREG_EL1_RW_NO_TRAP__VBAR_EL1_ROUNDTRIP = 18; // cp_sysreg_el1_rw_no_trap__vbar_el1_roundtrip
localparam int unsigned CP_SYSREG_EL1_RW_NO_TRAP__TPIDR_EL0_ROUNDTRIP = 19; // cp_sysreg_el1_rw_no_trap__tpidr_el0_roundtrip
localparam int unsigned CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__TLBI_VMALLE1 = 20; // cp_sysreg_el1_maintenance_no_trap__tlbi_vmalle1
localparam int unsigned CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__IC_IALLU = 21; // cp_sysreg_el1_maintenance_no_trap__ic_iallu

covergroup SYSREG_cg with function sample(int unsigned rule_id, int unsigned coverpoint_id, int unsigned bin_id);
  option.per_instance = 1;

  cp_rule: coverpoint rule_id {
    bins SYSREG_RULE_001_BIN = {SYSREG_RULE_001};
    bins SYSREG_RULE_002_BIN = {SYSREG_RULE_002};
    bins SYSREG_RULE_003_BIN = {SYSREG_RULE_003};
  }

  cp_coverpoint: coverpoint coverpoint_id {
    bins CP_SYSREG_EL0_EL1_READ_TRAP_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP};
    bins CP_SYSREG_EL0_EL1_WRITE_TRAP_BIN = {CP_SYSREG_EL0_EL1_WRITE_TRAP};
    bins CP_SYSREG_EL0_MAINTENANCE_TRAP_BIN = {CP_SYSREG_EL0_MAINTENANCE_TRAP};
    bins CP_SYSREG_EL0_ERET_TRAP_BIN = {CP_SYSREG_EL0_ERET_TRAP};
    bins CP_SYSREG_EL1_READ_NO_TRAP_BIN = {CP_SYSREG_EL1_READ_NO_TRAP};
    bins CP_SYSREG_EL1_RW_NO_TRAP_BIN = {CP_SYSREG_EL1_RW_NO_TRAP};
    bins CP_SYSREG_EL1_MAINTENANCE_NO_TRAP_BIN = {CP_SYSREG_EL1_MAINTENANCE_NO_TRAP};
  }

  cp_bin: coverpoint bin_id {
    bins CP_SYSREG_EL0_EL1_READ_TRAP__SCTLR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__SCTLR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__VBAR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__VBAR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__TTBR0_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__TTBR0_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__TCR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__TCR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__ESR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__ESR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__FAR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__FAR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_READ_TRAP__MAIR_EL1_READ_BIN = {CP_SYSREG_EL0_EL1_READ_TRAP__MAIR_EL1_READ};
    bins CP_SYSREG_EL0_EL1_WRITE_TRAP__SCTLR_EL1_WRITE_BIN = {CP_SYSREG_EL0_EL1_WRITE_TRAP__SCTLR_EL1_WRITE};
    bins CP_SYSREG_EL0_EL1_WRITE_TRAP__VBAR_EL1_WRITE_BIN = {CP_SYSREG_EL0_EL1_WRITE_TRAP__VBAR_EL1_WRITE};
    bins CP_SYSREG_EL0_MAINTENANCE_TRAP__TLBI_VMALLE1_BIN = {CP_SYSREG_EL0_MAINTENANCE_TRAP__TLBI_VMALLE1};
    bins CP_SYSREG_EL0_MAINTENANCE_TRAP__IC_IALLU_BIN = {CP_SYSREG_EL0_MAINTENANCE_TRAP__IC_IALLU};
    bins CP_SYSREG_EL0_ERET_TRAP__ERET_EL0_TRAP_BIN = {CP_SYSREG_EL0_ERET_TRAP__ERET_EL0_TRAP};
    bins CP_SYSREG_EL1_READ_NO_TRAP__SCTLR_EL1_READ_BIN = {CP_SYSREG_EL1_READ_NO_TRAP__SCTLR_EL1_READ};
    bins CP_SYSREG_EL1_READ_NO_TRAP__TTBR0_EL1_READ_BIN = {CP_SYSREG_EL1_READ_NO_TRAP__TTBR0_EL1_READ};
    bins CP_SYSREG_EL1_READ_NO_TRAP__TCR_EL1_READ_BIN = {CP_SYSREG_EL1_READ_NO_TRAP__TCR_EL1_READ};
    bins CP_SYSREG_EL1_READ_NO_TRAP__MAIR_EL1_READ_BIN = {CP_SYSREG_EL1_READ_NO_TRAP__MAIR_EL1_READ};
    bins CP_SYSREG_EL1_READ_NO_TRAP__DAIF_READ_BIN = {CP_SYSREG_EL1_READ_NO_TRAP__DAIF_READ};
    bins CP_SYSREG_EL1_RW_NO_TRAP__VBAR_EL1_ROUNDTRIP_BIN = {CP_SYSREG_EL1_RW_NO_TRAP__VBAR_EL1_ROUNDTRIP};
    bins CP_SYSREG_EL1_RW_NO_TRAP__TPIDR_EL0_ROUNDTRIP_BIN = {CP_SYSREG_EL1_RW_NO_TRAP__TPIDR_EL0_ROUNDTRIP};
    bins CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__TLBI_VMALLE1_BIN = {CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__TLBI_VMALLE1};
    bins CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__IC_IALLU_BIN = {CP_SYSREG_EL1_MAINTENANCE_NO_TRAP__IC_IALLU};
  }

  cross_rule_coverpoint: cross cp_rule, cp_coverpoint;
  cross_coverpoint_bin: cross cp_coverpoint, cp_bin;
endgroup

`endif // _SYSREG_COVERAGE_SVH_
