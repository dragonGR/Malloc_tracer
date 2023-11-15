#!/usr/bin/env ruby

require 'optparse'
require 'pp'
require_relative 'address_2_symbol'

mapfile = nil
target = nil

opt = OptionParser.new
opt.on('-m mapfile') { |v| mapfile = v }
opt.on('-t target') { |v| target = v }
opt.parse!

unless mapfile.nil? ^ target.nil?
  puts("Either mapfile or target must be given.")
  exit
end

a2s = File.foreach(mapfile) { |line| Address_2_Symbol.new line } unless mapfile.nil?

filename = ARGV[0]
puts "reading '#{filename}' ..."

allocation = Struct.new(:address, :size, :callstack)

arr = []

File.foreach(filename) do |line|
  type, address, size, *callstack = line.chomp.split(' ')

  case type
  when 'm'
    arr.push(allocation.new(address, size, callstack))
  when 'f'
    idx = arr.rindex { |alloc| alloc.address == address }
    next unless idx

    arr.delete_at idx
  else
    puts "Unknown operation type found: #{type}."
  end
end

exit puts "no memory leaks found." if arr.empty?

size = arr.size
puts "#{size} leaks found..."
puts

arr.each_index do |idx|
  puts "memory leak #{idx}:"

  alloc = arr[idx]
  puts "  address  : #{alloc.address}"
  puts "  size     : #{alloc.size.hex}"
  puts "  callstack:"

  alloc.callstack.each do |caller|
    if target
      output = IO.popen(["addr2line", "-Cife", target, caller]).read
      func, line = output.split
      puts "    #{caller} #{func} #{line}"
    else
      sym = a2s.translate caller.hex
      offset = sym.offset.nil? ? '?' : sym.offset.to_s(16)
      puts "    #{caller} #{sym.funcname}+0x#{offset} in #{sym.pathname}"
    end
  end

  puts
end