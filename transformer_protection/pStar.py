import cvxpy as cp
import numpy as np 

def pStar(self, solar, loads, timesteps, num_homes, time_vec, initSOC): # this function assumes solar and loads are lists of lists
    SOCs        = cp.Variable((timesteps+1,num_homes)) 
    meter       = cp.Variable((timesteps, num_homes))
    batt_output = cp.Variable((timesteps, num_homes))

    #create price vector
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
    
    constraints = []
    constraints += [SOCs[0, :] == initSOC]
    constraints += [SOCs[-1, :] == initSOC]
    
    constraints += [SOCs <= 1]
    constraints += [SOCs >= 0]

    print("num_homes = {} and length of self = {}".format(num_homes, len(self.dischg_power)))

    obj = 0 
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - batt_output[:, home]]
        constraints += [batt_output[:, home] <= self.dischg_power[home]]
        constraints += [batt_output[:, home] >= -self.chg_power[home]]  

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - self.dt * (batt_output[i, home] / self.batt_energy[home])]

        obj += cp.maximum(meter[:, home],0) * self.dt @ price_vector
        obj += self.lam * cp.sum(cp.square(cp.maximum(meter[:, home] - self.transformer_limit / self.num_homes, 0)))
        
        
    prob = cp.Problem(cp.Minimize(obj), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')

    return obj.value


def calcWeight(self, solar, loads, timesteps, num_homes, time_vec, initSOC, pStar): # this function assumes solar and loads are lists of lists
    SOCs        = cp.Variable((timesteps+1,num_homes)) 
    meter       = cp.Variable((timesteps, num_homes))
    batt_output = cp.Variable((timesteps, num_homes))
    weights     = cp.Variable(num_homes,)
    param       = cp.Parameter()
    costs       = np.zeros((num_homes,), dtype='float32')
    
    #create price vector
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
    
    constraints = []
    constraints += [SOCs[0, :] == cp.multiply(initSOC, weights)]
    constraints += [SOCs[-1, :] == cp.multiply(initSOC, weights)]
    
    for i in range(timesteps + 1):
        constraints += [SOCs[i, :] <= weights]
    constraints += [SOCs >= 0]
    
    constraints += [weights >= 0.2] # every homeowner must keep at least 20% of their battery 
    constraints += [weights <= 1] # every homeowner must give at least 20% of their battery 

    obj = 0 
    for home in range(num_homes):
        constraints += [meter[:, home] == -solar[:, home] + loads[:, home] - batt_output[:, home]]
        constraints += [batt_output[:, home] <= self.dischg_power[home] * weights[home]]
        constraints += [batt_output[:, home] >= -self.chg_power[home] * weights[home]]  

        for i in range(timesteps):
            constraints += [SOCs[i+1, home] == SOCs[i, home] - self.dt * (batt_output[i, home] / self.batt_energy[home])]

        obj += cp.maximum(meter[:, home],0) * self.dt @ price_vector
        obj += self.lam * cp.sum(cp.square(cp.maximum(meter[:, home] - self.transformer_limit / self.num_homes, 0)))

    constraints += [obj <= pStar * 1.15]
    
#    for i in np.arange(1.5, 2.05, .05): 
#        param.value = round(i, 2)

    prob = cp.Problem(cp.Minimize(cp.sum(weights)), constraints)
    total_cost = prob.solve(verbose=False, solver='MOSEK')

#        if prob.status not in [cp.INFEASIBLE, cp.UNBOUNDED]:
#            break

    for home in range(num_homes):
        costs[home] = np.maximum(meter.value[:, home],0) * self.dt @ price_vector

    return obj.value, prob.value, np.round(weights.value, 2)

def getFraction(self, solar, loads, timesteps, num_homes, time_vec, initSOC):
    print("calling pStar ...")
    pOpt    = pStar(self, solar, loads, timesteps, num_homes, time_vec, initSOC)
    print("got a pStar with a value = {}".format(pOpt))
    weights = calcWeight(self, solar, loads, timesteps, num_homes, time_vec, initSOC, pOpt)
    print("finished weights function with weights = {}".format(weights[-1]))

    with open("weights.txt", "a") as file:
        file.write(str(weights[-1]))
        file.write("\n")
    
    return weights[-1] 
