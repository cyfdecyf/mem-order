#!/usr/bin/env ruby

# For dumping memop log to work, it must contain object id information.

SIZE_TBL = {
  "memop" => ["iic", 2 * 4 + 1],
  "memop-index" => ["ii", 2 * 4],
}

def getsize(filename)
  return SIZE_TBL[filename] if SIZE_TBL.has_key? filename
  if filename.start_with? "version"
    ["ii", 8]
  elsif filename.start_with? "memop"
    ["cii", 9]
  end
end

logfile = ARGV[0]
packstr, entrysize = getsize(logfile)

idx = 0
File.open(logfile) do |f|
  while true
    s = f.read(entrysize)
    break if s == nil
    unpacked = s.unpack(packstr)
    break if unpacked[0] == -1
    print "#{idx} #{unpacked.inspect}\n"
    idx += 1
  end
end
