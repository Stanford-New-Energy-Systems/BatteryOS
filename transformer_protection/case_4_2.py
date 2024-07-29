import datetime
import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt 

from case_4_1 import genPriceVec as genPriceVec

def case_4_2(self, solar, loads, timesteps, num_homes, time_vec, initSOC):
    SOCs       = cp.Variable((timesteps+1, 1)) 
    meter      = cp.Variable((timesteps, num_homes))
    battery    = cp.Variable((timesteps, 1))
    costs      = np.zeros((num_homes,), dtype='float32')
    orig_costs = np.zeros((num_homes,), dtype='float32')

    price_vector = genPriceVec(self, timesteps, time_vec)

    constraints = []
    constraints += [SOCs[0] == initSOC]
    constraints += [SOCs[-1] == initSOC]
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]
    constraints += [battery <= self.dischg_power[0]]
    constraints += [battery >= -self.chg_power[0]] 

    for i in range(timesteps):
        constraints += [SOCs[i+1] == SOCs[i] - self.dt * battery[i, 0] / self.batt_energy[0]]  

    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] ]

    combined_meter = cp.sum(meter, axis=1) -  battery[:, 0]

    obj = cp.maximum(combined_meter,0) * self.dt @ price_vector 
    violation = self.lam * cp.sum(cp.square(cp.maximum(combined_meter - self.transformer_limit, 0)))

    prob = cp.Problem(cp.Minimize(obj + violation), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')

    
    for home in range(num_homes):
        orig_costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector
        
    for home in range(num_homes):
        costs[home] = orig_costs[home] / np.sum(orig_costs) * obj.value

    return meter.value, battery.value, costs, SOCs.value