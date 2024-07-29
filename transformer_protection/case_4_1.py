import datetime
import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt 

def genPriceVec(self, timesteps, time_vec):
    price_vector = np.zeros((timesteps,))
    for time in range(timesteps):
        if time_vec[time].month >=6 and time_vec[time].month<10: #summer
            if time_vec[time].hour < 15: #off peak
                price_vector[time] = self.off_peak_summer
            elif time_vec[time].hour == 15 or time_vec[time].hour >=21: #peak
                price_vector[time] = self.peak_summer
            else: #off peak
                price_vector[time] = self.partial_peak_summer
        else: #winter
            if time_vec[time].hour < 15: #off peak
                price_vector[time] = self.off_peak_winter
            elif time_vec[time].hour == 15 or time_vec[time].hour >=21: #peak
                price_vector[time] = self.peak_winter
            else: #off peak
                price_vector[time] = self.partial_peak_winter

    return price_vector

def case_4_1(self, solar, loads, timesteps, num_homes, time_vec, initSOC):
    meter   = cp.Variable((timesteps, num_homes))
    battery = cp.Variable((timesteps, num_homes))
    SOCs    = cp.Variable((timesteps+1, num_homes))
    costs   = np.zeros((num_homes,), dtype="float32") 

    # create price vector
    price_vector = genPriceVec(self, timesteps, time_vec)

    constraints = []
    constraints += [SOCs[0, :] == initSOC]
    constraints += [SOCs[-1, :] == initSOC]
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]

    obj = 0
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - battery[:, home]]
        constraints += [battery[:, home] <= self.dischg_power[home]]
        constraints += [battery[:, home] >= -self.chg_power[home]]

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - self.dt * battery[i, home] / self.batt_energy[home]] 

        obj += cp.maximum(meter[:, home], 0) * self.dt @ price_vector        

    prob = cp.Problem(cp.Minimize(obj), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')
    for home in range(num_homes):
        costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector

    return meter.value, battery.value, costs, SOCs.value 
