#!/usr/bin/env python
#
# ARMv8 binary replay
#
# Copyright (c) 2018 Northeastern University
#
import argparse
import binascii
import ConfigParser
import logging
import math
import mmap
import os.path
import struct
import sys
from argparse import Namespace
from bitarray import bitarray
from capstone.arm64 import *
from capstone import *
from enum import Enum
from datetime import datetime
from print_arm64_inst import print_insn_detail
from xprint import to_hex, to_x

CONFIG_SECTION_CODE_ADDRESSES = 'code-addresses'

CONFIG_DEFAULTS = {
        'load_address'   : '0x0000',
        'text_start'     : None,
        'text_end'       : None,
        'cfv_init'       : None,
        'cfv_quote'      : None,
        'omit_addresses' : None,
        'tracefile'      : None,
}

def read_config(pathname):
    parser = ConfigParser.SafeConfigParser(CONFIG_DEFAULTS)
    parser.read(pathname)

    return Namespace(
            load_address   = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'load_address'),
            text_start     = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'text_start'),
            text_end       = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'text_end'),
            cfv_init       = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'cfv_init'),
            cfv_quote      = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'cfv_quote'),
            omit_addresses = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'omit_addresses'),
            tracefile      = parser.get(CONFIG_SECTION_CODE_ADDRESSES, 'tracefile'),
    )

def hexbytes(insn):
    width = int(pow(2, math.ceil(math.log(len(insn))/math.log(2))))
    return "0x" + binascii.hexlify(bytearray(insn)).zfill(width)

def main():
    parser = argparse.ArgumentParser(description='ARMv8 Trace Based Binary Replay Tool')
    parser.add_argument('file', nargs='?', metavar='FILE',
            help='binary file to instrument')
    parser.add_argument('-L', '--load-address', dest='load_address', default=None,
            help='load address of binary image')
    parser.add_argument('--text-start', dest='text_start', default=None,
            help='start address of section to instrument')
    parser.add_argument('--text-end', dest='text_end', default=None,
            help='end address of section to instrument')
    parser.add_argument('--omit-addresses', dest='omit_addresses', default=None,
            help='comma separated list of addresses of instructions to omit from instrumentation')
    parser.add_argument('-l', '--little-endian', dest='flags', default=[],
            action='append_const', const=CS_MODE_LITTLE_ENDIAN,
            help='disassemble in little endian mode')
    parser.add_argument('-b', '--big-endian', dest='flags', default=[],
            action='append_const', const=CS_MODE_BIG_ENDIAN,
            help='disassemble in big endian mode')
    parser.add_argument('-o', '--outfile', dest='outfile', default=None,
            help='outfile for branch table')
    parser.add_argument('-t', '--tracefile', dest='tracefile', default=None,
            help='trace file for replay')
    parser.add_argument('-c', '--config', dest='config', default=None,
            help='pathname of configuration file')
    parser.add_argument('--verbose', '-v', action='count',
            help='verbose output (repeat up to three times for additional information)')

    args = parser.parse_args()

    if args.verbose is None:
        logging.basicConfig(format='%(message)s',level=logging.ERROR)
    if args.verbose is 1:
        logging.basicConfig(format='%(message)s',level=logging.WARNING)
    if args.verbose is 2:
        logging.basicConfig(format='%(message)s',level=logging.INFO)
    if args.verbose >= 3:
        logging.basicConfig(format='%(message)s',level=logging.DEBUG)

    try:
        config = read_config(args.config if args.config is not None
                else CONFIG_DEFAULT_PATHNAME)
    except ConfigParser.MissingSectionHeaderError as error:
            logging.error(error)
            sys.exit(1)

    def get_req_opt(opt):
        args_value =  getattr(args, opt) if hasattr(args, opt) else None
        config_value = getattr(config, opt) if hasattr(config, opt) else None

        if args_value is not None:
            return args_value
        elif config_value is not None:
            return config_value
        else:
            exit("%s: required option '%s' not defined" % (sys.argv[0], opt));

    def get_csv_opt(opt):
        args_value =  getattr(args, opt) if hasattr(args, opt) else None
        config_value = getattr(config, opt) if hasattr(config, opt) else None

        if args_value is not None:
            return args_value.split(',')
        elif config_value is not None:
            return config_value.split(',')
        else:
            return []

    opts = Namespace(
            binfile        = args.file,
            outfile        = args.outfile,
            cs_mode_flags  = args.flags,
            load_address   = int(get_req_opt('load_address'),   16),
            text_start     = int(get_req_opt('text_start'),     16),
            text_end       = int(get_req_opt('text_end'),       16),
            cfv_init       = int(get_req_opt('cfv_init'),       16),
            cfv_quote      = int(get_req_opt('cfv_quote'),      16),
            omit_addresses = [int(i,16) for i in get_csv_opt('omit_addresses')],
            tracefile      = args.tracefile,
    )

    logging.debug("load_address         = 0x%08x" % opts.load_address)
    logging.debug("text_start           = 0x%08x" % opts.text_start)
    logging.debug("text_end             = 0x%08x" % opts.text_end)
    logging.debug("cfv_init             = 0x%08x" % opts.cfv_init)
    logging.debug("cfv_quote            = 0x%08x" % opts.cfv_quote)
    logging.debug("omit_addresses       = %s" % ['0x%08x' % i for i in opts.omit_addresses])
    logging.debug("tracefile            = %s" % opts.tracefile)

    if not os.path.isfile(args.file):
        exit("%s: file '%s' not found" % (sys.argv[0], args.file));

    if not os.path.isfile(args.tracefile):
        exit("%s: file '%s' not found" % (sys.argv[0], args.tracefile));

    hookit(opts)

class ExecutionTrace:
    def __init__(self, tracefile):
        self.__fn = tracefile
        self.__idx = 0
        self.__trace = self.get_trace(tracefile)
        self.__len = len(self.__trace)
    def next_branch(self):
        print ("idx:%d , flag:%c"% (self.__idx, self.__trace[self.__idx]))
        if (self.__idx < self.__len):
            if self.__trace[self.__idx] == 'y':
                self.__idx += 1
                return 'y'
            elif self.__trace[self.__idx] == 'n':
                self.__idx += 1
                return 'n' 
            else:
                print ("unexpected trace symbol:" + self.__trace[self.__idx])
                return 'e'
        else:
            return 'e'
    # so far, we only accept tracefile in the format of
    # 'yyynnnyy'
    def get_trace(self, tracefile):
        with open(tracefile, 'r') as f:
            trace = f.readlines()
            
        assert(len(trace) != 0)
        return trace[0].strip()

def hookit(opts):
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM + sum(opts.cs_mode_flags))
    md.detail = True
    replay_start = False
    replay_stop = False
    taken = False
    target_address = 0
    trace = ExecutionTrace(opts.tracefile)
    trace_idx = 0
    stack = []
    ofd = open(opts.outfile,'w')
    last_flag = 'eq'
    last_ne_flag = False
    last_lt_flag = False

    with open(opts.binfile, "rb") as f:
        mm = mmap.mmap(f.fileno(), 0, prot=mmap.PROT_READ)

        offset = opts.text_start - opts.load_address
        logging.debug("hooking %s from 0x%08x to 0x%08x" % (opts.binfile, offset, opts.text_end - opts.load_address))
        mm.seek(offset)
        code = mm.read(mm.size() - mm.tell())

        current_address = opts.load_address + (offset)

        prev_address = -0x0001

        while True:
            taken = False
            if replay_stop == True:
                break

            for i in md.disasm(code, current_address):

                # Workaround for md.disasm returning dublicate instructions
                if i.address == prev_address:
                    continue
                else:
                    prev_address = i.address

                if (i.address in opts.omit_addresses):
                    logging.info("omit  at 0x%08x:         %-10s\t%s\t%s" %
                            (i.address, hexbytes(i.bytes), i.mnemonic, i.op_str))

                    if i.address >= opts.text_end:
                        break

                    continue

                # branch w/ link instruction; bl <pc relative offset>
                if (i.id == ARM64_INS_BL):
                    res = is_attestation_start(i, opts)
                    if (res):
                        replay_start = True
                        print("*******************replay start**************************")
                        ofd.write("start\n");
                        continue

                    res = is_attestation_end(i, opts)
                    if (res):
                        replay_stop = True
                        replay_start = False

                    if replay_start:
                        res = handle_branch_with_link(i, opts)
                        if res[0]:
                            taken = True
                            target_address = res[1]
                            stack.append(i.address + 4)
                            print("push stack ret address: %x" % (i.address + 4))
                            ofd.write("[bl]0x%x --> 0x%x\n" % (i.address, target_address))
                            break
                        else:
                            ofd.write("[bl][skip]0x%x\n" % (i.address))
                            pass

                elif (i.id == ARM64_INS_CSET):
                    if replay_start:
                        if 'ne' in i.op_str:
                            print 'ne-CSET:'+ i.mnemonic +'\t' +  i.op_str
                            last_ne_flag = True
                            last_flag = 'e'
                        elif 'eq' in i.op_str:
                            print 'eq-CSET:'+ i.mnemonic +'\t' +  i.op_str
                            last_ne_flag = False
                        elif 'gt' in i.op_str:
                            print 'gt-CSET:'+ i.mnemonic +'\t' +  i.op_str
                            last_lt_flag = False
                            last_flag = 'lg'
                        elif 'lt' in i.op_str:
                            print 'lt-CSET:'+ i.mnemonic +'\t' +  i.op_str
                            last_lt_flag = True 
                            last_flag = 'lg'
                        else:
                            print 'unknown-CSET:'+ i.mnemonic +'\t' +  i.op_str
                        
                ## branch instruction; b <pc relative offset>
                elif (i.id == ARM64_INS_B):
                    if replay_start:
                        if last_flag == 'e':
                            res = handle_branch(i, opts, trace, last_ne_flag)
                        else:
                            res = handle_branch(i, opts, trace, last_lt_flag)
                        if res[0]:
                            ofd.write("[b][y]0x%x --> 0x%x\n" % (i.address,res[1]))
                            taken = True
                            target_address = res[1]
                            break
                        else:
                            ofd.write("[b][n]0x%x\n" % (i.address))
                            pass # not taken

                ## branch while operand is register; br x1
                elif (i.id == ARM64_INS_BR):
                    if replay_start:
                        ofd.write("error[br]0x%x\n" % (i.address))
                        handle_branch_with_reg(i, opts)

                elif (i.id == ARM64_INS_BLR):
                    if replay_start:
                        ofd.write("error[blr]0x%x\n" % (i.address))
                        handle_branch_with_link_reg(i, opts)

                elif (i.id == ARM64_INS_RET):
                    if replay_start:
                        handle_ret(i, opts)
                        taken = True
                        target_address = stack.pop()
                        ofd.write("[ret]0x%x --> 0x%x\n" % (i.address,target_address))
                        break

                elif (i.id == ARM64_INS_TBZ):
                    if replay_start:
                        res = handle_tbz(i, opts, trace, last_ne_flag)
                        if res[0]:
                            taken = True
                            target_address = res[1]
                            ofd.write("[tbz][y]0x%x --> 0x%x\n" % (i.address,res[1]))
                            break
                        else:
                            ofd.write("[tbz][n]0x%x\n" % (i.address))
                            pass # not taken

                elif (i.id == ARM64_INS_TBNZ):
                    if replay_start:
                        res = handle_tbnz(i, opts, trace, last_ne_flag)
                        if res[0]:
                            taken = True
                            target_address = res[1]
                            ofd.write("[tnbz][y]0x%x --> 0x%x\n" % (i.address,res[1]))
                            break
                        else:
                            ofd.write("[tbnz][n]0x%x\n" % (i.address))
                            pass # not taken

                elif (i.id == ARM64_INS_CBNZ):
                    if replay_start:
                        res = handle_cbnz(i, opts, trace, last_ne_flag)
                        if res[0]:
                            taken = True
                            target_address = res[1]
                            ofd.write("[cbnz][y]0x%x --> 0x%x\n" % (i.address,res[1]))
                            break
                        else:
                            ofd.write("[cbnz][n]0x%x\n" % (i.address))
                            pass # not taken

                elif (i.id == ARM64_INS_CBZ):
                    if replay_start:
                        res = handle_cbz(i, opts, trace, last_ne_flag)
                        if res[0]:
                            taken = True
                            target_address = res[1]
                            ofd.write("[cbz][y]0x%x --> 0x%x\n" % (i.address,res[1]))
                            break
                        else:
                            ofd.write("[cbz][n]0x%x\n" % (i.address))
                            pass # not taken

                else:
                    pass
                    #print("unhandled inst")
                    #print_insn_detail(i)
                ## branch w/ link instruction; bl <pc relative offset>
                #elif (i.id == ARM64_INS_BL):
                #    pass

                ## branch w/ link while operand is register; blr x1
                #elif (i.id == ARM64_INS_BLR and len(i.operands) == 1 and i.operands[0].reg == ARM64_REG_X1):
                #    pass

                ## check for currently unhandled instructions
                #elif (i.id == ARM64_INS_BR):
                #    logging.warn("br    at 0x%08x: %-10s\t%s\t%s" %
                #            (i.address, hexbytes(i.bytes), i.mnemonic, i.op_str))

                #elif (i.id == ARM64_INS_BLR):
                #    logging.warn("blr    at 0x%08x: %-10s\t%s\t%s" %
                #            (i.address, hexbytes(i.bytes), i.mnemonic, i.op_str))

                #else:
                #    logging.debug("      0x%08x: %-10s\t%s\t%s" %
                #            (i.address, hexbytes(i.bytes), i.mnemonic, i.op_str))

                #logging.debug("      0x%08x: %-10s\t%s\t%s" %
                #    (i.address, hexbytes(i.bytes), i.mnemonic, i.op_str))

                if i.address >= opts.text_end:
                    break

                if taken:
                    break
            print("=========================block=========================")

            if taken:
                current_address = target_address
            else:
                current_address = (i.address if i.address > current_address
                                         else current_address + 4)

            if (current_address >= opts.text_end or
                current_address >= opts.load_address + mm.size()):
                break

            try:
                mm.seek(current_address - opts.load_address)
                code = mm.read(mm.size() - mm.tell())
            except:
                print ("current_address:0x%x, load_address:0x%x"%(current_address, opts.load_address))
                break

            if replay_stop == True:
                print("*******************replay stop**************************")
                break

    return

def hexbytes(insn):
    width = int(pow(2, math.ceil(math.log(len(insn))/math.log(2))))
    return "0x" + binascii.hexlify(bytearray(insn)).zfill(width)

def get_current_pc(i):
    return i.address

def get_target_offset(current_pc, target):
    return (target - current_pc) / 4  # pc relative offset of target

def get_target_address(current_pc, offset):
    return (offset * 4) + current_pc # absolute address of pc relative offset

def long_to_bytes(value, width=8, endian='big'):
    s = binascii.unhexlify(('%%0%dx' % (width)) % ((value + (1 << width*4)) % (1 << width*4)))
    return s[::-1] if endian == 'little' else s

def bytes_to_long(data, endian='big'):
    data = data[::-1] if endian == 'little' else data

    if data[0] & 0x80 > 0:
        return -bytes_to_long(bytearray(~d % 256 for d in data)) - 1

    return int(str(data).encode('hex'), 16)

def get_branch_target(i):
    b = bitarray(endian="big")
    b.frombytes(str(i.bytes))

    return get_target_address(get_current_pc(i), bytes_to_long(bytearray(b[0:24].tobytes()), endian='little'))

def get_tb_target(i):
    b = bitarray(endian="big")
    addr = bitarray(endian="big")
    b.frombytes(str(i.bytes))
    x = b[0:3]
    y = b[8:16]
    z = b[21:24]
    if b[21] == True:
        addr = bitarray('11', endian='big') + z + y + x 
    else:
        addr = bitarray('00', endian='big') + z + y + x 
    newaddr = addr[8:16] + addr[0:8]
    offset = bytes_to_long(bytearray(newaddr.tobytes()), endian='little')
    return get_target_address(get_current_pc(i), offset)

def get_cond_branch_target(i):
    b = bitarray(endian="big")
    addr = bitarray(endian="big")
    b.frombytes(str(i.bytes))
    x = b[0:3]
    y = b[8:16]
    z = b[16:24]
    if b[23] == True:
        addr = bitarray('11111', endian='big') + z + y + x 
    else:
        addr = bitarray('00000', endian='big') + z + y + x 
    newaddr = addr[16:24] + addr[8:16] + addr[0:8]
    offset = bytes_to_long(bytearray(newaddr.tobytes()), endian='little')
    return get_target_address(get_current_pc(i), offset)

def get_branch_link_target(i):
    b = bitarray(endian="big")
    addr = bitarray(endian="big")
    b.frombytes(str(i.bytes))
    x = b[0:3]
    y = b[8:16]
    z = b[16:24]
    if b[23] == True:
        addr = bitarray('11111', endian='big') + z + y + x 
    else:
        addr = bitarray('00000', endian='big') + z + y + x 
    newaddr = addr[16:24] + addr[8:16] + addr[0:8]
    offset = bytes_to_long(bytearray(newaddr.tobytes()), endian='little')
    return get_target_address(get_current_pc(i), offset)

def handle_branch(inst, opts, trace, last_ne_flag):
    res = [False,0]
    next_branch = 'e'
    target_address = inst.operands[0].imm

    if 'b.' in inst.mnemonic:		# handle b.cond
        print("b.cond")
        print("operands[0].imm 0x%s" % to_x(target_address))
        print_insn_detail(inst)
        next_branch = trace.next_branch()

        flip_cond = True
        if 'eq' in inst.mnemonic and last_ne_flag:
            flip_cond = True
        elif 'eq' in inst.mnemonic and last_ne_flag == False:
            flip_cond = False 
        elif 'ne' in inst.mnemonic and last_ne_flag:
            flip_cond = False 
        elif 'ne' in inst.mnemonic and last_ne_flag == False:
            flip_cond = True
        elif 'lt' in inst.mnemonic and last_ne_flag == True:
            flip_cond = False
        elif 'lt' in inst.mnemonic and last_ne_flag == False:
            flip_cond = True
        elif 'gt' in inst.mnemonic and last_ne_flag == True:
            flip_cond = True
        elif 'gt' in inst.mnemonic and last_ne_flag == False:
            flip_cond = False 
        else:
            print ("can't decide!")

        if flip_cond:
            if next_branch == 'y':
                next_branch = 'n'
            elif next_branch == 'n':
                next_branch = 'y'

        if next_branch == 'y':
            res[0] = True
            res[1] = inst.operands[0].imm
        elif next_branch == 'n':
            pass
        elif next_branch == 'e':
            print ('[handle_branch]next_branch return error')
            pass
    else:
        print("b ")
        print("operands[0].imm 0x%s" % to_x(target_address))
        if target_address < opts.text_end:
            print_insn_detail(inst)
            res[0] = True
            res[1] = target_address
        else:
            print("[b][abnormal][outside of .fini]")
            print_insn_detail(inst)

    if target_address < opts.text_start:
        print(("[b][plt] %s b hex target addr :" + hex(target_address)) % hex(inst.address))

    return res

def handle_branch_with_link_reg(inst, opts):
    res = [False,0]
    print("===============[blr]==================")
    print_insn_detail(inst)

    return res
    
def handle_branch_with_reg(inst, opts):
    res = [False,0]
    print("===============[br]==================")
    print_insn_detail(inst)

    return res

def handle_ret(i, opts):
    res = [False,0]
    print("===============[ret]==================")
    print_insn_detail(i)

    return res

def handle_cond_branch(i, opts, trace, flip_cond):
    res = [False,0]
    next_branch = 'e'
    target_address = i.operands[0].imm
    next_branch = trace.next_branch()
    print_insn_detail(i)

    if flip_cond:
        if next_branch == 'y':
            next_branch = 'n'
        elif next_branch == 'n':
            next_branch = 'y'

    if next_branch == 'y':
        res[0] = True
        res[1] = i.operands[1].imm
    elif next_branch == 'n':
        pass
    elif next_branch == 'e':
        print ('[handle_branch]next_branch return error')
        pass

    return res

def handle_cbz(i, opts, trace, last_ne_flag):
    print("===============[cbz]==================")
    if last_ne_flag:
        return handle_cond_branch(i, opts, trace, True)
    else:
        return handle_cond_branch(i, opts, trace, False)

def handle_cbnz(i, opts, trace, last_ne_flag):
    print("===============[cbnz]==================")
    if last_ne_flag:
        return handle_cond_branch(i, opts, trace, False)
    else:
        return handle_cond_branch(i, opts, trace, True)

def handle_tbz(i, opts, trace, last_ne_flag):
    print("===============[tbz]==================")
    if last_ne_flag:
        return handle_cond_branch(i, opts, trace, True)
    else:
        return handle_cond_branch(i, opts, trace, False)

def handle_tbnz(i, opts, trace, last_ne_flag):
    print("===============[tbnz]==================")
    if last_ne_flag:
        return handle_cond_branch(i, opts, trace, False)
    else:
        return handle_cond_branch(i, opts, trace, True)
    
def is_attestation_start(i, opts):
    #print("operands[0].imm 0x%s" % to_x(i.operands[0].imm))
    #print("cfv_init: 0x%s" % hex(opts.cfv_init))
    return hex(i.operands[0].imm) == hex(opts.cfv_init)

def is_attestation_end(i, opts):
    #print("operands[0].imm 0x%s" % to_x(i.operands[0].imm))
    #print("cfv_quote: 0x%s" % hex(opts.cfv_quote))
    return hex(i.operands[0].imm) == hex(opts.cfv_quote)

def handle_branch_with_link(inst, opts):
    res = [False,0]

    target_addr = get_branch_target(inst)
    hex_addr = hex(target_addr)
    if (hex_addr == hex(opts.cfv_init) + 'L'):
        print("hit bl cfv_init")
    elif (hex_addr == hex(opts.cfv_quote) + 'L'):
        print("hit bl cfv_quote")
    else:
        if target_addr < opts.text_start:
            #print(("[plt] %s bl hex target addr :" + hex_addr) % hex(inst.address))
            print("bl plt, skipped library call")
            print_insn_detail(inst)
        else:
            print("bl func, internal call, take action")
            print_insn_detail(inst)

            res[0] = True
            res[1] = target_addr

    return res

if __name__ == "__main__":
    main()

