#!/usr/local/bin/python3

import sys
import glob
import numpy as np
import cvxpy as cp
import pandas as pd
import matplotlib.pyplot as plt
from batteries import TeslaBattery

def solveOpt(batteries, params, solar_lst, loads_lst):
    size = len(batteries)

    constraints     = []

    meters          = cp.Variable((size,params["timesteps"]-1))
    battery_outputs = cp.Variable((size,params["timesteps"]-1))
    battery_SOCs    = cp.Variable((size,params["timesteps"]))

    for i in range(size):
        for j in range(1, params["timesteps"]):  
            constraints += [battery_SOCs[i][j] == battery_SOCs[i][j-1] - params["dt"] * battery_outputs[i][j-1] / batteries[i].energy]

    objective = 0
    for i in range(size):
        constraints += [battery_SOCs[i][0] == batteries[i].initSOC]
        constraints += [battery_SOCs[i] <= 1]
        constraints += [battery_SOCs[i] >= 0]
        constraints += [meters[i] == -solar_lst[i][:-1] + loads_lst[i][:-1] - battery_outputs[i]]
        constraints += [battery_outputs[i] <= batteries[i].discharge_kW]
        constraints += [battery_outputs[i] >= -batteries[i].charge_kW] 

        off_peak_one = cp.maximum(meters[i,:60*3],0)      * params["dt"] * params["off_peak"]
        off_peak_two = cp.maximum(meters[i,60*9:], 0)     * params["dt"] * params["off_peak"]
        peak         = cp.maximum(meters[i,60*3:60*9], 0) * params["dt"] * params["peak_price"]

        objective += cp.sum(off_peak_one) + cp.sum(peak) + cp.sum(off_peak_two)

    constraints += [cp.abs(meters[0]) <= params["t_limit"]]
#    total_cost = cp.maximum(meters[0], 0) * params["dt"] * params["peak_price"]
    for i in range(1, size):
        constraints += [cp.abs(meters[i]) <= params["t_limit"]]
#        total_cost += cp.maximum(meters[i], 0) * params["dt"] * params["peak_price"]

#    objective = cp.sum(total_cost)
    problem   = cp.Problem(cp.Minimize(objective), constraints)
    result    = problem.solve(verbose=False)
    
    return meters, battery_outputs, objective 

def getParams(dt, hours, transformer_limit_kW, peak_price, off_peak_price):
    params = {}
    params["dt"]         = dt
    params["t_limit"]    = transformer_limit_kW
    params["timesteps"]  = int(hours/dt) 
    params["peak_price"] = peak_price
    params["off_peak"]   = off_peak_price

    return params

def process_data(filename):
    data  = pd.read_csv(filename)
    solar = np.array(data["Solar [kW]"][:24*60])
    loads = np.array(data["Loads [kW]"][:24*60])

    return solar, loads, data

def plot(data, outputs):
    plt.figure(figsize=(10,10))

    for i in range(len(outputs)):
        plt.plot(data['Date & Time'][:24*60-1], outputs[i].value, label = "Battery Output {}".format(i+1))

    plt.xticks(np.arange(0,24*60,60), np.arange(24))
    plt.xlabel('Time [hr]')
    plt.ylabel('Power [kW]')
    plt.legend()
    plt.savefig("aggregate_battery_outputs.png")
    plt.show()
    return

def plotData(data, outputs, solar, meter, loads, filename=None):
#    size = len(outputs)
    size = outputs.shape[0]
    fig, axs = plt.subplots(size, figsize=(20,20))

    for i in range(size):
        axs[i].plot(data['Date & Time'][:24*60-1], outputs[i].value, label = "Battery Output {}".format(i+1))
        axs[i].plot(data['Date & Time'][:24*60],   solar[i], label = 'Solar')
        axs[i].plot(data['Date & Time'][:24*60],   loads[i], label = 'Loads')
        axs[i].plot(data['Date & Time'][:24*60-1], meter[i].value, label = 'Meter')
        axs[i].legend()
        axs[i].set_xticks(np.arange(0,24*60,60), np.arange(24))
        axs[i].set_xlabel("Time [hr]", fontsize = 15)
        axs[i].set_ylabel("Power [kW]", fontsize = 15)
        axs[i].set_ylim(-10, 10)

    for ax in axs.flat:
        ax.label_outer()    

    if filename != None:
        plt.savefig(filename)    

    plt.show()
    return

if __name__ == "__main__":
    dt                   = 1/60 # 1 minute
    hours                = 24
    peak_price           = 0.45
    off_peak_price       = 0.10
    transformer_limit_kW = 75

#    files = glob.glob("./data/*.csv")
    files = glob.glob("./data/*mar_5_6*.csv")
#    size = 5
        
    size = 1 
    with open("results/individual_tests.csv", 'w') as file: 
#        while size < 25:
        for j in range(len(files)):
            data      = [None] * size
            solar     = [None] * size
            loads     = [None] * size
            batteries = []

#            for i in range(size):
            batteries.append(TeslaBattery(.85)) 
            solar[0], loads[0], data[0] = process_data(files[j])
                
            params = getParams(dt, hours, transformer_limit_kW/len(files), peak_price, off_peak_price)

            meters, outputs, result = solveOpt(batteries, params, solar, loads)

            print("result value = {}".format(result.value))
            print("result type = {}".format(type(result.value)))

            if type(result.value) == type(None):
                print("no solution found for size = {}".format(size))
                break

            tform = []
            for i in range(meters.value.shape[1]):
                tform.append(sum(meters.value[:,i]))
            sform = [str(x) for x in tform]
            tform = np.array(tform)
            
            maxVal = max(tform)
            absVal = max(abs(tform))

            # Write to File
            #   - number of homes
            #   - direction of power in transformer (source vs sink)
            #   - max power transformer dealt with
            #   - minimized cost value
            #   - list of power outputs for entire community 

            file.write("{},".format(size))
            if absVal == abs(maxVal):
                file.write("source,")
            else:
                file.write("sink,")
            file.write("{},".format(absVal))
            file.write("{},".format(result.value))
            file.write(",".join(sform))
            file.write("\n")

#            size +=  1 

#            print("the max total value on the transformer is {}".format(max(tform)))
#            print("the max total value on the transformer is {}".format(min(tform)))
#            print("the max total value on the transformer is {}".format(max(abs(tform))))

#    plotData(data[0], outputs, solar, meters, loads, "optimize_individual.png")


