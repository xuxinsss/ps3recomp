"""Prepare locally supplied Cg shaders with independent bounded workers.

Each worker uses the runtime's exact-source cache and atomic cache writes.
No shader inputs are downloaded, shipped or uploaded. Worker logs remain local.
"""
import argparse
import json
import subprocess
import time
from pathlib import Path

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('tool', type=Path)
    parser.add_argument('input', type=Path)
    parser.add_argument('cache', type=Path)
    parser.add_argument('logs', type=Path)
    parser.add_argument('--jobs', type=int, default=4, choices=range(1, 9))
    args = parser.parse_args()
    args.cache.mkdir(parents=True, exist_ok=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    workers, streams = [], []
    start = time.perf_counter()
    try:
        for index in range(args.jobs):
            log = args.logs/f'worker-{index}.log'
            stream = log.open('wb')
            streams.append(stream)
            process = subprocess.Popen([
                str(args.tool.resolve()), str(args.input.resolve()),
                str(args.cache.resolve()), str(index), str(args.jobs)
            ], stdout=stream, stderr=subprocess.STDOUT, startupinfo=startup)
            workers.append((process, log))
        last = {}
        while True:
            running = False
            for process, log in workers:
                running |= process.poll() is None
                lines = log.read_text(errors='replace').splitlines()
                if lines and last.get(log) != lines[-1]:
                    print(lines[-1], flush=True)
                    last[log] = lines[-1]
            if not running:
                break
            time.sleep(2)
        result = dict(seconds=time.perf_counter()-start,
                      returncodes=[p.returncode for p, _ in workers])
        (args.logs/'result.json').write_text(json.dumps(result, indent=2))
        print('[precompile]', json.dumps(result), flush=True)
        return 0 if all(p.returncode == 0 for p, _ in workers) else 1
    finally:
        for process, _ in workers:
            if process.poll() is None:
                process.terminate()
                process.wait()
        for stream in streams:
            stream.close()

if __name__ == '__main__':
    raise SystemExit(main())
