#!/usr/bin/env python3

import P
import plotPID
import pandas as pd

def convertToSOC(capacity_kWh, maxCapacity_kWh):
    return capacity_kWh/maxCapacity_kWh * 100 

def run(pid, hours, tStep, initpowers_kW, solar_kW, interval = 1):
    total_kW, powers_kW        = [], []
    currSolar_kW, capacity_kWh = [], [] 
    
    currPowers_kW = initpowers_kW
    solarIndex    = 0 
    pRequest_kW   = sum(initpowers_kW)
    nBatt         = len(initpowers_kW)

    for i in range(nBatt):
        powers_kW.append([])
        capacity_kWh.append([])
        currSolar_kW.append([])
    
    seconds = hours * 3600
    for second in range(seconds, 0, -interval):
        remaining_hours = second / 3600

        if second % 300 == 0: # every five minutes 
            solarIndex += 1 

        currPowers_kW = pid.calculate(pRequest_kW, currPowers_kW, solar_kW[solarIndex], remaining_hours, tStep = tStep)

        for i in range(nBatt):
            powers_kW[i].append(currPowers_kW[i])
            currSolar_kW[i].append(solar_kW[solarIndex][i])
            capacity_kWh[i].append(convertToSOC(pid.capacities_kWh[i], pid.maxCapacities_kWh[i]))
        total_kW.append(sum(currPowers_kW))

    return total_kW, currSolar_kW, powers_kW, capacity_kWh

def processData(files):
    solar_kW = []
    for file in files:
        df = pd.read_csv(file)
        df.loc[:,'Solar [kW]'][df.loc[:,"Solar [kW]"] < 0] = 0
        df = df[60*8:60*20]
        solar_kW.append(list(df[::5]["Solar [kW]"]))
    return solar_kW

if __name__ == "__main__":
    pRequest_kW  = 35
    
    nBatt        = 4
#    SOCs         = [1.] * nBatt 
    SOCs         = [1., .97, .85, .85]
    solar_kW     = [] 
    maxPowers_kW = [10] * nBatt

    maxCapacities_kWh = [40] * nBatt 
    capacities_kWh    = [SOCs[i] * maxCapacities_kWh[i] for i in range(nBatt)]

    print("capacities = {}".format(capacities_kWh))
    
    files = ["../Data/data_1642_june_10_11.csv",
             "../Data/data_2335_june_10_11.csv",
             "../Data/data_6139_june_10_11.csv",
             "../Data/data_8156_june_10_11.csv"]

    s_kW = processData(files)
    for i in range(len(s_kW[0])):
        solar_kW.append([s_kW[j][i] for j in range(len(s_kW))]) 

    threshold = 2
    P_Controller = P.P(capacities_kWh, maxCapacities_kWh) 
    P_Controller.setPDiffThreshold(threshold)

    # Initial Set Up
    hours      = 5
    tStep      = 3600/300 #for five minutes
    pValues_kW = [pRequest_kW / nBatt] * nBatt 
    
    # Run P Controller
    t, s, p, c = run(P_Controller, hours, tStep = tStep, initpowers_kW = pValues_kW, solar_kW = solar_kW, interval = 300) 

    #print(s)
    [print("average power of battery = {}".format(sum(x)/len(x))) for x in p]
    print(t)
    plotPID.plotCapacities(c, "PID_balanced_SOC_test_test.png")
