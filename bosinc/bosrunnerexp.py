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
            time.sleep(0.5)

    for p in child_bos_list:
        p.send_signal(signal.SIGINT)
        p.wait()

def main(args: T.List[str]): 
    if args[1] == "agg": 
        # 1000 batteries 
        run_agg(1200, 2200-1)
    elif args[1] == "chain":
        pass
    pass

if __name__ == "__main__": 
    main(sys.argv)

