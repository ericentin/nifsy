defmodule Nifsy.ReadLineBench do
  @test_files [
    {"tmp/kilobyte.txt", 1024},
    {"tmp/megabyte.txt", 1024 * 1024},
    {"tmp/gigabyte.txt", 1024 * 1024 * 1024}
  ]

  @benchmarks [
    {"1 KB (16 B) - nifsy", :nifsy_bench, [50, "tmp/kilobyte.txt", 16,                 45]},
    {"1 KB (16 B) - file",  :file_bench,  [50, 'tmp/kilobyte.txt', 16,                 45]},
    {"1 KB (1 KB) - nifsy", :nifsy_bench, [50, "tmp/kilobyte.txt", 1024,               45]},
    {"1 KB (1 KB) - file",  :file_bench,  [50, 'tmp/kilobyte.txt', 1024,               45]},
    {"1 KB (64KB) - nifsy", :nifsy_bench, [50, "tmp/kilobyte.txt", 65536,              45]},
    {"1 KB (64KB) - file",  :file_bench,  [50, 'tmp/kilobyte.txt', 65536,              45]},
    {"1 MB (1 KB) - nifsy", :nifsy_bench, [50, "tmp/megabyte.txt", 1024,               1448]},
    {"1 MB (1 KB) - file",  :file_bench,  [50, 'tmp/megabyte.txt', 1024,               1448]},
    {"1 MB (64KB) - nifsy", :nifsy_bench, [50, "tmp/megabyte.txt", 65536,              1448]},
    {"1 MB (64KB) - file",  :file_bench,  [50, 'tmp/megabyte.txt', 65536,              1448]},
    {"1 MB (1 MB) - nifsy", :nifsy_bench, [50, "tmp/megabyte.txt", 1024 * 1024,        1448]},
    {"1 MB (1 MB) - file",  :file_bench,  [50, 'tmp/megabyte.txt', 1024 * 1024,        1448]},
    {"1 GB (1 KB) - nifsy", :nifsy_bench, [10, "tmp/gigabyte.txt", 1024,               46341]},
    {"1 GB (1 KB) - file",  :file_bench,  [10, 'tmp/gigabyte.txt', 1024,               46341]},
    {"1 GB (64KB) - nifsy", :nifsy_bench, [10, "tmp/gigabyte.txt", 65536,              46341]},
    {"1 GB (64KB) - file",  :file_bench,  [10, 'tmp/gigabyte.txt', 65536,              46341]},
    {"1 GB (1 MB) - nifsy", :nifsy_bench, [10, "tmp/gigabyte.txt", 1024 * 1024,        46341]},
    {"1 GB (1 MB) - file",  :file_bench,  [10, 'tmp/gigabyte.txt', 1024 * 1024,        46341]},
    {"1 GB (1 GB) - nifsy", :nifsy_bench, [10, "tmp/gigabyte.txt", 1024 * 1024 * 1024, 46341]},
    {"1 GB (1 GB) - file",  :file_bench,  [10, 'tmp/gigabyte.txt', 1024 * 1024 * 1024, 46341]}
  ]

  def benchmark() do
    if "perf" in System.argv do
      :erlang.system_monitor(self(), [
        :busy_port,
        :busy_dist_port,
        long_gc: 10,
        large_heap: 1024 * 1024,
        long_schedule: 10
      ])
    end
    Enum.each(@test_files, &maybe_generate_file_with_lines/1)
    :io.format(
      '| ~-19s | ~10s | ~12s | ~10s |\n' ++
      '| ~-19c | ~9c: | ~11c: | ~9c: |\n',
      [ "name", "total", "usec/iter", "usec/line", ?-, ?-, ?-, ?- ])
    pids = Enum.reduce @benchmarks, %{}, fn {name, bench, args}, acc ->
      if ("nifsy" in System.argv and bench == :nifsy_bench) or not "nifsy" in System.argv do
        task = Task.async(__MODULE__, bench, args)
        {u_sec, avg_u_sec, u_sec_per_line} = task |> Task.await(60_000)
        :io.format('| ~-19s | ~10w | ~12.2f | ~10.3f |\n', [
          name,
          u_sec,
          Float.round(avg_u_sec, 2),
          Float.round(u_sec_per_line, 3)
        ])
        Map.put(acc, task.pid, name)
      else
        acc
      end
    end
    if "perf" in System.argv do
      :io.format(
        '\n' ++
        '| ~-19s | ~16s | ~12s | ~-10s |\n' ++
        '| ~19c | ~15c: | ~11c: | ~10c |\n',
        [ "name", "metric", "total/freq", "report", ?-, ?-, ?-, ?- ])
      flush_system_monitor(pids, %{})
    end
  end

  defp benchmark(n, function, arguments, cleanup) do
    acc_u_sec = bench_loop(n, 0, function, arguments, cleanup)
    {acc_u_sec, acc_u_sec / n}
  end

  defp bench_loop(0, acc_u_sec, _function, _arguments, _cleanup) do
    acc_u_sec
  end
  defp bench_loop(i, acc_u_sec, function, arguments, cleanup) do
    args = arguments.()
    {u_sec, _} = :timer.tc(function, args)
    cleanup.(args)
    bench_loop(i - 1, acc_u_sec + u_sec, function, arguments, cleanup)
  end

  def nifsy_bench(n, filename, read_ahead_bytes, num_lines) do
    read_line_loop =
      fn (f, file_desc, size) ->
        case Nifsy.read_line(file_desc) do
          {:ok, :eof} -> size
          {:ok, line} -> f.(f, file_desc, size + byte_size(line) + 1)
        end
      end

    function =
      fn (file_desc) ->
        read_line_loop.(read_line_loop, file_desc, 0)
      end

    arguments =
      fn () ->
        {:ok, file_desc} = Nifsy.open(filename, :read, [buffer_bytes: read_ahead_bytes])
        [file_desc]
      end

    cleanup =
      fn ([file_desc]) ->
        Nifsy.close(file_desc)
      end

    {acc_u_sec, avg_u_sec} = benchmark(n, function, arguments, cleanup)
    {acc_u_sec, avg_u_sec, avg_u_sec / num_lines}
  end

  def file_bench(n, filename, read_ahead_bytes, num_lines) do
    read_line_loop =
      fn (f, io_device, size) ->
        case :file.read_line(io_device) do
          :eof -> size
          {:ok, line} -> f.(f, io_device, size + byte_size(line))
        end
      end

    function =
      fn (io_device) ->
        read_line_loop.(read_line_loop, io_device, 0)
      end

    arguments =
      fn () ->
        {:ok, io_device} = :file.open(filename, [:binary, :raw, :read, {:read_ahead, read_ahead_bytes}])
        [io_device]
      end

    cleanup =
      fn ([io_device]) ->
        :file.close(io_device)
      end

    {acc_u_sec, avg_u_sec} = benchmark(n, function, arguments, cleanup)
    {acc_u_sec, avg_u_sec, avg_u_sec / num_lines}
  end

  defp flush_system_monitor(pids, stats) do
    receive do
      {:monitor, pid_or_port, metric, report} ->
        increment = &(&1 + 1)
        update = &(Map.update(&1, report, 1, increment))
        case Map.fetch(pids, pid_or_port) do
          {:ok, name} ->
            stats = Map.update(stats, {name, metric}, %{ report => 1 }, update)
            flush_system_monitor(pids, stats)
          :error ->
            stats = Map.update(stats, {inspect(pid_or_port), metric}, %{ report => 1 }, update)
            flush_system_monitor(pids, stats)
        end
    after
      # Give the system monitor time to finish sending any reports.
      1000 ->
        sorted_stats =
          stats
          |> Enum.sort_by(&(&1 |> elem(1) |> Map.values() |> Enum.sum()))
          |> Enum.reverse()
        Enum.each sorted_stats, fn {{name, metric}, reports} ->
          :io.format('| ~-19s | ~16s | ~-12s | ~-10s |\n', [
            name,
            to_string(metric),
            reports |> Map.values() |> Enum.sum() |> to_string(),
            ""
          ])
          sorted_reports =
            reports
            |> Enum.sort_by(&(elem(&1, 1)))
            |> Enum.reverse()
          Enum.each sorted_reports, fn {report, freq} ->
            :io.format('| ~-19s | ~16s | ~12s | `~s` |\n', [
              "",
              "",
              to_string(freq),
              inspect(report)
            ])
          end
        end
        :ok
    end
  end

  defp generate_file_with_lines(filename, file_size) do
    :ok = :filelib.ensure_dir(filename)
    {:ok, io_device} = :file.open(filename, [:binary, :raw, :write])

    write_loop =
      fn (f, b, s) ->
        if s + byte_size(b) >= file_size do
          b = :binary.part(b, 0, file_size - s - 1)
          :file.write(io_device, b <> "\n")
          s + byte_size(b) + 1
        else
          :file.write(io_device, b <> "\n")
          f.(f, b <> "0", s + byte_size(b) + 1)
        end
      end

    write_loop.(write_loop, "", 0)
    :file.close(io_device)
  end

  defp maybe_generate_file_with_lines({filename, file_size}) do
    case :filelib.file_size(filename) do
      ^file_size ->
        :ok
      _ ->
        IO.puts "Generating #{filename} of size #{file_size}..."
        generate_file_with_lines(filename, file_size)
    end
  end
end

Nifsy.ReadLineBench.benchmark()
