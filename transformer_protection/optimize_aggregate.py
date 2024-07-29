#!/usr/bin/python3

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

    for i in range(size):
        constraints += [battery_SOCs[i][0] == batteries[i].initSOC]
        constraints += [battery_SOCs[i] <= 1]
        constraints += [battery_SOCs[i] >= 0]
        constraints += [meters[i] == -solar_lst[i][:-1] + loads_lst[i][:-1] - battery_outputs[i]]
        constraints += [battery_outputs[i] <= batteries[i].discharge_kW]
        constraints += [battery_outputs[i] >= -batteries[i].charge_kW] 

    meters_total = meters[0]
    total_cost = cp.maximum(meters[0], 0) * params["dt"] * params["peak_price"]
    for i in range(1, size):
        meters_total += meters[i]
        total_cost += cp.maximum(meters[i], 0) * params["dt"] * params["peak_price"]

    constraints += [cp.abs(meters_total) <= params["t_limit"]]

    objective = cp.sum(total_cost)
    problem   = cp.Problem(cp.Minimize(objective), constraints)
    result    = problem.solve(verbose=True)
    
    return meters, battery_outputs, result 

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
    peak_price           = 0.41
    transformer_limit_kW = 72

#     files = ["data/data_661_mar_5_6.csv",
#              "data/data_2335_mar_5_6.csv",
#              "data/data_1642_mar_3_4.csv",
#              "data/data_4373_mar_9_10.csv",
#              "data/data_4767_mar_3_4.csv",
#              "data/data_6139_mar_5_6.csv",
#              "data/data_7719_mar_2_3.csv",
#              "data/data_8156_feb_9_10.csv",
#              "data/data_9278_feb_3_4.csv"]

    size = 5
    files = glob.glob("./data/*.csv")

    data      = [None] * size
    solar     = [None] * size
    loads     = [None] * size
    batteries = []

#    batteries.append(Battery(5, 5, 5, .85))
#    batteries.append(Battery(5, 5, 5, .85))
#    batteries.append(Battery(5, 5, 5, .85))

    for i in range(size):
        batteries.append(TeslaBattery(.85)) 
        solar[i], loads[i], data[i] = process_data(files[i])
        
#    s0, l0, d0 = process_data("data/data_4373_mar_9_10.csv")
#    s1, l1, d1 = process_data("data/data_661_mar_5_6.csv")
#    s2, l2, d2 = process_data("data/data_2335_mar_5_6.csv")
#
#    solar.append(s0)
#    solar.append(s1)
#    solar.append(s2)
#
#    loads.append(l0)
#    loads.append(l1)
#    loads.append(l2)

    print(-solar[0])

    params = getParams(dt, hours, transformer_limit_kW, peak_price)

    meters, outputs, result = solveOpt(batteries, params, solar, loads)

    tform = []
    for i in range(meters.value.shape[1]):
        tform.append(sum(meters.value[:,i]))
    print("the max total value on the transformer is {}".format(max(tform)))

#    plot(d0, outputs)

#    plotData(data[0], outputs, solar, meters, loads, "optimize_agg.png")

