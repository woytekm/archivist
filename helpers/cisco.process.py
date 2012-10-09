# Archivist - network device config archiver
#
# this is a script for post-processing downloaded cisco IOS config file
# in short - load, filter as you wish, and save to the same filename
#

import sys
import re

filtered_patterns = [
"Current configuration",	
"Last configuration",
"NVRAM config last",
"Building configuration",
"Generating configuration",
"No configuration change since",
"Non-Volatile memory is in use",
"Error on initialize VLAN database",
"Cryptochecksum",
"write term",
"^$",			# empty lines
"^\r$",			# empty lines
"\[OK\]",		# OK displayed by FWSM
"^ntp clock-period",
"^[^#]+#"		# enabled prompt of the cisco router (stolen from Rancid)
]

infile = open(sys.argv[1], "r")
in_lines = infile.readlines()
infile.close()

outfile = open(sys.argv[1], "w")

written_line = 0
 
for line in in_lines:

  remove = 0

  for pattern in filtered_patterns:
    if re.search(pattern,line):				# scan through unwanted pattern list
      remove = 1

  if re.search("^!\r$",line) and written_line < 3:	# skip heading exclamation marks
    remove = 1

  if(not remove):
    outfile.write("%s" % line)
    written_line += 1

outfile.write("\n"); 
outfile.close()

# end
