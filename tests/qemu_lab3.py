#!/usr/bin/env python3
"""Exercise real timer/PLIC interrupts and asynchronous register restoration."""
import argparse
import json
import pathlib
import re
import select
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
AUDIT = ROOT / "build" / "lab3-tests"


def build(directory, cpus, testing=False, case=0):
    directory.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        ["make", "-j4", f"BUILD={directory}", f"CPUS={cpus}",
         "LAB2_TEST=0", f"LAB3_TEST={int(testing)}", f"TEST_CASE={case}"],
        cwd=ROOT, capture_output=True, text=True)
    (directory / f"build-{case}.log").write_text(proc.stdout + proc.stderr)
    if proc.returncode:
        raise RuntimeError(proc.stdout + proc.stderr)
    return directory / "kernel.elf"


class Machine:
    def __init__(self, elf, cpus, log):
        self.log = log
        self.data = b""
        self.proc = subprocess.Popen(
            ["qemu-system-riscv64", "-machine", "virt", "-bios", "default",
             "-m", "128M", "-smp", str(cpus), "-nographic", "-kernel", str(elf)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    @property
    def text(self):
        return self.data.decode(errors="replace").replace("\r", "")

    def until(self, condition, timeout=30, allow_panic=False):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = self.text
            if not allow_panic and "PANIC" in text:
                raise AssertionError(text)
            if condition(text):
                return text
            ready, _, _ = select.select([self.proc.stdout], [], [], 0.1)
            if ready:
                chunk = self.proc.stdout.read1(65536)
                if not chunk:
                    break
                self.data += chunk
            if self.proc.poll() is not None:
                break
        raise RuntimeError("QEMU timed out or exited:\n" + self.text)

    def send(self, payload):
        self.proc.stdin.write(payload)
        self.proc.stdin.flush()

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        self.log.write_text(self.text)


def normal(cpus):
    directory = AUDIT / f"normal-{cpus}"
    machine = Machine(build(directory, cpus), cpus, directory / "boot.log")
    try:
        text = machine.until(lambda s: len(re.findall(
            r"^hart \d+: trap entry ready[^\n]*\n", s, re.M)) == cpus)
        ids = re.findall(r"^hart (\d+): trap entry ready", text, re.M)
        assert sorted(map(int, ids)) == list(range(cpus)), text
        satps = re.findall(r"Sv39 enabled, satp=(0x[0-9a-f]+)", text)
        assert len(satps) == cpus and len(set(satps)) == 1, text
        for payload in (b"Lab3_echo_0123456789!?", b"second_irq_ABCxyz", b"third_irq_done"):
            # Multiple requests verify that claim/complete re-enables the source.
            for begin in range(0, len(payload), 8):
                part = payload[begin:begin + 8]
                marker = len(machine.data)
                machine.send(part)
                machine.until(lambda s, part=part, marker=marker:
                              part.decode() in machine.data[marker:].decode(errors="replace"))
            machine.send(b"\r")
        text = machine.until(lambda s: "timer: ticks=10 (~1 s)\n" in s)
        assert "TRAP" not in text and "PANIC" not in text, text
        return {"test": "boot_uart_timer", "cpus": cpus, "passed": True}
    finally:
        machine.close()


def context(cpus):
    directory = AUDIT / f"context-{cpus}"
    machine = Machine(build(directory, cpus, True), cpus, directory / "boot.log")
    try:
        text = machine.until(lambda s: "LAB3 UART READY\n" in s)
        assert f"LAB3 TEST PASS cpus={cpus}" in text, text
        ids = re.findall(r"^PASS hart (\d+): register restore", text, re.M)
        assert sorted(map(int, ids)) == list(range(cpus)), text
        match = re.search(r"LAB3 TEST PASS cpus=\d+ ticks=(\d+) boot_hart=(\d+)", text)
        boot_hart = int(match[2])
        boot_count = re.search(rf"PASS hart {boot_hart}:.*timer=(\d+)", text)
        # Other harts or a busy host may delay the summary by many ticks.
        # The kernel itself verifies total <= the boot hart's current count.
        assert 5 <= int(boot_count[1]) <= int(match[1]), text
        machine.send(b"Z")
        text = machine.until(lambda s: "LAB3 UART PASS received=1 external=1\n" in s)
        assert "\nZ\nLAB3 UART PASS" in text, text
        return {"test": "registers_locks_per_hart_timer_plic", "cpus": cpus,
                "boot_hart": boot_hart, "passed": True}
    finally:
        machine.close()


def negative(case, cause, address=None):
    directory = AUDIT / f"exception-{case}"
    machine = Machine(build(directory, 1, True, case), 1, directory / "boot.log")
    try:
        text = machine.until(lambda s: "PANIC hart=0: unexpected supervisor trap\n" in s,
                             allow_panic=True)
        assert f"scause={cause}" in text and "TRAP hart=0" in text, text
        if address is not None:
            assert f"stval=0x{address:016x}" in text, text
        return {"test": "exception_diagnostic", "case": case, "passed": True}
    finally:
        machine.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cpus", type=int, nargs="+", default=[1, 2, 4, 8])
    options = parser.parse_args()
    if any(cpu < 1 or cpu > 8 for cpu in options.cpus):
        parser.error("CPU counts must be between 1 and 8")
    results = []
    for cpus in options.cpus:
        results.append(normal(cpus))
        print(f"PASS boot, repeated UART echo and timer: {cpus} hart(s)", flush=True)
        results.append(context(cpus))
        print(f"PASS register restore, locks, per-hart timers and PLIC: {cpus} hart(s)", flush=True)
    results.append(negative(1, 2))
    print("PASS illegal instruction diagnostic", flush=True)
    results.append(negative(2, 15, 0x80000000))
    print("PASS unmapped store diagnostic", flush=True)
    (AUDIT / "results.json").write_text(json.dumps(results, indent=2))
    print(f"All {len(results)} Lab 3 QEMU checks passed. Logs: {AUDIT}", flush=True)


if __name__ == "__main__":
    main()
