#!/usr/bin/env ruby

SIZE_TBL = {
  "memop" => 4,
  "memop-index" => 2,
}

def getsize(filename)
  if filename.start_with? "version"
    return 2
  end
  size = SIZE_TBL[filename]
  size = 3 unless size
  size
end

logfile = ARGV[0]
logsize = getsize(logfile)

File.open(logfile) do |f|
  while true
    s = f.read(logsize * 4)
    break if s == nil
    unpacked = s.unpack("i" * logsize)
    break if unpacked[0] == -1
    p unpacked
  end
end