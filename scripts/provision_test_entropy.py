#!/usr/bin/env python3
"""Provision independent SINGLE-BOOT test seeds without logging seed bytes.

The trusted build workstation supplies these seeds. Regenerate the candidate
before every acceptance boot. This is not a persistent production entropy
service or an authenticated/secret boot-media provisioning scheme.
"""
from pathlib import Path
import secrets
import struct


def provision(data):
    fields = list(struct.unpack_from('>10I', data))
    magic, total, structure, strings, reserve, version, compatible, bootcpu, ns, nt = fields
    if magic != 0xd00dfeed or total != len(data) or version < 17:
        raise ValueError('Expected a complete FDT v17 or newer')
    names = bytearray(data[strings:strings+ns])
    added = {}
    for name in ['macrn,vm0-rng-seed','macrn,vm1-rng-seed']:
        added[name] = (len(names), secrets.token_bytes(32))
        names.extend(name.encode()+b'\0')
    source = memoryview(data)[structure:structure+nt]
    output = bytearray()
    stack = []
    offset = 0
    found = False
    while offset < len(source):
        start = offset
        token = struct.unpack_from('>I', source, offset)[0]
        offset += 4
        if token == 1:
            end = bytes(source).index(b'\0', offset)
            stack.append(bytes(source[offset:end]).decode())
            offset = (end+4) & ~3
        elif token == 2:
            if '/'.join(stack) == '/chosen':
                for name_offset, seed in added.values():
                    output.extend(struct.pack('>III', 3, len(seed), name_offset)+seed)
                found = True
            stack.pop()
        elif token == 3:
            length, name_offset = struct.unpack_from('>II', source, offset)
            offset += 8
            end = data.index(b'\0', strings+name_offset)
            name = data[strings+name_offset:end].decode()
            offset = (offset+length+3) & ~3
            if '/'.join(stack) == '/chosen' and name in added:
                continue
        elif token == 4:
            pass
        elif token == 9:
            output.extend(source[start:offset])
            break
        else:
            raise ValueError(f'Invalid FDT token {token}')
        output.extend(source[start:offset])
    if not found or stack:
        raise ValueError('Missing chosen node or unbalanced FDT')
    end_reserve = reserve
    while struct.unpack_from('>QQ', data, end_reserve) != (0, 0):
        end_reserve += 16
    reservations = data[reserve:end_reserve+16]
    structure_offset = 40+len(reservations)
    strings_offset = structure_offset+len(output)
    header = struct.pack('>10I', magic, strings_offset+len(names), structure_offset,
                         strings_offset, 40, version, compatible, bootcpu, len(names), len(output))
    return header+reservations+output+names


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Refuse to reuse an existing single-boot candidate DTB')
    args.output.write_bytes(provision(args.input.read_bytes()))
    args.output.chmod(0o600)
    print('PROVISIONED_SINGLE_BOOT_TEST_ENTROPY; VM seeds independent; values not logged')
