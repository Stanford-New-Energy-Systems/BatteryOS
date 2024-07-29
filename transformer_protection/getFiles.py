import os
import random
import numpy as np
import pandas as pd

def countViolations(meter, limit = 25, violation_time_steps = 2):
    new_meter = meter.reshape(-1, violation_time_steps)
    iix = np.where((new_meter > [limit] * violation_time_steps).sum(axis = 1) == violation_time_steps)[0]

    return len(iix)

def getFiles(files, trial, iters = 1488):
    random.seed(50)
    for i in range(trial):
        solar = np.zeros((iters, 1))
        loads = np.zeros((iters, 1))

        names = []
        count = 0
        violations = 0

        while violations == 0:
            count += 1

            file = random.sample(files, 1)
            print("opening file = {}".format(file))
            
            names.append(file)

            data = pd.read_csv(file[0])

            solar[:, 0] += data.solar
            loads[:, 0] += data.loads

            violations = countViolations(loads - solar)

        print("{} homes under this configuration".format(count))

    #return [x[0].split("july/")[0] + x[0].split("july/")[1] for x in names], count
    return [x[0].split("july/")[1] for x in names], count


if __name__ == "__main__":
    random.seed(50)

    july_folder_path = "Data_L2/csv/july/"
    july_files = os.listdir(july_folder_path)
    july_files = ["{}{}".format(july_folder_path, july_file) for july_file in july_files]

    print(july_files)
    trial = 1

    print(getFiles(july_files, trial))    

    random.seed(50)
    print(random.sample(july_files, 1))
