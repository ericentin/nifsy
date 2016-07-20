{:ok, handle} = IO.inspect Nifsy.open('mix.exs')
Enum.each 1..35, fn _ ->
  IO.inspect Nifsy.read_line(handle, 4_096)
end
