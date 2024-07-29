#!/usr/local/bin/python3

import sys
import pandas as pd

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("./dataset.py inputFileName outputFileName")

    df = pd.read_csv(sys.argv[1])
    df = df.filter(items = ["Date & Time", "simple.gen [kW]", "simple.Solar [kW]"]) 

    df = df.set_index("Date & Time")

    index = list(df.index)
    index.reverse()

    df = df.reindex(index)
    df.reset_index(inplace = True)
    df.columns = ["Date & Time", "Loads [kW]", "Solar [kW]"]

    df.to_csv(sys.argv[2], index=False)

    print(df)
