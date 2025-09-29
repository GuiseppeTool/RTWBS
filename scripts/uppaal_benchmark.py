import subprocess
import os 
import time
import datetime
import psutil


eval_path = "assets/eval"
results_path = "results/uppaal"
timeout_h = 10  # hours

def sort_key(x):
        fname = os.path.basename(x).replace('.xml', '')
        parts = fname.split('_')
        prefix_order = {'s': 0, 'm': 1, 'l': 2, 'xl': 3}
        prefix = parts[0].lower()
        num = int(parts[1])
        return (num, prefix_order.get(prefix, 99))

def syn_bench_uppaal(benchmarks, timeout_h, csv_file):
    results = {}
    
    for b in benchmarks:
        print("current benchmark:", b)
        #if b in ["assets/eval/s_1.xml", "assets/eval/m_1.xml", "assets/eval/l_1.xml", "assets/eval/xl_1.xml", "assets/eval/s_3.xml"]:
        #    print("Skipping benchmark:", b)
        #    results[b] = (0, 0)
        #    continue
        model_path = b
        start_time = time.perf_counter()
        mem_usage = 0
        peak_mem = 0
        cmd = [os.environ["VERIFYTA_PATH"], model_path]
        try:
            proc = subprocess.Popen(
                ["/usr/bin/time", "-v"] + cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                    text=True,
                #timeout=timeout_h * 3600
            )
            p = psutil.Process(proc.pid)
            timed_out = False
            end_time = 0
            last_triggered = 0 
            print("Running command:", ' '.join(cmd))
            while True:
                if proc.poll() is not None:
                    break
                try:
                    mem = p.memory_info().rss
                    if mem > peak_mem:
                        peak_mem = mem
                except psutil.NoSuchProcess:
                    print("Process ended before memory check.")
                    break
                end_time = time.perf_counter()
                if (end_time - start_time) > timeout_h * 3600:
                    print(f"Killing process after {timeout_h} hours timeout.")
                    try:
                        parent = psutil.Process(proc.pid)
                        for child in parent.children(recursive=True):
                            child.kill()
                        parent.kill()
                    except psutil.NoSuchProcess:
                        pass
                    timed_out = True
                    break

                interval = int((end_time - start_time) // 1)  # 5-second buckets
                if interval > last_triggered:
                    last_triggered = interval
                    print(f"[verifyta] still running... elapsed time: {int(end_time - start_time)} seconds")
                    time_left = (timeout_h * 3600) - (end_time - start_time)
                    print(f"[verifyta] time left before killing: {int(time_left)} seconds")
                time.sleep(0.1)


            
            elapsed_time = end_time - start_time
            mem_usage = peak_mem // 1024  # KB

            stdout, stderr = proc.communicate()
            elapsed_time = time.perf_counter() - start_time

            # Parse memory from time -v output (if available)
            for line in stderr.splitlines():
                if "Maximum resident set size" in line:
                    peak_memory = int(line.split(":")[1].strip())   # MB

            if timed_out:
                print(f"verifyta timed out after {timeout_h} hours on {b}")

            print(peak_memory)
        except Exception as e:
            print(f"Error running verifyta on {b}: {e}")
            end_time = time.perf_counter()
            elapsed_time = end_time - start_time
            #mem_usage = peak_mem // 1024 if 'peak_mem' in locals() else None
            for line in stderr.splitlines():
                if "Maximum resident set size" in line:
                    peak_memory = int(line.split(":")[1].strip())   # MB
            print(peak_memory)
        results[b] = (elapsed_time, mem_usage)
        with open(csv_file, "a") as f:
            f.write(f"{b},{elapsed_time*1000},{peak_memory if peak_memory is not None else 'N/A'}\n")
        print(f"Benchmark: {b}, Time: {elapsed_time:.2f}s, Memory: {mem_usage} KB")
    return results


if __name__ == "__main__":



    # Save results in CSV
    now = datetime.datetime.now()
    date_time = now.strftime("%Y-%m-%d_%H-%M-%S")
    
    if not os.path.exists(results_path):
        os.makedirs(results_path)

    csv_file = os.path.join(results_path, f"results_{date_time}.csv")
    with open(csv_file, "w") as f:
        f.write("Benchmark,Time(ms),Memory(KB)\n")

    benchmarks = [os.path.join(eval_path, f) for f in os.listdir(eval_path)]
    benchmarks.sort(key=sort_key)
    results = syn_bench_uppaal(benchmarks, timeout_h, csv_file)

    print(f"\nResults saved to {csv_file}")
