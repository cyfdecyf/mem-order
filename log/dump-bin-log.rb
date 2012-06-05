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

idx = 0
prev_id = -1
File.open(logfile) do |f|
  while true
    s = f.read(logsize * 4)
    break if s == nil
    unpacked = s.unpack("i" * logsize)
    break if unpacked[0] == -1
    if logfile == "memop" && unpacked[0] != prev_id
      idx = 0 
      prev_id = unpacked[0]
    end
    print "#{idx} #{unpacked.inspect}\n"
    idx += 1
  end
end