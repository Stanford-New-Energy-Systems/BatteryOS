import subprocess
import os 
import sys 
import signal
import time
import typing as T
exec_name: str = "./build/bos"

def run_agg(portmin: int, portmax: int): 
    # 1 - 1000 batteries 
    child_bos_list: T.List[subprocess.Popen] = []
    for port in range(portmin, portmax+1): 
        cp = subprocess.Popen([exec_name, "client", str(port)])
        child_bos_list.append(cp)
        for i in range(5):
            mp = subprocess.Popen([exec_name, "server", str(portmin), str(port)])
            mp.wait()
            time.sleep(1.5)

    for p in child_bos_list:
        p.send_signal(signal.SIGINT)
        p.wait()

def run_chain(portmin:int, portmax: int): 
    bos_chain: T.List[subprocess.Popen] = [] 
    endpoint = subprocess.Popen([exec_name, "client", str(portmin)])

    for port in range(portmin+1, portmax+1):
        chainnode = subprocess.Popen([exec_name, "chain", str(port), str(port-1)])
        bos_chain.append(chainnode)
        for i in range(5):
            chainend = subprocess.Popen([exec_name, "chainend", str(port)])
            chainend.wait()
            time.sleep(1.5)

    for c in bos_chain:
        c.send_signal(signal.SIGINT)
        c.wait()
    endpoint.send_signal(signal.SIGINT)
    endpoint.wait()

def main(args: T.List[str]): 
    if args[1] == "agg": 
        # 1000 batteries 
        run_agg(1200, 2199)
    elif args[1] == "chain":
        # chain length 1000 => 1200 - 2199
        # chain length 60 => 1200 - 1259
        run_chain(1200, 1259)
    pass

if __name__ == "__main__": 
    main(sys.argv)

