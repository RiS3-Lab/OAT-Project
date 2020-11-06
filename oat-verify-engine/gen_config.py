#!/usr/bin/env python

import argparse
import sys
import re
import os

DEFAULT_OUTPUT_CONFIG = 'output.cfg'

CONFIG_DEFAULTS = {
        'load_address'   : '0x400000',
        'text_start'     : '0x400430',
        'text_end'       : '0x400818',
        'omit_addresses' : '0x000000',
        'cfv_init'       : '0x000000',
        'cfv_quote'      : '0x000000',
}

def extract_text_start_and_end_from_ph(phfile):
	text_start = ''
	text_end = ''

	text_start_re = re.compile('^\s+\[[\s\d]+\]\s\.text\s+PROGBITS\s+0000000000([0-9a-f]{6})\s+[0-9a-f]+$')
	text_end_re = re.compile('^\s+\[[\s\d]+\]\s\.fini\s+PROGBITS\s+0000000000([0-9a-f]{6})\s+[0-9a-f]+$')

	ph = open(phfile, 'r')
	lines = ph.readlines()
	for line in lines:
		res = text_start_re.match(line)
		if res != None:
			text_start = '0x' + res.group(1)

		res = text_end_re.match(line)
		if res != None:
			text_end = '0x' + res.group(1)
	return text_start, text_end

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Extract Trampoline Function Address From Objdump File')
	parser.add_argument('file', nargs='?', metavar='FILE',
		help='objdump file to extract')
	parser.add_argument('--program-header', dest='ph', default=None,
		help='program header file to extract text start and text end')
	parser.add_argument('-L', '--load-address', dest='load_address', default=None,
		help='load address of binary image')
	parser.add_argument('--text-start', dest='text_start', default=None,
		help='start address of section to instrument')
	parser.add_argument('--text-end', dest='text_end', default=None,
		help='end address of section to instrument')
	parser.add_argument('-o', '--outfile', dest='outfile', default=None,
		help='outfile for branch table')

	args = parser.parse_args()

	if args.file is None:
		sys.exit("%s: lack of input file!" % (sys.argv[0]))
	elif not os.path.isfile(args.file):
		sys.exit("%s: file '%s' not found" % (sys.argv[0], args.file))

	dumpfile = args.file
	lines = open(dumpfile, 'r').readlines()

	outfile = args.outfile if args.outfile != None else DEFAULT_OUTPUT_CONFIG
	ofd = open(outfile, 'w')

	phfile = args.ph if args.ph != None else None
	if phfile is not None:
		text_start, text_end = extract_text_start_and_end_from_ph(phfile)
		CONFIG_DEFAULTS['text_start'] = text_start
		CONFIG_DEFAULTS['text_end'] = text_end

	if args.load_address is not None:
		CONFIG_DEFAULTS['load_address'] = args.load_address
	if args.text_start is not None:
		CONFIG_DEFAULTS['text_start'] = args.text_start
	if args.text_end is not None:
		CONFIG_DEFAULTS['text_end'] = args.text_end

	bl_cfa_init = re.compile('^\s+[0-9a-f]{6}\:\s+[0-9a-f]+\s+bl\s+([0-9a-f]{6})\s+\<cfv_init\@plt\>\s?$')
	bl_cfa_quote = re.compile('^\s+[0-9a-f]{6}\:\s+[0-9a-f]+\s+bl\s+([0-9a-f]{6})\s+\<cfv_quote\@plt\>\s?$')
	omit_addresses = '0x000000'

	for line in lines:
		res = bl_cfa_init.match(line)
		if res != None:
			CONFIG_DEFAULTS['cfv_init'] = '0x' + res.group(1)

		res = bl_cfa_quote.match(line)
		if res != None:
			CONFIG_DEFAULTS['cfv_quote'] = '0x' + res.group(1)

	print 'omit_addresses:' + omit_addresses
	CONFIG_DEFAULTS['omit_addresses'] = omit_addresses

	ofd.write('[code-addresses]\n')
	ofd.write('load_address   = %s\n' % CONFIG_DEFAULTS['load_address'])
	ofd.write('text_start     = %s\n' % CONFIG_DEFAULTS['text_start'])
	ofd.write('text_end       = %s\n' % CONFIG_DEFAULTS['text_end'])
	ofd.write('cfv_init       = %s\n' % CONFIG_DEFAULTS['cfv_init'])
	ofd.write('cfv_quote      = %s\n' % CONFIG_DEFAULTS['cfv_quote'])
	ofd.write('omit_addresses = %s\n' % CONFIG_DEFAULTS['omit_addresses'])
	ofd.write('\n')

	ofd.close()
