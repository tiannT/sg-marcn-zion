#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Execute the real SG2044 initialization code with mocked MMIO and PCD-derived expectations."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--uefi-dsc', type=Path, required=True)
    ap.add_argument('--output-dir', type=Path, required=True)
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    source = repo / 'hypervisor/arch/riscv/sg2044_static.c'
    fields = {}
    for field, index, data in re.findall(r'PcdPcieHostBridgeTable\.(\w+)\[(\d+)\]\|\{([^}]+)\}', args.uefi_dsc.read_text()):
        if field in {'PcieReg','Pcie32BitSpaceStartAddr','Pcie32BitSpaceEndAddr','Pcie64BitSpaceStartAddr','Pcie64BitSpaceEndAddr'}:
            fields[field, int(index)] = bytes(int(x.strip(), 0) for x in data.split(','))
    plic = 0x6d40000000
    expected = {}
    routes = []
    for i in range(5):
        ctrl = struct.unpack('<8Q', fields['PcieReg', i])[2]
        begin32 = int.from_bytes(fields['Pcie32BitSpaceStartAddr', i], 'little')
        end32 = int.from_bytes(fields['Pcie32BitSpaceEndAddr', i], 'little')
        begin64 = int.from_bytes(fields['Pcie64BitSpaceStartAddr', i], 'little')
        end64 = int.from_bytes(fields['Pcie64BitSpaceEndAddr', i], 'little')
        values = {ctrl+0xc64: (begin64 >> 16) & 0xffffffff,
                  ctrl+0xc68: (end64 >> 16) & 0xffffffff,
                  ctrl+0xc6c: (((end32 >> 16) & 0xffff) << 16) | ((begin32 >> 16) & 0xffff)}
        expected.update(values)
        routes.append(values)
    for context in range(128):
        for word in range(27):
            expected[plic+0x2000+context*0x80+word*4] = 0
        expected[plic+0x200000+context*0x1000] = 0 if context in (0,2) else 7
    for irq in range(1,864):
        expected[plic+irq*4] = 0
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='sg2044-board-test-') as temp:
        t = Path(temp)
        (t/'asm').mkdir()
        (t/'types.h').write_text('#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n#define __aligned(x) __attribute__((aligned(x)))\n')
        (t/'sprintf.h').write_text('#include <stdio.h>\n')
        (t/'rtl.h').write_text('#include <string.h>\n')
        (t/'asm/page.h').write_text('#define PAGE_SIZE 4096\n')
        (t/'asm/static_vm.h').write_text('#include <types.h>\nbool static_vm_ram_range(uint16_t vm, uint64_t address, uint64_t bytes);\n')
        (t/'asm/io.h').write_text('''#include <stdint.h>
uint32_t mock_read(uint64_t address);
void mock_write(uint32_t value, uint64_t address);
#define mmio_readl(addr) mock_read((uint64_t)(uintptr_t)(addr))
#define mmio_writel(value,addr) mock_write((value),(uint64_t)(uintptr_t)(addr))
''')
        (t/'asm/cpu.h').write_text('uint64_t mock_csr(const char *name);\n#define cpu_csr_read(name) mock_csr(#name)\n')
        (t/'mock.c').write_text('''#include <types.h>
#include <string.h>
#include <stdio.h>
#include <asm/apicreg.h>
#include <asm/sg2044_static.h>
struct item { uint64_t address; uint32_t value; } cells[8192], writes[8192];
unsigned int ncells, nwrites;
uint64_t fail_address, fake_time, fake_hart;
uint64_t reads[32768];
unsigned int nreads;
static struct plic_regs fixture;
char output[131072];
void reset(void) { ncells=nwrites=nreads=0;fail_address=0;output[0]=0; }
uint32_t mock_read(uint64_t address) {
 if(nreads>=32768)__builtin_trap();
 reads[nreads++]=address;
 for (unsigned int i=0;i<ncells;i++) if(cells[i].address==address) return cells[i].value;
 return 0x13579bdf;
}
void set_cell(uint64_t address, uint32_t value) {
 for (unsigned int i=0;i<ncells;i++) if(cells[i].address==address){cells[i].value=value;return;}
 cells[ncells++]=(struct item){address,value};
}
void mock_write(uint32_t value, uint64_t address) {
 if(nwrites>=8192) __builtin_trap();
 writes[nwrites++]=(struct item){address,value};
 if(address!=fail_address) set_cell(address,value);
}
uint64_t mock_csr(const char *name) {
 if(!strcmp(name,"time"))return fake_time;
 if(!strcmp(name,"mhartid"))return fake_hart;
 if(!strcmp(name,"mie"))return 0xaaa;
 return 0;
}
void early_printk(const char *s) {
 size_t have=strlen(output), add=strlen(s);
 if(have+add>=sizeof(output))__builtin_trap();
 memcpy(output+have,s,add+1);
}
void fail_at(uint64_t address) {fail_address=address;}
unsigned int write_count(void){return nwrites;}
uint64_t write_address(unsigned int i){return writes[i].address;}
uint32_t write_value(unsigned int i){return writes[i].value;}
const char *logs(void){return output;}
void clear_logs(void){output[0]=0;}
void set_clock(uint64_t tick,uint64_t hart){fake_time=tick;fake_hart=hart;}
bool static_vm_ram_range(uint16_t vm,uint64_t address,uint64_t bytes) {
 return vm==0 && bytes && address>=0xa0000000UL && address<=0x2a0000000UL && bytes<=0x2a0000000UL-address;
}
void bind_fixture(void){memset(&fixture,0,sizeof(fixture));sg2044_static_irq_bind(&fixture);}
void set_fixture_irq(unsigned int irq,unsigned int p,unsigned int c,unsigned int e) {
 unsigned int bit=1U<<(irq&31),w=irq>>5;
 fixture.pending[w]=(fixture.pending[w]&~bit)|(p?bit:0);
 fixture.claimed[w]=(fixture.claimed[w]&~bit)|(c?bit:0);
 fixture.enable[1][w]=(fixture.enable[1][w]&~bit)|(e?bit:0);
 fixture.source_priority[irq]=1;
}
unsigned int read_count(void){return nreads;}
uint64_t read_address(unsigned int i){return reads[i];}
''')
        libpath = t/'board-test.so'
        command = ['cc','-shared','-fPIC','-O1','-Wall','-Wextra','-Werror','-Wdeclaration-after-statement',
                   '-DCONFIG_MACRN','-DCONFIG_STATIC_DUAL','-DCONFIG_PLATFORM_SG2044','-DCONFIG_NR_CPUS=2',
                   '-I'+str(t),'-I'+str(repo/'hypervisor/include/arch/riscv'),
                   str(source),str(t/'mock.c'),'-o',str(libpath)]
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (args.output_dir/'compile.log').write_text(proc.stdout)
        proc.check_returncode()
        lib = ctypes.CDLL(str(libpath))
        u64,u32 = ctypes.c_uint64,ctypes.c_uint32
        lib.mock_read.argtypes=[u64];lib.mock_read.restype=u32
        lib.set_cell.argtypes=[u64,u32];lib.fail_at.argtypes=[u64]
        lib.write_address.argtypes=[u32];lib.write_address.restype=u64
        lib.write_value.argtypes=[u32];lib.write_value.restype=u32
        lib.logs.restype=ctypes.c_char_p
        lib.set_clock.argtypes=[u64,u64]
        lib.read_address.argtypes=[u32];lib.read_address.restype=u64
        lib.set_fixture_irq.argtypes=[u32,u32,u32,u32]
        lib.sg2044_static_devices_prepare.restype=ctypes.c_bool
        lib.sg2044_static_irq_trace.argtypes=[ctypes.c_char_p,u32]
        lib.reset()
        assert lib.sg2044_static_devices_prepare()
        writes = [(lib.write_address(i),lib.write_value(i)) for i in range(lib.write_count())]
        assert all(a in expected for a,v in writes), 'Unexpected MMIO write address'
        assert all(lib.mock_read(a)==v for a,v in expected.items()), 'PCD/config mismatch'
        (args.output_dir/'init.log').write_bytes(lib.logs())
        results.append(dict(case='PCD-derived PCIe routes and physical PLIC ownership',passed=True,writes=len(writes),unique_addresses=len(expected)))
        first = next(iter(routes[0]))
        lib.reset();lib.set_cell(first,0xffffffff)
        assert not lib.sg2044_static_devices_prepare() and lib.write_count()==0
        results.append(dict(case='Inaccessible PCI controller refuses initialization',passed=True))
        lib.reset();lib.fail_at(first)
        assert not lib.sg2044_static_devices_prepare() and lib.write_count()==3
        results.append(dict(case='HNI readback failure refuses Guest boot',passed=True))
        lib.reset();lib.fail_at(plic+0x2000)
        assert not lib.sg2044_static_devices_prepare()
        assert lib.write_count()==16
        results.append(dict(case='PLIC enable readback failure refuses Guest boot',passed=True))
        lib.reset()
        lib.sg2044_static_irq_trace(b'M_CLAIM',351)
        lib.sg2044_static_irq_trace(b'M_CLAIM',856)
        lib.sg2044_static_irq_trace(b'UNKNOWN',352)
        assert not lib.logs()
        for _ in range(100):lib.sg2044_static_irq_trace(b'S_INJECT',357)
        assert lib.logs().count(b'[IRQ] S_INJECT source=357 ')==2
        for phase in [b'M_CLAIM',b'S_INJECT',b'V_CLAIM',b'V_COMPLETE']:
            for irq in [64,65,73,74,125,*range(352,385)]:
                for _ in range(5):lib.sg2044_static_irq_trace(phase,irq)
        assert lib.logs().count(b'[IRQ] ')==96
        assert b'M_CLAIM source=65 ' in lib.logs()
        results.append(dict(case='Per-source/stage two-line limit, INTx inclusion and global 96-line cap',passed=True))
        lib.clear_logs();before=lib.write_count()
        lib.set_clock(10*50000000,1);lib.sg2044_static_irq_observe();assert not lib.logs()
        lib.set_clock(10*50000000,0);lib.sg2044_static_irq_observe()
        assert not lib.logs(), 'Observer must wait until VM0 PLIC is initialized'
        lib.bind_fixture();lib.set_fixture_irq(65,1,0,1)
        for sec in (10,11,19):
            lib.set_clock(sec*50000000,0);lib.sg2044_static_irq_observe()
        # Read-only legacy mode observations on the second sample.
        cfg=0x5000000000
        for addr,value in [(cfg,0x504515b7),(cfg+4,7),(cfg+0x3c,0x1ff),
                           (cfg+0x34,0xb0),(cfg+0xb0,0x00400011),
                           (cfg+0xb4,0x3000),(cfg+0x10,0x50800004),(cfg+0x14,0),
                           (0x6c000c0c48,0x20),(0x6c000c0ca0,2),
                           (0x50800014,0),(0x5080001c,0)]:
            lib.set_cell(addr,value)
        lib.set_clock(20*50000000,0);lib.sg2044_static_irq_observe()
        assert b'INTX_DISABLE=0 PIN=1' in lib.logs() and b'MSIX_ENABLED=0' in lib.logs()
        assert b'[NVME] VECTOR' not in lib.logs()
        assert b'CTRL_SIG=20 CTRL_IRQ_EN=2' in lib.logs()
        assert b'source=65 M=5 CHECK=5 VCLAIM=5 VCOMPLETE=5 P=1 C=0 E=1 PRI=1 THR=0' in lib.logs()
        results.append(dict(case='Legacy NVMe config observed without reading disabled MSI-X table',passed=True))
        results.append(dict(case='Correct CTRL +0xc00 and counters continue beyond text budget',passed=True))
        # Enabled MSI-X comparison, including a masked I/O vector.
        lib.set_cell(cfg+0xb0,0x80400011)
        for vector,data,mask in [(0,5,0),(1,6,1)]:
            entry=0x50803000+vector*16
            for off,value in [(0,0x7ee00000),(4,0),(8,data),(12,mask)]:lib.set_cell(entry+off,value)
        for sec in (30,40):
            lib.set_clock(sec*50000000,0);lib.sg2044_static_irq_observe()
        assert b'VECTOR0 addr=0:7ee00000 data=5 masked=0 plic_source=357' in lib.logs()
        assert b'VECTOR1 addr=0:7ee00000 data=6 masked=1 plic_source=358' in lib.logs()
        assert lib.logs().count(b'[IRQ] SNAPSHOT')==3
        # Command metadata in VM0-private RAM; never follow payload PRPs.
        asq,cq=0xa0400000,0xa0500000
        for address,value in [(0x50800014,1),(0x5080001c,1),(0x5080000c,0),
                              (0x50800024,0x3f003f),(0x50800028,asq),(0x5080002c,0),
                              (0x50800030,0xa0600000),(0x50800034,0),
                              (asq,5),(asq+24,cq),(asq+28,0),
                              (asq+40,(1023<<16)|1),(asq+44,3)]:lib.set_cell(address,value)
        for j in range(4):
            lib.set_cell(cq+j*16+8,0x10001)
            lib.set_cell(cq+j*16+12,0x10000|j)
        lib.set_clock(70*50000000,0);lib.sg2044_static_irq_observe()
        assert b'QSIZE=1024 PC=1 IEN=1 IV=0 CQ=a0500000' in lib.logs()
        assert b'CQ1[0] SQID=1 SQHEAD=1 CID=0 STATUS=1 PHASE=1' in lib.logs()
        results.append(dict(case='Read-only NVMe CREATE_CQ1 interrupt flags and bounded completion metadata',passed=True))
        read_start=lib.read_count()
        lib.set_cell(asq+24,0);lib.set_cell(asq+28,4)
        for sec in (130,140,170):
            lib.set_clock(sec*50000000,0);lib.sg2044_static_irq_observe()
        assert b'CQ_NOT_IN_VM0_RAM' in lib.logs()
        assert not any(0x400000000<=lib.read_address(i)<0x500000000 for i in range(read_start,lib.read_count()))
        assert not any(lib.read_address(i) in [0x6c000c0048,0x6c000c00a0] for i in range(lib.read_count()))
        assert lib.logs().count(b'[IRQ] SNAPSHOT')==5
        results.append(dict(case='Foreign CQ memory refused and obsolete CTRL addresses never read',passed=True))
        assert lib.write_count()==before, 'Observation must not program hardware'
        (args.output_dir/'observation.log').write_bytes(lib.logs())
        results.append(dict(case='Enabled MSI-X vector address/mask/source decoding is read-only',passed=True))
        results.append(dict(case='Hart0 snapshots: bound-PLIC gate, read-only, elapsed 0/10/30/60/120 seconds, max five',passed=True))
    record=dict(passed=all(x['passed'] for x in results),results=results,
                source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                factory_dsc_sha256=hashlib.sha256(args.uefi_dsc.read_bytes()).hexdigest(),
                scope='Host execution of real initialization C with mocked MMIO/CSRs. No physical hardware accessed.',compile_command=command)
    (args.output_dir/'results.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record,indent=2))


if __name__=='__main__':
    main()
