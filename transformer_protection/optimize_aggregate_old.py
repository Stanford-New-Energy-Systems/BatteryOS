#!/usr/local/bin/python3

import sys
import numpy as np
import cvxpy as cp
import pandas as pd
import matplotlib.pyplot as plt

class Battery:
    def __init__(self, energy_kWh, charge_kW, discharge_kW, SOC):
        self.energy       = energy_kWh 
        self.initSOC      = SOC
        self.charge_kW    = charge_kW
        self.discharge_kW = discharge_kW 

def solveOpt(batteries, params, solar_lst, loads_lst):
    size = len(batteries)

    meters          = []
    constraints     = []
    battery_SOCs    = []
    battery_outputs = []

    for i in range(size):
        meters.append(cp.Variable((params["timesteps"]-1,)))
        battery_SOCs.append(cp.Variable((params["timesteps"],)))
        battery_outputs.append(cp.Variable((params["timesteps"]-1,)))

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

    constraints += [meters_total <= params["t_limit"]]

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

if __name__ == "__main__":
    dt                   = 1/60 # 1 minute
    hours                = 24
    peak_price           = 0.41
    transformer_limit_kW = 10

    solar     = []
    loads     = []
    batteries = []

    batteries.append(Battery(5, 5, 5, .85))
    batteries.append(Battery(5, 5, 5, .85))
    batteries.append(Battery(5, 5, 5, .85))

    s0, l0, d0 = process_data("Data/data_apr_9_10.csv")
    s1, l1, d1 = process_data("Data/data_apr_15_16.csv")
    s2, l2, d2 = process_data("Data/data_apr_20_21.csv")

    solar.append(s0)
    solar.append(s1)
    solar.append(s2)

    loads.append(l0)
    loads.append(l1)
    loads.append(l2)

    params = getParams(dt, hours, transformer_limit_kW, peak_price)

    meters, outputs, result = solveOpt(batteries, params, solar, loads)

    plot(d0, outputs)
