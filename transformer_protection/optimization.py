#!/usr/local/bin/python3

import sys
import numpy as np
import cvxpy as cp
import pandas as pd
import matplotlib.pyplot as plt


class StationaryBattery(object):
    def __init__(self):
        
        self.dt = 1/60 * 1 #1 min
        self.hours = 24
        self.timesteps = int(self.hours / self.dt)
        self.eta = 1
        self.energy = 10 #kWh
        self.chg_power = 5 #kW
        self.dischg_power = 5 #kW
        self.peak_price=0.41 #$/kWh
        self.off_peak_price = 0.39 #$/kWh
        self.transformer_limit = 10 #kW

    def solve_opt(self, solar, loads):
        meter_amount = cp.Variable((self.timesteps-1,))
        batt_output = cp.Variable((self.timesteps-1,))
        batt_soc = cp.Variable((self.timesteps,))
        obj = cp.sum(cp.maximum(meter_amount,0) * self.dt * self.peak_price)
        constraints = []
        constraints += [batt_output<=self.dischg_power]
        constraints += [batt_output>=-self.chg_power]
        for i in range(1, self.timesteps):
            constraints += [batt_soc[i] == batt_soc[i-1] - self.dt * batt_output[i-1] / self.energy]
        constraints += [batt_soc[0] == .5]
        constraints += [batt_soc<=1]
        constraints += [batt_soc>=0]
        constraints += [meter_amount == -solar[:-1] + loads[:-1] - batt_output]
        constraints += [meter_amount <= self.transformer_limit]
        prob = cp.Problem(cp.Minimize(obj), constraints)
        result = prob.solve(verbose=True)
        return meter_amount.value, batt_output.value, result

def process_data(filename=None):
    if filename == None:
        #data=pd.read_csv('Data\data_apr_9_10.csv')
        data=pd.read_csv('Data/data_apr_9_10.csv')
    else:
        data=pd.read_csv(filename)
    solar=np.array(data['Solar [kW]'][:24*60])
    print(len(solar))
    loads=np.array(data['Loads [kW]'][:24*60])
    
    return solar, loads, data

def plot(data, meter, output):
    plt.figure()
    plt.plot(data['Date & Time'][:24*60], data['Solar [kW]'][:24*60], color = 'tab:orange', label='Solar')
    plt.plot(data['Date & Time'][:24*60], -data['Loads [kW]'][:24*60], color = 'tab:green', label='Loads')
    plt.plot(data['Date & Time'][:24*60-1], meter, color = 'tab:blue', label='Meter')
    plt.plot(data['Date & Time'][:24*60-1], output,color = 'tab:red',  label='Battery Output')
    plt.xticks(np.arange(0,24*60,60), np.arange(24))
    plt.xlabel('Time [hr]')
    plt.ylabel('Power [kW]')
    plt.legend()
    plt.show()
    return

def main():
    if len(sys.argv) != 2:
        sys.exit("./optimization.py inputFileName")
 
    solar, loads, data = process_data(sys.argv[1])
    battery_problem = StationaryBattery()
    meter, output, cost = battery_problem.solve_opt(solar, loads)
    #output.to_csv('one_day_batt_minute.csv')
    #plot(data, meter, output)
    print(len(output))
    return
    


if __name__ == '__main__':
    main()

