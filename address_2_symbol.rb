#!/usr/bin/env ruby

class Address_2_Symbol
  def initialize(mapfile)
    @maps = Hash.new
    @symbols = Hash.new

    mapfile.each_line do |line|
      range, perm, offset, dev, inode, pathname = line.split
      if perm.include? 'x'
        first, last = range.split '-'
        @maps[Range.new(first.hex, last.hex)] = pathname
      end
    end
  end

  def translate(addr)
    if addr.respond_to?(:map)
      addr.map { |e| translate_one e }
    else
      translate_one addr
    end
  end

  private

  def base_address_of_symbol_of(pathname)
    readelf_l = IO.popen(["readelf", "-Wl", pathname]).read

    base = 0
    readelf_l.each_line do |line|
      type, offset, virtaddr, physaddr, filesiz, memsize, rest = line.chomp.split(nil, 7)
      next unless type == 'LOAD'

      flg, _, align = rest.rpartition(' ')
      next unless flg.include? 'E'

      base = virtaddr.hex - (virtaddr.hex % align.hex)
    end

    base
  end

  def symbol_table_of(pathname)
    return @symbols[pathname] if @symbols.key? pathname

    base = base_address_of_symbol_of(pathname)

    entry = Struct.new :address, :name

    symlist = IO.popen(["objdump", "-d", "-j", ".text", pathname, "|", "grep", "-E", "^[0-9a-f]+\\s+.+:$"]).read
    @symbols[pathname] = symlist.each_line.inject([]) do |tbl, line|
      address, name_with_bracket = line.chomp.split
      md = /\<(.+)\>/.match(name_with_bracket)
      name = md[1]

      tbl.push entry.new(address.hex - base, name)
    end
  end

  def translate_one(addr)
    @maps.each do |range, pathname|
      if range.include? addr
        offset = addr - range.first

        ret_type = Struct.new :funcname, :offset, :pathname
        s = ret_type.new('?', nil, pathname)

        symbol = symbol_table_of(pathname)
        symbol.each_index do |idx|
          entry = symbol[idx]
          if entry.address < offset and
              (symbol[idx + 1].nil? or symbol[idx + 1].address > offset)
            s.funcname = entry.name
            s.offset = offset - entry.address
            break
          end
        end
        return s
      end
    end
  end
end
