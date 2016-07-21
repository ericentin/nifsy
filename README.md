# Nifsy

A nifty Elixir NIF for the FS.

## Benchmarking

You can use `mix run bench/<file>` to run a benchmark.

For read_line, `mix run bench/read_line_bench.exs`:

```
name                | total    | usec/iter | usec/line
1 KB (1 KB) - nifsy | 1021     | 20.42     | 0.454
1 KB (1 KB) - file  | 7946     | 158.92    | 3.532
1 KB (64KB) - nifsy | 1165     | 23.3      | 0.518
1 KB (64KB) - file  | 8514     | 170.28    | 3.784
1 MB (1 KB) - nifsy | 111505   | 2230.1    | 1.54
1 MB (1 KB) - file  | 1799176  | 35983.52  | 24.85
1 MB (64KB) - nifsy | 56804    | 1136.08   | 0.785
1 MB (64KB) - file  | 166133   | 3322.66   | 2.295
1 MB (1 MB) - nifsy | 125552   | 2511.04   | 1.734
1 MB (1 MB) - file  | 117589   | 2351.78   | 1.624
1 GB (1 KB) - nifsy | 11406610 | 1140661.0 | 24.615
1 GB (1 KB) - file  | 18497341 | 1849734.1 | 39.916
1 GB (64KB) - nifsy | 3723895  | 372389.5  | 8.036
1 GB (64KB) - file  | 11642739 | 1164273.9 | 25.124
1 GB (1 MB) - nifsy | 3974572  | 397457.2  | 8.577
1 GB (1 MB) - file  | 4834672  | 483467.2  | 10.433
1 GB (1 GB) - nifsy | 36739982 | 3673998.2 | 79.282
1 GB (1 GB) - file  | 24542805 | 2454280.5 | 52.961
```
