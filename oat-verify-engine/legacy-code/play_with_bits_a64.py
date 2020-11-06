#!/usr/bin/env python

import binascii
import math
import logging
from bitarray import bitarray
from capstone.arm64 import *
from capstone import *

def hexbytes(insn):
    width = int(pow(2, math.ceil(math.log(len(insn))/math.log(2))))
    return "0x" + binascii.hexlify(bytearray(insn)).zfill(width)

def get_current_pc(i):
    #return i.address + 4
    return i.address

def get_tbz_target(i):
    b = bitarray(endian="big")
    addr = bitarray(endian="big")
    b.frombytes(str(i.bytes))
    print "instruction bits",
    print b
    x = b[0:3]
    y = b[8:16]
    z = b[21:24]
    if b[21] == True:
        addr = bitarray('11', endian='big') + z + y + x 
    else:
        addr = bitarray('00', endian='big') + z + y + x 
    print 'addr:',
    newaddr = addr[8:16] + addr[0:8]
    offset = bytes_to_long(bytearray(newaddr.tobytes()), endian='little')
    print 'offset:',
    print offset
    #return get_target_address(get_current_pc(i), bytes_to_long(bytearray(b[0:24].tobytes()), endian='little'))
    return get_target_address(get_current_pc(i), offset)

def get_branch_target(i):
    b = bitarray(endian="big")
    addr = bitarray(endian="big")
    b.frombytes(str(i.bytes))
    print "instruction bits",
    print b
    x = b[0:3]
    y = b[8:16]
    z = b[16:24]
    if b[23] == True:
        addr = bitarray('11111', endian='big') + z + y + x 
    else:
        addr = bitarray('00000', endian='big') + z + y + x 
    print 'addr:',
    newaddr = addr[16:24] + addr[8:16] + addr[0:8]
    offset = bytes_to_long(bytearray(newaddr.tobytes()), endian='little')
    print 'offset:',
    print offset
    #return get_target_address(get_current_pc(i), bytes_to_long(bytearray(b[0:24].tobytes()), endian='little'))
    return get_target_address(get_current_pc(i), offset)

def get_target_offset(current_pc, target):
    #return (target - current_pc) / 4 - 1  # pc relative offset of target
    return (target - current_pc) / 4  # pc relative offset of target

def get_target_address(current_pc, offset):
    #return (offset * 4) + current_pc + 4  # absolute address of pc relative offset
    return (offset * 4) + current_pc # absolute address of pc relative offset

def long_to_bytes(value, width=8, endian='big'):
    s = binascii.unhexlify(('%%0%dx' % (width)) % ((value + (1 << width*4)) % (1 << width*4)))
    return s[::-1] if endian == 'little' else s

def bytes_to_long(data, endian='big'):
    data = data[::-1] if endian == 'little' else data

    if data[0] & 0x80 > 0:
        return -bytes_to_long(bytearray(~d % 256 for d in data)) - 1

    return int(str(data).encode('hex'), 16)

def disasm_single(word, address):
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    md.detail = True

    for i in md.disasm(str(word), address):
        return i

def rewrite_branch(i, target):
    word = new_branch_with_link_instruction(i, target)

    if 'b.' not in i.mnemonic:
        print "non-condition branch"
    else:
        print "conditional branch"

    j = disasm_single(word, i.address)

    logging.info("b     at 0x%08x: hooking %-10s\t%s\t%s -> %-10s\t%s\t%s" %
            (i.address,
                hexbytes(i.bytes), i.mnemonic, i.op_str,
                hexbytes(j.bytes), j.mnemonic, j.op_str))

    logging.info("b.cond at 0x%08x: target: 0x%08x" % (i.address, get_tbz_target(i)))
    return

def new_branch_with_link_instruction(i, target):
    bits = bitarray('0'*32, endian='big')
    addr = bitarray('0'*32, endian='big')

    bits[24:30] = bitarray('100101')  # opcode for BL
    addr[00:32] = bytes_to_bits(long_to_bytes(
        get_target_offset(get_current_pc(i), target),
        width=8, endian='little'))
    bits[00:24] = addr[00:24]
    bits[30:32] = addr[24:26] 

    print 'bl bits:',
    print bits

    print 'bytes:', 
    print len(bits.tobytes())
    print len(bytearray(bits.tobytes()))
    for b in bytearray(bits.tobytes()):
        print hex(b)

    return bytearray(bits.tobytes())

def bytes_to_bits(data_bytes):
    bits = bitarray(endian='big')
    bits.frombytes(str(data_bytes))
    return bits

if __name__ == "__main__":
        logging.basicConfig(format='%(message)s',level=logging.DEBUG)
	addr = 0x4001a0
	target = 0x400ec0
	#inst = '\x4c\x00\x00\x94'
	#inst = '\xcf\xff\xff\x97'
	#inst = '\x41\x00\x00\x54'
	#inst = '\xfd\x7b\xc1\xa8'
	#inst = '\xe0\x0f\x40\xb9'
	#inst = '\xad\xfc\xff\x54'
	#inst = '\x61\x00\x00\xb5'
	inst = '\x72\x63\x68\x36'
	i = disasm_single(inst, addr)

        # pop frame pointer and return address to link register off stack; ldp x29, x30, [sp], #16/32/64
        if (i.id == ARM64_INS_LDP and len(i.operands) == 4
                and i.operands[0].reg == ARM64_REG_X29
                and i.operands[1].reg == ARM64_REG_X30
                and i.operands[2].type == ARM64_OP_MEM and i.operands[2].value.mem.base == 4
                and i.operands[3].type == ARM64_OP_IMM and i.operands[3].value.imm == 16):

            print i.operands[2].type == ARM64_OP_MEM
            print i.operands[2].value.mem.base
            print i.operands[3].type == ARM64_OP_IMM
            print i.operands[3].value.imm
            print i.reg_name(0)
            print i.reg_name(1)
            print i.reg_name(2)
            print i.reg_name(3)
            print i.reg_name(4)
            print i.reg_name(5)
            print i.reg_name(6)
            print i.reg_name(7)

            print "match ldp !"
        elif (i.id == ARM64_INS_CBNZ and len(i.operands) == 2
                and i.operands[0].type == ARM64_OP_REG and i.operands[0].reg == ARM64_REG_X1
                and i.operands[1].type == ARM64_OP_IMM): 
            print "match cbnz!"
            print "i.operands[1].value.imm: 0x%08x" % i.operands[1].value.imm
        elif (i.id == ARM64_INS_TBZ and len(i.operands) == 3
                and i.operands[0].type == ARM64_OP_REG and i.operands[0].reg == ARM64_REG_W18
                and i.operands[1].type == ARM64_OP_IMM and i.operands[1].value.imm == 13
                and i.operands[2].type == ARM64_OP_IMM): 
            print "match tbz!"
            print "i.operands[2].value.imm: 0x%08x" % i.operands[2].value.imm
            print "test get_tbz_target():"
            print hex(get_tbz_target(i))
        else:
            print 'not match!'

        if i.mnemonic == "b.le":
            print "i == b.le " + i.mnemonic
        else:
            print "i != b.le " + i.mnemonic
	rewrite_branch(i, target)
