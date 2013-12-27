#!/usr/bin/env ruby

# Corresponds to struct wait_memop in mem.h.
# The 2nd 32-bit int is padding added by compiler.
packstr = "llqq"
entsize = 3*8

idx = 0
while true
  s = ARGF.read(entsize)
  break if s == nil
  unpacked = s.unpack(packstr)
  break if unpacked[0] == -1
  printf "%3d: B%4d @%4d R%4d\n", idx, unpacked[0], unpacked[2], unpacked[3]
  idx += 1
end

