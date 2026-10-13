#!/usr/bin/env python3
"""Build and verify Lab 2 in QEMU; each expected panic runs in a fresh VM."""
import argparse
import json
import pathlib
import re
import select
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
NEGATIVE = {
    1: "pmem_alloc: kernel pool exhausted",
    2: "pmem_alloc: user pool exhausted",
    3: "pmem_free: invalid page or wrong pool",
    4: "pmem_free: double free",
    5: "pmem_free: invalid page or wrong pool",
    6: "vm_mappages: remap to different PA",
    7: "vm_mappages: invalid permissions",
    8: "vm range: invalid VA or length",
    9: "vm_unmappages: missing leaf",
    10: "scause=15",  # write to kernel text
    11: "scause=15",  # write to rodata
    12: "scause=15",  # unmapped firmware
    13: "vm_getpte: invalid root or VA",
    14: "vm_mappages: invalid PA range",
    15: "vm range: invalid VA or length",
    16: "pmem_free: invalid page or wrong pool",
    17: "pmem_free: invalid page or wrong pool",
    18: "vm range: invalid VA or length",
    19: "vm range: invalid VA or length",
    20: "vm_mappages: invalid permissions",
    21: "scause=15",  # removed alias must fault after sfence
    22: "scause=12",  # RW data cannot execute
}


def build(directory, cpus, testing, case=0):
    args = ["make", "-j4", f"BUILD={directory}", f"CPUS={cpus}",
            f"LAB2_TEST={int(testing)}", f"TEST_CASE={case}"]
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / f"build-{case}.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return directory / "kernel.elf"


def boot(elf, cpus, finished, timeout=60):
    args = ["qemu-system-riscv64", "-machine", "virt", "-bios", "default",
            "-m", "128M", "-smp", str(cpus), "-nographic", "-kernel", str(elf)]
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = b""
    deadline = time.monotonic() + timeout
    try:
        while time.monotonic() < deadline:
            ready, _, _ = select.select([proc.stdout], [], [], 0.2)
            if ready:
                chunk = proc.stdout.read1(65536)
                if not chunk:
                    break
                output += chunk
                text = output.decode(errors="replace").replace("\r", "")
                if finished(text):
                    return text
            if proc.poll() is not None:
                break
        raise RuntimeError("QEMU timed out or exited:\n" + output.decode(errors="replace"))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cpus", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument("--skip-negative", action="store_true")
    options = parser.parse_args()
    if any(cpu < 1 or cpu > 8 for cpu in options.cpus):
        parser.error("CPU counts must be between 1 and 8")
    audit = ROOT / "build" / "tests"
    results = []
    for cpus in options.cpus:
        directory = audit / f"boot-{cpus}"
        elf = build(directory, cpus, False)
        output = boot(elf, cpus, lambda s: len(re.findall(r"^hart \d+: Sv39 enabled,[^\n]*\n", s, re.M)) == cpus)
        (directory / "boot.log").write_text(output)
        ids = [int(x) for x in re.findall(r"^hart (\d+): Sv39 enabled,", output, re.M)]
        assert sorted(ids) == list(range(cpus)), output
        satps = re.findall(r"Sv39 enabled, satp=(0x[0-9a-f]+)", output)
        assert len(set(satps)) == 1 and int(satps[0], 16) >> 60 == 8, output
        assert "PANIC" not in output and "TRAP" not in output, output
        assert "physical pages: kernel=1024 " in output, output
        results.append({"test": "boot", "cpus": cpus, "passed": True})
        print(f"PASS normal boot: {cpus} hart(s), shared Sv39 root", flush=True)

        directory = audit / f"memory-{cpus}"
        elf = build(directory, cpus, True)
        output = boot(elf, cpus, lambda s: re.search(r"(?:LAB2 TEST PASS|PANIC)[^\n]*\n", s))
        (directory / "boot.log").write_text(output)
        assert f"LAB2 TEST PASS cpus={cpus}" in output and "PANIC" not in output, output
        assert "PASS physical:" in output and "PASS virtual:" in output, output
        ids = [int(x) for x in re.findall(r"^PASS hart (\d+):", output, re.M)]
        assert sorted(ids) == list(range(cpus)), output
        results.append({"test": "memory_and_paging", "cpus": cpus, "passed": True})
        print(f"PASS memory and paging: {cpus} hart(s)", flush=True)

    if not options.skip_negative:
        directory = audit / "negative"
        for case, expected in NEGATIVE.items():
            elf = build(directory, 1, True, case)
            output = boot(elf, 1, lambda s: re.search(r"PANIC[^\n]*\n", s))
            (directory / f"case-{case}.log").write_text(output)
            assert expected in output, f"case {case}: expected {expected}\n{output}"
            if case in (10, 11, 12, 21, 22):
                target = {10: 0x80200000, 12: 0x80000000,
                          21: 0x10010000, 22: 0x10010000}.get(case)
                if target is not None:
                    assert f"stval=0x{target:016x}" in output, output
            results.append({"test": "expected_failure", "case": case, "passed": True})
            print(f"PASS expected failure {case}: {expected}", flush=True)
    audit.mkdir(parents=True, exist_ok=True)
    (audit / "results.json").write_text(json.dumps(results, indent=2))
    print(f"All {len(results)} QEMU checks passed. Logs: {audit}", flush=True)


if __name__ == "__main__":
    main()
