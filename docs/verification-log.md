# Verification Log

Findings from sensor validation and system verification.

---

## VER-001: Memory sensor reporting ~99.5% utilization on idle system

**Date:** 2026-02-13
**Sensor:** `MemorySensor`
**Severity:** High — false DEGRADED state on startup

**Observation:**
Memory sensor reported `0.995` (99.5% utilization) on a system that macOS `memory_pressure` reported as 35% free.

**Root Cause:**
The sensor calculated used memory as `total - free_count`. On macOS, `free_count` from `vm_statistics64` only reflects pages that are completely unused. macOS aggressively fills memory with file caches, compressed pages, and inactive pages — all of which are reclaimable but were being counted as "used."

**Fix:**
Changed calculation to count only pages representing real memory pressure:
```
used = active_count + wire_count + compressor_page_count
```

**Before:** `0.995` (99.5%)
**After:** `0.798` (79.8%)
**macOS reported:** 65% used (35% free)

**Lesson:**
OS-level memory semantics differ from system-level memory pressure. "Free" on macOS does not mean "available." Sensor adapters must account for platform-specific memory management behavior.

---

## VER-002: Battery sensor scale mismatch with threshold rules

**Date:** 2026-02-13
**Sensor:** `BatterySensor`
**Severity:** Medium — false DEGRADED state on startup

**Observation:**
Battery sensor reported values in the 0–100 range (e.g., `19.0` for 19% charge). The threshold rule expected all sensors to operate on a 0.0–1.0 normalized scale, causing any battery reading above `0.95` (i.e., above 0.95%) to trigger DEGRADED.

**Root Cause:**
`pmset -g batt` outputs battery as a percentage (0–100). The sensor passed this value through without normalization. All other sensors (CPU, memory, storage) reported as 0.0–1.0 ratios.

**Fix:**
Normalized battery reading by dividing by 100.0 in `BatterySensor::read()`. Updated `configs/default.json` to reflect 0.0–1.0 bounds for battery.

**Before:** `19.0` (raw percentage)
**After:** `0.21` (normalized ratio)

**Lesson:**
All sensors feeding into a shared rule evaluation pipeline must use a consistent value scale. Normalization belongs in the sensor adapter, not the rule layer.
