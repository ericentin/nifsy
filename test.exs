{:ok, handle} = IO.inspect Nifsy.open('mix.exs')
IO.inspect Nifsy.read(handle, 1_000, 1_000)
