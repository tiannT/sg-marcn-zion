#!/usr/bin/env python3
"""Verify an existing R3 Linux kernel, optionally copying it from the payload."""
import argparse,hashlib,json,struct
from pathlib import Path

def main():
    root=Path(__file__).resolve().parents[1]
    reference=json.loads((root/'kernel-reference.json').read_text())
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--kernel',type=Path)
    group.add_argument('--payload',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.kernel:
        data=args.kernel.read_bytes()
    else:
        with args.payload.open('rb') as f:
            header=f.read(256)
            if header[:8]!=b'MACRNBRT':parser.error('Not an R3 unified payload')
            offset,size=struct.unpack_from('<2Q',header,40+2*48)
            if size!=reference['kernel_bytes'] or offset+size>args.payload.stat().st_size:
                parser.error('Unexpected reference kernel range')
            f.seek(offset);data=f.read(size)
    digest=hashlib.sha256(data).hexdigest()
    if len(data)!=reference['kernel_bytes'] or digest!=reference['kernel_sha256']:
        parser.error('Kernel does not match the R3 Tianquan reference')
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('xb') as f:f.write(data)
        args.output.chmod(0o600)
    print(json.dumps({'kernel_release':reference['kernel_release'],'bytes':len(data),
                      'sha256':digest,'reference_match':True},indent=2))

if __name__=='__main__':main()
