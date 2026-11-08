"""Extract final-stage spawn and portal data from the original program.

This is a controller-route aid, not a gameplay implementation. Section
membership of a portal is deliberately not inferred from its coordinates.
"""
import argparse
import hashlib
import json
from pathlib import Path


def audit(program):
    def word(at):
        return int.from_bytes(program[at:at + 2], "big")

    def spawn_table(at):
        last = word(at)
        if last & 0x8000:
            return []
        records = []
        for index in range(last + 1):
            pos = at + 2 + index * 8
            records.append({
                "source_address": f"{pos:06x}",
                "class": program[pos],
                "variant": program[pos + 1],
                "selector": program[pos + 2],
                "flags": program[pos + 3],
                "x": word(pos + 4), "y": word(pos + 6),
            })
        return records

    if len(program) < 0x4109a:
        raise ValueError("Original program is too short for the spawn tables")
    common = spawn_table(0x40b9a)
    portals = []
    for record in common:
        if record["class"] != 14 or record["selector"] != 2:
            continue
        variant = record["variant"]
        if variant > 11:
            raise ValueError("Unexpected portal variant")
        portals.append({
            **record,
            "recurring": bool(program[0x2da4c + variant]),
            "destination_section": word(0x2d974 + variant * 2),
            "destination_x": word(0x2d994 + variant * 4),
            "destination_y": word(0x2d996 + variant * 4),
            "admission": {"source_entry": "02d7ea", **(
                {"flag_address": "83e7", "flag_nonzero": True}
                if variant == 10 else {
                    "x_tolerance": 12, "y_tolerance": 14,
                    "required_input_bits": 8, "forbidden_input_bits": 3,
                    "hp_address": "8362", "hp_nonzero": True,
                })},
        })
    sections = []
    for section in range(7):
        offset = word(0x4033c + section * 2)
        if offset & 0x8000:
            offset -= 0x10000
        at = 0x4033c + offset
        sections.append({"section": section, "table": f"{at:06x}",
                         "records": spawn_table(at)})
    return {"program_sha256": hashlib.sha256(program).hexdigest(),
            "common_table": "040b9a", "portals": portals,
            "sections": sections}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(audit(args.program.read_bytes()), indent=2) + "\n"
    if args.output:
        args.output.write_text(result)
    else:
        print(result, end="")
