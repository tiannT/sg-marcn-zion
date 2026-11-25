"""Keep the SG2044 legacy ISA; discard stale inherited QEMU extension lists."""
import os
from pathlib import Path
import subprocess

fdtget=os.environ.get('FDTGET','fdtget')
fdtput=os.environ.get('FDTPUT','fdtput')
env=os.environ.copy()
def get(path,node,prop):
    r=subprocess.run([fdtget,'-t','s',str(path),node,prop],
                     env=env,capture_output=True,text=True)
    return None if r.returncode else r.stdout.strip().split()
def sanitize(path):
    children=subprocess.check_output([fdtget,'-l',str(path),'/cpus'],env=env,text=True)
    changed=[]
    for child in children.splitlines():
        node='/cpus/'+child
        if get(path,node,'device_type')!=['cpu']:continue
        legacy=get(path,node,'riscv,isa')
        if legacy!=['rv64imafdcv']:raise ValueError('Unexpected native CPU ISA: '+repr(legacy))
        old=get(path,node,'riscv,isa-extensions')
        for prop in ['riscv,isa-extensions','riscv,isa-base']:
            if get(path,node,prop) is not None:
                subprocess.run([fdtput,'-d',str(path),node,prop],env=env,check=True)
        assert get(path,node,'riscv,isa-extensions') is None
        changed.append(dict(node=node,legacy_isa=legacy,removed_inherited_extensions=old))
    if not changed:raise ValueError('No native CPU nodes')
    return changed

def qemu_uart_clock(path):
    children=subprocess.check_output([fdtget,'-l',str(path),'/soc'],env=env,text=True)
    uart=next('/soc/'+n for n in children.splitlines()
              if 'snps,dw-apb-uart' in (get(path,'/soc/'+n,'compatible') or []))
    for node,prop,values in [(uart,'reg',[0,0x10000000,0,0x1000]),(uart,'interrupts',[10,4]),
                             ('/cpus','timebase-frequency',[10000000])]:
        subprocess.run([fdtput,'-t','x',str(path),node,prop,*[f'{v:x}' for v in values]],env=env,check=True)
    return dict(uart=uart,address='0x10000000',irq=[10,4],timebase=10000000,
                scope='Only physical UART address/IRQ and clock rebased for local QEMU')
