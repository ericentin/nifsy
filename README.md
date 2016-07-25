[![Build Status](https://travis-ci.org/antipax/nifsy.svg?branch=master)](https://travis-ci.org/antipax/nifsy)

# Nifsy

A nifty Elixir NIF for the FS.

## Benchmarking

You can use `mix run bench/<file>` to run a benchmark.

For read_line, `mix run bench/read_line_bench.exs`:

| name                |      total |    usec/iter |  usec/line |
| ------------------- | ---------: | -----------: | ---------: |
| 1 KB (16 B) - nifsy |       3246 |        64.92 |      1.443 |
| 1 KB (16 B) - file  |      13248 |       264.96 |      5.888 |
| 1 KB (1 KB) - nifsy |       2291 |        45.82 |      1.018 |
| 1 KB (1 KB) - file  |       6879 |       137.58 |      3.057 |
| 1 KB (64KB) - nifsy |       1106 |        22.12 |      0.492 |
| 1 KB (64KB) - file  |       9543 |       190.86 |      4.241 |
| 1 MB (1 KB) - nifsy |      83880 |      1677.60 |      1.159 |
| 1 MB (1 KB) - file  |    1598955 |     31979.10 |     22.085 |
| 1 MB (64KB) - nifsy |      39978 |       799.56 |      0.552 |
| 1 MB (64KB) - file  |     145726 |      2914.52 |      2.013 |
| 1 MB (1 MB) - nifsy |      71193 |      1423.86 |      0.983 |
| 1 MB (1 MB) - file  |     104737 |      2094.74 |      1.447 |
| 1 GB (1 KB) - nifsy |   14401603 |   1440160.30 |     31.077 |
| 1 GB (1 KB) - file  |   18981727 |   1898172.70 |     40.961 |
| 1 GB (64KB) - nifsy |    3887583 |    388758.30 |      8.389 |
| 1 GB (64KB) - file  |   11666435 |   1166643.50 |     25.175 |
| 1 GB (1 MB) - nifsy |    4036145 |    403614.50 |      8.710 |
| 1 GB (1 MB) - file  |    4832690 |    483269.00 |     10.429 |
| 1 GB (1 GB) - nifsy |   36451711 |   3645171.10 |     78.660 |
| 1 GB (1 GB) - file  |   26565286 |   2656528.60 |     57.326 |
