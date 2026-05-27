import argparse
import random
import shlex
import subprocess
import time
import os
import signal

AMMUNITION = [
    "http://localhost:8080/api/v1/maps",
    "http://localhost:8080/api/v1/maps/map1",
    "http://localhost:8080/api/v1/maps/map2",
]

SHOTS = 100
PERF_DATA = "perf.data"
GRAPH_SVG = "graph.svg"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=str)
    return parser.parse_args()


def run(command):
    args = shlex.split(command)
    process = subprocess.Popen(args)
    return process


def stop(process):
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def shoot(url):
    subprocess.Popen(
        ["curl", "-s", url],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def make_shots():
    for _ in range(SHOTS):
        url = random.choice(AMMUNITION)
        shoot(url)
        time.sleep(0.1)


def build_flamegraph():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    flamegraph_dir = os.path.join(script_dir, "FlameGraph")
    stackcollapse = os.path.join(flamegraph_dir, "stackcollapse-perf.pl")
    flamegraph_pl = os.path.join(flamegraph_dir, "flamegraph.pl")

    perf_script = subprocess.Popen(
        ["perf", "script", "-i", PERF_DATA],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    collapse = subprocess.Popen(
        ["perl", stackcollapse],
        stdin=perf_script.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    perf_script.stdout.close()

    with open(GRAPH_SVG, "w") as svg_file:
        flamegraph = subprocess.Popen(
            ["perl", flamegraph_pl],
            stdin=collapse.stdout,
            stdout=svg_file,
            stderr=subprocess.DEVNULL,
        )
        collapse.stdout.close()
        flamegraph.wait()

    perf_script.wait()
    collapse.wait()


args = parse_args()

server_process = run(args.server)
time.sleep(1)

perf_process = subprocess.Popen(
    ["perf", "record", "-g", "-p", str(server_process.pid), "-o", PERF_DATA],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)

make_shots()

perf_process.send_signal(signal.SIGINT)
perf_process.wait()

stop(server_process)

build_flamegraph()

print(f"Готово! Флеймграф сохранён в {GRAPH_SVG}")
