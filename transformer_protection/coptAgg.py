#!/usr/bin/python3

import sys
import glob
import numpy as np
import cvxpy as cp
import pandas as pd
import matplotlib.pyplot as plt
from batteries import TeslaBattery
from batteries import Battery

def solveOpt(batteries, nBatt, params, solar_lst, loads_lst):
    nHomes = len(solar_lst)

    assert(nHomes >= nBatt)

    constraints     = []

    meters          = cp.Variable((nHomes,params["timesteps"]-1))
    battery_outputs = cp.Variable((nBatt,params["timesteps"]-1))
    battery_SOCs    = cp.Variable((nBatt,params["timesteps"]))

    for i in range(nBatt):
        for j in range(1, params["timesteps"]):  
            constraints += [battery_SOCs[i][j] == battery_SOCs[i][j-1] - params["dt"] * battery_outputs[i][j-1] / batteries[i].energy]

    for i in range(nBatt):
        constraints += [battery_SOCs[i][0] == batteries[i].initSOC]
        constraints += [battery_SOCs[i] <= 1]
        constraints += [battery_SOCs[i] >= 0]
        constraints += [meters[i] == -solar_lst[i][:-1] + loads_lst[i][:-1] - battery_outputs[i]]
        constraints += [battery_outputs[i] <= batteries[i].discharge_kW]
        constraints += [battery_outputs[i] >= -batteries[i].charge_kW] 
    
    for i in range(nBatt, nHomes):
        constraints += [meters[i] == -solar[i][:-1] + loads_lst[i][:-1]]

    meters_total = meters[0]
    total_cost = cp.maximum(meters[0], 0) * params["dt"] * params["peak_price"]
    for i in range(1, nHomes):
        meters_total += meters[i]
        total_cost += cp.maximum(meters[i], 0) * params["dt"] * params["peak_price"]

    constraints += [cp.abs(meters_total) <= params["t_limit"]]

    objective = cp.sum(total_cost)
    problem   = cp.Problem(cp.Minimize(objective), constraints)
    result    = problem.solve(verbose=False)
    
    return meters, battery_outputs, objective 

def getParams(dt, hours, transformer_limit_kW, peak_price):
    params = {}
    params["dt"]         = dt
    params["t_limit"]    = transformer_limit_kW
    params["timesteps"]  = int(hours/dt) 
    params["peak_price"] = peak_price

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
    fig, axs = plt.subplots(size, figsize=(10,10))

#    for i in range(size):
#        axs[i].plot(data['Date & Time'][:24*60-1], outputs[i].value, label = "Battery Output {}".format(i+1))
#        axs[i].plot(data['Date & Time'][:24*60],   solar[i], label = 'Solar')
#        axs[i].plot(data['Date & Time'][:24*60],   loads[i], label = 'Loads')
#        axs[i].plot(data['Date & Time'][:24*60-1], meter[i].value, label = 'Meter')
#        axs[i].legend()
#        axs[i].set_xticks(np.arange(0,24*60,60), np.arange(24))
#        axs[i].set_xlabel("Time [hr]", fontsize = 15)
#        axs[i].set_ylabel("Power [kW]", fontsize = 15)
#        axs[i].set_ylim(-10, 10)
    axs.plot(data['Date & Time'][:24*60-1], outputs[0].value, label = "Battery Output")
    axs.plot(data['Date & Time'][:24*60],   solar[0], label = 'Solar')
    axs.plot(data['Date & Time'][:24*60],   loads[0], label = 'Loads')
    axs.plot(data['Date & Time'][:24*60-1], meter[0].value, label = 'Meter')
    axs.legend()
    axs.set_xticks(np.arange(0,24*60,60), np.arange(24))
    axs.set_xlabel("Time [hr]", fontsize = 15)
    axs.set_ylabel("Power [kW]", fontsize = 15)
    axs.set_ylim(-75, 75)

#    for ax in axs.flat:
#        ax.label_outer()    

    if filename != None:
        plt.savefig(filename)    

    plt.show()
    return

if __name__ == "__main__":
    dt                   = 1/60 # 1 minute
    hours                = 24
    peak_price           = 0.41
    transformer_limit_kW = 72

    nBatt = 14
    files = glob.glob("./data/*.csv")

    size  = 14
    with open("results/aggregate_tests.csv", 'w') as file:
        while size < 100:
            data      = [None] * size
            solar     = [None] * size
            loads     = [None] * size
            batteries = []

            for i in range(size): # size is always greater than or equal to nBatt 
                if i < nBatt:
                    batteries.append(TeslaBattery(.85)) 
                solar[i], loads[i], data[i] = process_data(files[i])

#            if nBatt == 0:
#                nBatt = 1
#                batteries.append(Battery(0,0,0,0))

            params = getParams(dt, hours, transformer_limit_kW, peak_price)

            meters, outputs, result = solveOpt(batteries, nBatt, params, solar, loads)

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

            size += 1

#    plotData(data[0], outputs, np.array([totalSolar]), meters, np.array([totalLoads]), "optimize_agg_copy.png")

