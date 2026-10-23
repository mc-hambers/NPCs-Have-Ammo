"""Verify the compiled DLL and installed reset-hook mappings without launching Skyrim."""
import argparse
import ctypes
import json
import struct
from pathlib import Path

def address_library(path):
    data=path.read_bytes()
    fmt=struct.unpack_from('<I',data)[0]
    version=struct.unpack_from('<4I',data,4)
    if fmt != 5: raise ValueError('This hook-file audit requires the installed format-5 Address Library')
    pointer_size, reserved, count=struct.unpack_from('<3I',data,84)
    if pointer_size != 8 or len(data)<96+count*4: raise ValueError('Invalid Address Library')
    def lookup(identifier):
        if identifier>=count: raise ValueError('ID outside mapping')
        result=struct.unpack_from('<I',data,96+identifier*4)[0]
        if result in (0,0xFFFFFFFF): raise ValueError('Missing relocation')
        return result
    return version,lookup

class PE:
    def __init__(self,path):
        self.data=path.read_bytes()
        header=struct.unpack_from('<I',self.data,0x3C)[0]
        if self.data[header:header+4] != b'PE\0\0': raise ValueError('Invalid PE')
        machine,sections=struct.unpack_from('<HH',self.data,header+4)
        if machine != 0x8664: raise ValueError('Not x64')
        opt_size=struct.unpack_from('<H',self.data,header+20)[0]
        self.base=struct.unpack_from('<Q',self.data,header+24+24)[0]
        self.sections=[]
        start=header+24+opt_size
        for n in range(sections):
            offset=start+n*40
            name=self.data[offset:offset+8].rstrip(b'\0').decode()
            virtual_size,rva,size,file_offset=struct.unpack_from('<4I',self.data,offset+8)
            flags=struct.unpack_from('<I',self.data,offset+36)[0]
            self.sections.append((name,rva,max(virtual_size,size),file_offset,flags))
    def offset(self,rva):
        for name,start,size,file_offset,flags in self.sections:
            if start<=rva<start+size: return file_offset+rva-start
        raise ValueError('RVA outside image sections')
    def executable(self,rva):
        return any(start<=rva<start+size and flags & 0x20000000 for _,start,size,_,flags in self.sections)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('dll',type=Path)
    ap.add_argument('--game',type=Path)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    PE(args.dll)
    library=ctypes.WinDLL(str(args.dll.resolve()))
    for name in ['SKSEPlugin_Load','SKSEPlugin_Query']: getattr(library,name)
    exported=(ctypes.c_uint32*2).in_dll(library,'SKSEPlugin_Version')
    if exported[1] != 0x01000030: raise ValueError('Unexpected version metadata')
    plugin_name=ctypes.string_at(ctypes.addressof(exported)+8,256).split(b'\0',1)[0].decode()
    if plugin_name != 'NPCs Have Ammo (arrow/bolt restock mod)': raise ValueError('Unexpected plugin name')
    report={'plugin_name':plugin_name,'dll_architecture':'x64','windows_loader':'passed','skse_exports':'passed','version':'1.0.3','gameplay_tested':False}
    if args.game:
        version,lookup=address_library(args.game/'Data/SKSE/Plugins/versionlib-1-7-104-0.bin')
        game=PE(args.game/'SkyrimSE.exe')
        hooks={}
        for label,identifier in [('Actor',207511),('Character',207886)]:
            table_rva=lookup(identifier)
            pointer=struct.unpack_from('<Q',game.data,game.offset(table_rva+0x8A*8))[0]
            target_rva=pointer-game.base
            if not game.executable(target_rva): raise ValueError(f'{label} reset slot does not point to executable code')
            hooks[label]={'address_library_id':identifier,'vtable_rva':hex(table_rva),'slot':'0x8A','reset_target_rva':hex(target_rva)}
        report['installed_runtime']='.'.join(map(str,version))
        report['reset_hook_file_audit']=hooks
        key=b'bForceNPCsUseAmmo:Combat'
        if key not in game.data: raise ValueError('NPC ammo consumption setting name absent')
        report['ammo_setting_name_file_check']='passed'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))

if __name__=='__main__': main()
