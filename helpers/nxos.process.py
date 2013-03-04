# Archivist - network device config archiver
#
# this is a script for post-processing downloaded Cisco NX-OS config file
# in short - load, filter as you wish, and save to the same filename
#

import sys
import re

filtered_patterns = [
"^!Command",
"^!Time",
"show running-config",
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

  if re.search("^!\r$",line) and written_line < 5:	# skip heading exclamation marks
    remove = 1

  if(not remove):
    outfile.write("%s" % line)
    written_line += 1

outfile.write("\n"); 
outfile.close()

# end
