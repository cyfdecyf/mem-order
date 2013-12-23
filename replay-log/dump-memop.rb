#!/usr/bin/env ruby

packstr = "qqq"
entsize = 3*8

idx = 0
while true
  s = ARGF.read(entsize)
  break if s == nil
  unpacked = s.unpack(packstr)
  break if unpacked[0] == -1
  printf "%3d: B%4d @%4d %4d\n", idx, unpacked[0], unpacked[1]
  idx += 1
end

