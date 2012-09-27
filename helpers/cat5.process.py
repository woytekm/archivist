# Archivist - network device config archiver
#
# this is a script for post-processing downloaded Catalyst OS config file
# in short - load, filter as you wish, and save to the same filename
#

import sys
import re

filtered_patterns = [
"^\.+",                      # dots displayed while building config
"\(enable\)\s+$",            # "enabled" prompt of CatOS device
"^#time",                    # when config output was generated
"^TFTP session in progress", # some tftp upload message
"^[1-2][0-9]{3}",            # if it starts with a year - skip it (can be some log messages)
"show config all"            # show config command
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

  if(not remove):
    outfile.write("%s" % line)
    written_line += 1
 
outfile.close()

# end
