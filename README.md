[![Build Status](https://travis-ci.org/antipax/nifsy.svg?branch=master)](https://travis-ci.org/antipax/nifsy)

# Nifsy

A nifty NIF for the FS, providing faster filesystem operations.

Nifsy is available on [Hex](https://hex.pm/packages/nifsy).

Full documentation is available on [Hex](https://hexdocs.pm/nifsy/).

More developers are using Elixir for data processing, and the speed at which we read and write to files has lately become a concern. Due to the way FS interaction is currently handled in OTP (via a port driver), there is a considerable amount of overhead, which results in limited performance.

By implementing FS operations as NIFs (Native Implemented Functions), we can remove this overhead and achieve close-to-C levels of performance. Generally, Nifsy operations are 4 to **25 times** faster than their maximally-optimized equivalents in `File`/`:file`.

## Obligatory NIF warning

Nifsy is a NIF, which means that **if it crashes, the entire node will crash**. This single fact may eliminate it as a possibility for some applications. However, there is full test coverage for Nifsy, and due to its nature as a simple wrapper for the basic POSIX open/read/write/close syscalls, this is fairly unlikely.

You should also read the section below, since there are certain caveats to be aware of if you are running on a BEAM with/without dirty schedulers enabled.

## Nifsy on BEAM without Dirty Schedulers

If dirty schedulers are not enabled (and they probably are not, unless you have compiled OTP yourself with `./configure --enable-dirty-schedulers`), it is important to note that all Nifsy operations could block for an indefinite amount of time. Since the BEAM's scheduler expects all NIFs to complete in approximately 1ms, this means that the scheduler could become entirely blocked with FS ops, resulting in impacts to overall latency. Additionally, NIFs which take too much time to execute can result in "scheduler collapse," which is a serious condition in which the BEAM may stop executing code without crashing. Thus, **if you are going to use Nifsy on a BEAM without dirty schedulers, you must use the `+sfwi Interval` flag to set a scheduler forced wakeup interval.** Let me say that again:

### **If you are going to use Nifsy on a BEAM without dirty schedulers, you must use the `+sfwi Interval` flag to set a scheduler forced wakeup interval.**

The value is in milliseconds, and should be configured based on your application's needs. Lower values may result in increased overhead due to unnecessary scheduler wakeups, and a high value may result in increased time spent doing nothing. You can pass this to `elixir` and `iex` like `--erl "+sfwi 100"`.

Depending on your application/architecture, this may or may not be OK. For instance, in a data pipeline, you may have a node which downloads a file, reads it into memory, transforms it, and then loads it into a database. If the node does nothing else, the fact that Nifsy FS ops block is probably not a big deal, as long as the `+sfwi` flag is set.

## Nifsy on BEAM with Dirty Schedulers

If dirty schedulers are enabled (via `./configure --enable-dirty-schedulers`), there is no longer a problem as Nifsy FS ops will run on the dirty schedulers and thus not block the regular BEAM scheduler. However, using the dirty schedulers imposes a performance cost due to extra synchronization and copying. This may result in operations on small files taking longer with Nifsy than without, though on large files (depending on buffer size), Nifsy will still result in anywhere from 2 to 4 times better performance.

## Performance tips

Using the streaming API (`stream!`) incurs an overhead, so if you implement your application's functionality directly with `open`/`read`/`read_line`/`write`/`flush`/`close`, you may see an improvement in speed. However, you will lose the ability to automatically integrate with any stream-aware Elixir code.

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

## Development

Nifsy C code should be formatted using the `clang-format` tool. Nifsy C code uses the default code style rules in `clang-format`, so there is no need to specify any specific preset. If you wish to contribute C code to Nifsy, please ensure your code is formatted properly before opening a PR.
