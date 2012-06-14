#!/usr/bin/env luajit

function process_log()
  local warlog = {}
  local line

  -- read log
  while true do
    local objid, version, memop, tid = io.read("*number", "*number", "*number", "*number")
    if not objid then break end
    warlog[#warlog + 1] = {objid, version, memop, tid}
  end

  table.sort(warlog, function (a, b) 
    if a[1] == b[1] then return a[2] < b[2] end
    return a[1] < b[1]
  end)

  -- write out result
  for k, v in ipairs(warlog) do
    io.write(table.concat(v, " "))
    io.write("\n")
  end
end

process_log()

