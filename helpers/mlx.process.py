# Archivist - network device config archiver
#
# this is a script for post-processing downloaded Brocade MLX config file
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
"^ntp clock-period",              # clock-period is skipped
"^([^#]+#)",                      # "enabled" prompt of IOS device (stolen from rancid)
"write term"
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

  if re.search("^!\r$",line) and written_line < 3:      # skip heading exclamation marks
    remove = 1

  if(not remove):
    outfile.write("%s" % line)
    written_line += 1
 
outfile.close()

# end
