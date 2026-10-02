#!/usr/bin/env python3
import sys
import re
import math

def calculate_percentiles(values):
    if not values:
        return None
    s = sorted(values)
    n = len(s)
    def get_p(p):
        idx = (p / 100.0) * (n - 1)
        lower = int(math.floor(idx))
        upper = int(math.ceil(idx))
        if lower == upper:
            return s[lower]
        return s[lower] + (s[upper] - s[lower]) * (idx - lower)
    return {
        'count': n,
        'p50': get_p(50),
        'p95': get_p(95),
        'p99': get_p(99),
        'min': s[0],
        'max': s[-1],
        'avg': sum(s) / n
    }

def main():
    log_file = sys.argv[1] if len(sys.argv) > 1 else "/tmp/clak.log"
    groups = {}
    pattern = re.compile(r'\[LATENCY\]\s+group=([^\s]+)\s+delta_ms=([0-9\.]+)\s+action=([^\s]+)')

    try:
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                m = pattern.search(line)
                if m:
                    group, delta_str, action = m.group(1), m.group(2), m.group(3)
                    if action.startswith("REPLACE"):
                        delta = float(delta_str)
                        groups.setdefault(group, []).append(delta)
    except Exception as e:
        print(f"Error reading {log_file}: {e}")
        return

    print(f"{'Group':<28} | {'Count':<6} | {'p50 (ms)':<9} | {'p95 (ms)':<9} | {'p99 (ms)':<9} | {'Min':<6} | {'Max':<6}")
    print("-" * 85)
    for g, vals in sorted(groups.items()):
        stats = calculate_percentiles(vals)
        print(f"{g:<28} | {stats['count']:<6} | {stats['p50']:<9.2f} | {stats['p95']:<9.2f} | {stats['p99']:<9.2f} | {stats['min']:<6.2f} | {stats['max']:<6.2f}")

if __name__ == '__main__':
    main()
