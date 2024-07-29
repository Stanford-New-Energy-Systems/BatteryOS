#!/usr/local/bin/python3

import math
import random
import os
import numpy as np
import matplotlib.pyplot as plt
import datetime as datetime
from optimization_MPC import MPC 
from optimization_MPC import process_data
from getFiles import getFiles
import pandas as pd

class transformer(object):
    def __init__(self, loads):
        self.timesteps = len(loads)
        self.dt = 1/2 # hours

        self.delta_theta_h  = [0] * self.timesteps
        self.delta_theta_TO = [0] * self.timesteps
        
        self.delta_theta_TO_U = [0] * self.timesteps
        #self.delta_theta_TO_i = [0] * self.timesteps
        self.delta_theta_H_U = [0] * self.timesteps
        #self.delta_theta_H_i = [0] * self.timesteps
        self.tau_TO = [0] * self.timesteps
        self.K_U = [0] * self.timesteps
        self.K_i = [0] * self.timesteps

        core_weight = 110.23 #lbs
        tank_weight = 110.23 #lbs
        oil_vol = 40.33 #gallons
        self.C = 0.06 * core_weight + 0.04 * tank_weight + 1.33 * oil_vol # eq 12b
        self.K = 25 #kVA
        self.theta_h  = [0] * self.timesteps
        self.Faa      = [0] * self.timesteps
        

        self.theta_a   = 25      # degrees celsius, ambient temp
        self.LT_normal = 180000  # hours (found on page 13 of IEEE transformer model document) & C.1.3.5 - life @ rated HST
        self.n=0.8 # for ONAN, from table 4
        self.m=0.8
        self.P_TR = 481
        
        self.R = 377 / 104 #W/W
        self.delta_theta_TO_R = 65 #"temperature rise" on data sheet
        self.delta_theta_H_AR = 80 # assumption from data sheet
        self.delta_theta_H_R = self.delta_theta_H_AR - self.delta_theta_TO_R
        self.tau_TO_R = self.C * self.delta_theta_TO_R / self.P_TR # eq 14
        
        self.delta_theta_TO[0] = self.delta_theta_TO_R * np.power((1)/(self.R+1),self.n) -0.001# start with no load
        #self.delta_theta_TO[0]
        
        self.theta_h[0] = self.theta_a + self.delta_theta_TO_R * np.power((1)/(self.R+1),self.n)
       

    def calc_aging(self, loads):
        timesteps = len(loads)
        loads = np.maximum(np.array(loads), 0) 
        #loads = 26 * np.ones((len(loads))) ## test loads
        #loads[300:500]=20 
        for i in range(1, timesteps):
            
            self.K_U[i] = loads[i] / self.K
            self.K_i[i] = loads[i-1] / self.K

            self.delta_theta_TO_U[i] = self.delta_theta_TO_R * np.power((self.K_U[i]**2 * self.R + 1)/(self.R+1),self.n) # eq 11
            #self.delta_theta_TO_i[i] = self.delta_theta_TO_R * np.power((self.K_i[i]**2 * self.R + 1)/(self.R+1),self.n) #eq 10
            
            self.delta_theta_H_U[i] = self.delta_theta_H_R * self.K_U[i]**(2*self.m) # eq 18
            self.delta_theta_h[i] = self.delta_theta_H_U[i] # conservative estimate based on guide
            
            # if loads[i] == 0:# or loads[i]<loads[i-1]:
            #     self.delta_theta_TO[i] =0 
            # else:
            
            self.tau_TO[i] = self.tau_TO_R * ((self.delta_theta_TO_U[i]/self.delta_theta_TO_R - self.delta_theta_TO[i-1]/self.delta_theta_TO_R) / (np.power(self.delta_theta_TO_U[i]/self.delta_theta_TO_R,1/self.n)-np.power(self.delta_theta_TO[i-1]/self.delta_theta_TO_R,1/self.n))) # eq 15

              
            self.delta_theta_TO[i] = (self.delta_theta_TO_U[i] - self.delta_theta_TO[i-1]) * (1-np.exp(-self.dt/self.tau_TO[i])) + self.delta_theta_TO[i-1] # eq 9

            
            #  self.tau_TO[i] = self.tau_TO_R * ((self.delta_theta_TO_U[i]/self.delta_theta_TO_R - self.delta_theta_TO[i-1]/self.delta_theta_TO_R) / (np.power(self.delta_theta_TO_U[i]/self.delta_theta_TO_R,1/self.n)-np.power(self.delta_theta_TO[i-1]/self.delta_theta_TO_R,1/self.n))) # eq 15
            #self.tau_TO[i] = self.tau_TO_R * ((self.delta_theta_TO_U[i]/self.delta_theta_TO_R - self.delta_theta_TO_i[i]/self.delta_theta_TO_R) / (np.power(self.delta_theta_TO_U[i]/self.delta_theta_TO_R,1/self.n)-np.power(self.delta_theta_TO_i[i]/self.delta_theta_TO_R,1/self.n))) # eq 15

            #  self.delta_theta_TO[i] = (self.delta_theta_TO_U[i] - self.delta_theta_TO[i-1]) * (1-np.exp(-self.dt/self.tau_TO[i])) + self.delta_theta_TO[i-1] # eq 9
              #self.delta_theta_TO[i] = (self.delta_theta_TO_U[i] - self.delta_theta_TO_i[i]) * (1-np.exp(-self.dt/self.tau_TO[i])) + self.delta_theta_TO_i[i] # eq 9
            
            self.theta_h[i] = self.theta_a  +self.delta_theta_TO[i] + self.delta_theta_h[i] #eq 7
            self.Faa[i] = np.exp((15000/383) - (15000/(self.theta_h[i]  + 273))) # eq 2


        Feqa = [(sum(self.Faa[:i]) * (self.dt)) / (self.dt * i) for i in range(1, timesteps+1)] #eq 3

        percentLOL = [Feqa[i] *(self.dt * i)* 100 /self.LT_normal for i in range(timesteps) ] #eq 4

        return percentLOL, self.theta_h

    

def plot_fig_3(delta_t, lol_no_bess, theta_no_bess, lol_pf, theta_pf, lol_mpc, theta_mpc, start, mpc_problem, case):
    time_vec = np.arange(0,int(len(lol_no_bess) * delta_t / 4), delta_t)
    colorset = ['#4477AA', '#EE6677', '#228833', '#CCBB44', '#66CCEE', '#AA3377', '#BBBBBB']
    if case==1:
        label_pf = 'Individual PF'
        label_mpc = 'Individual MPC'
    elif case==3:

        label_pf = 'Hybrid PF'
        label_mpc = 'Hybrid MPC'
    else:
        label_pf = 'Joint PF'
        label_mpc = 'Joint MPC'
    
    fig, ax = plt.subplots(2,1, figsize = (7.5,6.3))
    dates = [(start + datetime.timedelta(days=day)).strftime('%m/%d') for day in range(int(mpc_problem.total_horizon/mpc_problem.mpc_horizon)+1)]


    #ax[0]: hottest spot temp
    ax[0].plot(time_vec, theta_no_bess[:24*2*7], color = colorset[0], linewidth =2, label='No Battery')
    ax[0].plot(time_vec, theta_pf[:24*2*7], color = colorset[1], linewidth =2, label=label_pf)
    ax[0].plot(time_vec, theta_mpc[:24*2*7], color = colorset[2], linewidth =2, label=label_mpc)


    ax[0].set_ylabel('Hottest Spot Temp. [$^\circ$C]', fontsize=14)
    ax[0].legend(fontsize=11)
    ax[0].tick_params(axis='both', which='major', labelsize=12)
    print(int(len(time_vec) * mpc_problem.dt))
    print(int(np.floor(int(mpc_problem.total_horizon/mpc_problem.mpc_horizon + 1))/2))
    #ax[0].set_xticks(np.linspace(0,int(len(time_vec) * mpc_problem.dt), int(np.floor(int(mpc_problem.total_horizon/mpc_problem.mpc_horizon + 1))/2)))
    ax[0].set_xticks(np.linspace(0,int(len(time_vec) * mpc_problem.dt), 8))
    #ax[0].set_xticklabels(dates[::2])
    ax[0].set_xticklabels(dates[:8])
    #ax[0].set_xlabel('Date', fontsize=13)
    #ax[0].axhline(0, color='k', linewidth=.8)

    #ax[1]: percent loss of life
    ax[1].plot(time_vec, lol_no_bess[:24*2*7], color = colorset[0], linewidth =2, label='No Battery')
    ax[1].plot(time_vec, lol_pf[:24*2*7], color=colorset[1],linewidth =2,  label=label_pf)
    ax[1].plot(time_vec, lol_mpc[:24*2*7], color = colorset[2], linewidth =2, label=label_mpc)


    ax[1].set_ylabel('% Loss of Life', fontsize=14)
    ax[1].legend(fontsize=11)
    #ax[1].set_xticks(np.linspace(0,int(len(time_vec)*mpc_problem.dt ), int(np.floor(int(mpc_problem.total_horizon/mpc_problem.mpc_horizon + 1))/2)) )
    ax[1].set_xticks(np.linspace(0,int(len(time_vec) * mpc_problem.dt), 8))
    # ax[1].set_xticklabels(dates[::2])
    ax[1].set_xticklabels(dates[:8])
    ax[1].tick_params(axis='both', which='major', labelsize=12)
    ax[1].set_xlabel('Date', fontsize=13)

    

    plt.savefig('Figures/3_case_1_lol_hst.pdf', bbox_inches='tight')
    plt.savefig('Figures/3_case_1_lol_hst.png', bbox_inches='tight')
    plt.show()
    return

def plot_fig_4(lol_no_bess, lol_pf_1, lol_mpc_1, lol_pf_3, lol_mpc_3):
    colorset = ['#4477AA', '#EE6677', '#228833', '#CCBB44', '#66CCEE', '#AA3377', '#BBBBBB']
    fig, (ax1, ax2) = plt.subplots(2,1,figsize=(7.5, 5))

    ax1.spines['bottom'].set_visible(False)
    ax2.spines['top'].set_visible(False)

    ax1.xaxis.tick_top()
    ax1.tick_params(labeltop='off')  # don't put tick labels at the top
    ax2.xaxis.tick_bottom()

    d = .015  # how big to make the diagonal lines in axes coordinates
    # arguments to pass to plot, just so we don't keep repeating them
    kwargs = dict(transform=ax1.transAxes, color='k', clip_on=False)
    ax1.plot((-d, +d), (-d, +d), **kwargs)        # top-left diagonal
    ax1.plot((1 - d, 1 + d), (-d, +d), **kwargs)  # top-right diagonal

    kwargs.update(transform=ax2.transAxes)  # switch to the bottom axes
    ax2.plot((-d, +d), (1 - d, 1 + d), **kwargs)  # bottom-left diagonal
    ax2.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)  # bottom-right diagonal
    boxprops = dict(color="black",linewidth=1.5)
    medianprops = dict(color='black',linewidth=1.5)
    capprops = dict(color="black",linewidth=1.5)
    whiskerprops = dict(color="black",linewidth=1.5)
    bplot = ax1.boxplot([lol_no_bess, [], [], [], []], patch_artist=True, 
                 boxprops=boxprops, medianprops=medianprops, capprops=capprops, whiskerprops=whiskerprops,
                     showfliers=False)
    bplot['boxes'][0].set_facecolor(colorset[0])
    bplot2 = ax2.boxplot([[], lol_pf_1, lol_mpc_1, lol_pf_3, lol_mpc_3], patch_artist=True, 
                 boxprops=boxprops, medianprops=medianprops, capprops=capprops, whiskerprops=whiskerprops,
                    labels=['No BESS', 'Indiv. PF', 'Indiv. MPC', 'Joint PF', 'Joint MPC'], showfliers=False)
    # for patch, color in zip(bplot['boxes'], colorset[0]):
    #     patch.set_facecolor(color)
    for patch, color in zip(bplot2['boxes'], colorset[:5]):
        patch.set_facecolor(color)
    #ax1.set_ylabel('% Loss of Life', fontsize=15)
    fig.text(0.01, 0.5, '% Loss of Life', fontsize=15, va='center', rotation='vertical')
    ax2.set_ylim(0, 0.05)
    ax1.set_ylim(0.01, 2.7)
    ax1.xaxis.set_tick_params(labeltop=False)
    ax2.set_xticklabels(labels=ax2.get_xticklabels(), fontsize=14)
    ax2.set_yticklabels(labels=ax2.get_yticklabels(), fontsize=14)
    ax1.set_yticklabels(labels=ax1.get_yticklabels(), fontsize=14)
    

    plt.savefig('Figures/4_case_1_3_lol.pdf', bbox_inches='tight')
    plt.savefig('Figures/4_case_1_3_lol.png', bbox_inches='tight')
    plt.show()
    return

def plot_fig_6(lol_pf_1, lol_pf_3, lol_pf_4):
    colorset = ['#4477AA', '#EE6677', '#228833', '#CCBB44', '#66CCEE', '#AA3377', '#BBBBBB']
    fig, axes = plt.subplots(figsize=(7.5, 4.5))
    boxprops = dict(color="black",linewidth=1.5)
    medianprops = dict(color='black',linewidth=1.5)
    capprops = dict(color="black",linewidth=1.5)
    whiskerprops = dict(color="black",linewidth=1.5)
    bplot = axes.boxplot([lol_pf_1, lol_pf_3, lol_pf_4], patch_artist=True, 
                 boxprops=boxprops, medianprops=medianprops, capprops=capprops, whiskerprops=whiskerprops,
                    labels=['Indiv. PF', 'Joint PF', 'Hybrid PF'], showfliers=False)
    for patch, color in zip(bplot['boxes'], ['#EE6677', '#CCBB44', '#AA3377']):
        patch.set_facecolor(color)
    axes.set_ylabel('% Loss of Life', fontsize=15)
    axes.set_xticklabels(labels=axes.get_xticklabels(), fontsize=14)
    axes.set_yticklabels(labels=axes.get_yticklabels(), fontsize=14)
    

    plt.savefig('Figures/6_case_1_3_4_lol_pf.pdf', bbox_inches='tight')
    plt.savefig('Figures/6_case_1_3_4_lol_pf.png', bbox_inches='tight')
    plt.show()
    return

def plot_fig_8(lol_no_bess,lol_pf_1, lol_mpc_1, lol_pf_3, lol_mpc_3, lol_pf_4, lol_mpc_4):
    colorset = ['#4477AA', '#EE6677', '#228833', '#CCBB44', '#66CCEE', '#AA3377', '#BBBBBB']
    fig, ax1 = plt.subplots(1,1,figsize=(9.5, 5))

    # ax1.spines['bottom'].set_visible(False)
    # ax2.spines['top'].set_visible(False)

    # ax1.xaxis.tick_top()
    # ax1.tick_params(labeltop='off')  # don't put tick labels at the top
    # ax2.xaxis.tick_bottom()

    # d = .015  # how big to make the diagonal lines in axes coordinates
    # # arguments to pass to plot, just so we don't keep repeating them
    # kwargs = dict(transform=ax1.transAxes, color='k', clip_on=False)
    # ax1.plot((-d, +d), (-d, +d), **kwargs)        # top-left diagonal
    # ax1.plot((1 - d, 1 + d), (-d, +d), **kwargs)  # top-right diagonal

    # kwargs.update(transform=ax2.transAxes)  # switch to the bottom axes
    # ax2.plot((-d, +d), (1 - d, 1 + d), **kwargs)  # bottom-left diagonal
    # ax2.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)  # bottom-right diagonal
    boxprops = dict(color="black",linewidth=1.5)
    medianprops = dict(color='black',linewidth=1.5)
    capprops = dict(color="black",linewidth=1.5)
    whiskerprops = dict(color="black",linewidth=1.5)
    #bplot1 = ax1.boxplot([lol_no_bess,[], [], [], [], [], []], patch_artist=True, 
    #             boxprops=boxprops, medianprops=medianprops, capprops=capprops, whiskerprops=whiskerprops,
    #                 showfliers=False)
    #bplot1['boxes'][0].set_facecolor(colorset[0])
    bplot2 = ax1.boxplot([lol_no_bess,lol_pf_1, lol_mpc_1, lol_pf_3, lol_mpc_3, lol_pf_4, lol_mpc_4], patch_artist=True, 
                 boxprops=boxprops, medianprops=medianprops, capprops=capprops, whiskerprops=whiskerprops,
                    labels=['No BESS', 'Indiv. PF', 'Indiv. MPC' ,'Joint PF', 'Joint MPC','Hybrid PF', 'Hybrid MPC'], showfliers=False)
    for patch, color in zip(bplot2['boxes'], colorset[:7]):
        patch.set_facecolor(color)
    
    ax1.set_xlabel('January', fontsize=15)
    
    fig.text(0.03, 0.5, '% Loss of Life', fontsize=15, va='center', rotation='vertical')
    #ax2.set_ylim(0, 0.5)
    #ax1.set_ylim(0.01, 6)
    #ax1.xaxis.set_tick_params(labeltop=False)
    
    ax1.set_xticklabels(labels=ax1.get_xticklabels(), fontsize=14)
    #ax2.set_yticklabels(labels=ax2.get_yticklabels(), fontsize=14)
    ax1.set_yticklabels(labels=ax1.get_yticklabels(), fontsize=14)
    

    plt.savefig('Figures/8_case_all_jan.pdf', bbox_inches='tight')
    plt.savefig('Figures/8_case_all_jan.png', bbox_inches='tight')
    plt.show()
    return

def run_transformer_algo(data, experiment_file, trial, case, start_jan=False):
    if start_jan==False:
        start = datetime.datetime(2018, 7, 2, 0 ,0)
        mpc_problem = MPC(data, experiment_file)
        path = 'Code/Results/case_' + str(case)
        
        if case==1:
            #meter file includes battery
            meter_pf = np.sum(np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.loadtxt(path  + '/f1/batt_output_d_1_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) + np.sum(np.loadtxt(path  + '/f1/batt_output_d_1_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1)

            meter_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_1_mpc_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.loadtxt(path +  '/f1/batt_output_d_1_mpc_forecast_0_trial_'+str(trial)+'.csv'), axis=1)
            no_bess_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_1_mpc_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) + np.sum(np.loadtxt(path +  '/f1/batt_output_d_1_mpc_forecast_0_trial_'+str(trial)+'.csv'), axis=1)


        elif case==3:
            #meter PF file does not include battery
            meter_pf = np.sum(np.loadtxt(path  + '/meter_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) - np.sum(np.loadtxt(path  + '/batt_output_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/meter_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) 
            no_bess_mpc = np.sum(np.loadtxt(path  + '/meter_d_3_mpc_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) 

            meter_mpc = np.sum(np.loadtxt(path + '/meter_d_3_mpc_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.loadtxt(path +  '/batt_output_d_3_mpc_forecast_0_trial_'+str(trial)+'.csv'), axis=1)



        elif case==4:
            #meter file includes battery
            meter_pf = np.sum(np.loadtxt(path  + '/meter/meter_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1)- np.sum(np.loadtxt(path  + '/f1/batt_output_d_4_ev_forecast_0_trial_'+str(trial)+'.csv')[:1344], axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/meter/meter_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1)

            meter_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_4_mpc_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.loadtxt(path +  '/f1/batt_output_d_1_mpc_forecast_0_trial_'+str(trial)+'.csv'), axis=1)

    else:
        start = datetime.datetime(2018, 1, 8, 0 ,0)
        mpc_problem = MPC(data, experiment_file)
        path = 'Code/Results/case_' + str(case)

            
        if case==1:
            #meter file includes battery
            meter_pf = np.sum(np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.l_0_trial_'+str(trial)+'.csv'), axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) + np.sum(np.loadtxt(path  + '/f1/batt_output_d_1_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1)

            meter_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_1_mpc_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) #- np.sum(np.loaial)+'.csv'), axis=1)
            no_bess_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_1_mpc_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) + np.sum(np.loadtxt(path +  '/f1/batt_output_d_1_mpc_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1)



        elif case==3:
            #meter PF file does not include battery
            meter_pf = np.sum(np.loadtxt(path  + '/meter_d_3_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) - np.sum(np.loadtxt(path  + '/batt_output_d_3_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/meter_d_3_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) 
            no_bess_mpc = np.sum(np.loadtxt(path  + '/meter_d_3_mpc_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) 

            meter_mpc = np.sum(np.loadtxt(path + '/meter_d_3_mpc_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) #



        elif case==4:
            #meter file includes battery
            meter_pf = np.sum(np.loadtxt(path  + '/meter/meter_d_3_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1)- np.sum(np.loadtxt(path  + '/f1/batt_output_d_4_ev_forecast_0_jan_trial_'+str(trial)+'.csv')[:1344], axis=1)
            no_bess = np.sum(np.loadtxt(path  + '/meter/meter_d_3_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1)

            meter_mpc = np.sum(np.loadtxt(path + '/f1/meter_d_4_mpc_ev_forecast_0_jan_trial_'+str(trial)+'.csv'), axis=1) #-t_0_trial_'+str(trial)+'.csv'), axis=1)

        
        # fig, axes = plt.subplots(figsize=(10, 6))
        # axes.plot(no_bess, label='No BESS_pf')
        # #axes.plot(no_bess_mpc, label='No BESS mpc')
        # axes.plot(meter_pf , label='PF')
        # axes.plot(meter_mpc, label='MPC')
        # #axes.plot(np.sum(np.loadtxt(path  + '/batt_output_d_3_ev_forecast_0_trial_'+str(trial)+'.csv'), axis=1), label='pf')
        # #axes.plot(mpc_problem.data_list[0].loc[idxs, 'loads'], label='Loads')
        # #axes.plot(np.sum(np.loadtxt(path  + '/batt_output_d_3_mpc_forecast_0_trial_'+str(trial)+'.csv'), axis=1), label='mpc')
        # plt.legend()
        # plt.show()

    #idxs=[]
    #for j in range(1344): #range(meter_pf.shape[0]):
    #    idxs.append(np.where(mpc_problem.data_list[0].loc[:, 'datetime']==start + datetime.timedelta(hours = j * mpc_problem.dt))[0][0])
    
    # no_bess = np.zeros((len(mpc_problem.data_list[0].loc[idxs, 'loads']),))
    # for home in range(mpc_problem.num_homes):
    #     no_bess += mpc_problem.data_list[home].loc[idxs, 'loads']
    #     no_bess -= mpc_problem.data_list[home].loc[idxs, 'solar']

    # fig, axes = plt.subplots(figsize=(10, 6))
    # axes.plot(no_bess, label='No BESS')
    # #axes.plot(no_bess_mpc, label='No BESS mpc')
    # axes.plot(meter_pf , label='PF')
    # #axes.plot(meter_mpc, label='MPC')
    # #axes.plot(mpc_problem.data_list[0].loc[idxs, 'loads'], label='Loads')
    # plt.legend()
    # plt.show()

    t_no_bess = transformer(no_bess)
    t_pf = transformer(meter_pf)
    t_mpc = transformer(meter_mpc)
   
    lol_no_bess, theta_no_bess = t_no_bess.calc_aging(no_bess)
    lol_pf, theta_pf = t_pf.calc_aging(meter_pf)
    lol_mpc, theta_mpc = t_mpc.calc_aging(meter_mpc)

    #plot(mpc_problem.dt, lol_no_bess, theta_no_bess, lol_pf, theta_pf, lol_mpc, theta_mpc, start, mpc_problem, case)

    #return lol_no_bess[-1], lol_pf[-1], lol_mpc[-1]
    return lol_no_bess, theta_no_bess, lol_pf, theta_pf, lol_mpc, theta_mpc, start, mpc_problem


def fig_3():
    #July, case 1
    case=1
    max_trials = 1
    experiment_file = "Code/Experiments/case1.json" 
    random.seed(50)
    july_folder_path = "Data_L2/csv/july/"
    july_files = os.listdir(july_folder_path)
    july_files = ["{}{}".format(july_folder_path, july_file) for july_file in july_files]


    lol_no_bess=np.zeros((max_trials))
    lol_pf=np.zeros((max_trials))
    lol_mpc=np.zeros((max_trials))

    
    for trial in range(1,max_trials+1):
        home_filenames = getFiles(july_files, trial)
        print(home_filenames)
        data = []
        for file in home_filenames[0]:
            file = july_folder_path + file
            data.append(process_data(file, dt=1))
        lol_no_bess, theta_no_bess, lol_pf, theta_pf, lol_mpc, theta_mpc, start, mpc_problem = run_transformer_algo(data, experiment_file, trial-1, case)
    
    delta_t=.5
    
    plot_fig_3(delta_t, lol_no_bess, theta_no_bess, lol_pf, theta_pf, lol_mpc, theta_mpc, start, mpc_problem, case)

def fig_4():
    #July, case 1
    case=1
    num_timesteps = int(24/0.5 * 7 * 4)
    max_trials = 50
    experiment_file = "Code/Experiments/case1.json" 
    random.seed(50)
    july_folder_path = "Data_L2/csv/july/"
    july_files = os.listdir(july_folder_path)
    july_files = ["{}{}".format(july_folder_path, july_file) for july_file in july_files]

    num_homes_stat = []
    path = 'Code/Results/case_' + str(case)
    for trial in range(1,max_trials+1):
        meter_pf = np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_trial_'+str(trial)+'.csv')
        num_homes_stat.append(meter_pf.shape[1])
    print('Mean homes', np.mean(num_homes_stat))
    print('Standard dev homes', np.std(num_homes_stat))

    lol_no_bess=np.zeros((max_trials, num_timesteps))
    lol_pf_1=np.zeros((max_trials, num_timesteps))
    lol_mpc_1=np.zeros((max_trials, num_timesteps))

    home_filenames = getFiles(july_files, trial=1)
    data = []
    #for file in home_filenames[0]:
        #file = july_folder_path + file
    file = july_folder_path + home_filenames[0][0]
    data.append(process_data(file, dt=1))

    for trial in range(1,max_trials+1):
        # home_filenames = getFiles(july_files, trial)
        # data = []
        # #for file in home_filenames[0]:
        # file = july_folder_path + home_filenames[0][0]
        # #file = july_folder_path + file
        # data.append(process_data(file, dt=1))
        lol_no_bess[trial-1], x, lol_pf_1[trial-1], x, lol_mpc_1[trial-1] = run_transformer_algo(data, experiment_file, trial, case)[:5]

    #July, case 3 joint
    case=3   

    experiment_file = "Code/Experiments/case3.json" 
    random.seed(50)
    path = 'Code/Results/case_' + str(case)
    lol_pf_3=np.zeros((max_trials, num_timesteps))
    lol_mpc_3=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        # home_filenames = getFiles(july_files, trial)
        # data = []
        # #for file in home_filenames[0]:
        # file = july_folder_path + home_filenames[0][0]
        # #file = july_folder_path + file
        # data.append(process_data(file, dt=1))
        lol_pf_3[trial-1], x, lol_mpc_3[trial-1] = run_transformer_algo(data, experiment_file, trial, case)[2:5]
    
    plot_fig_4(lol_no_bess[:,-1], lol_pf_1[:,-1], lol_mpc_1[:,-1], lol_pf_3[:,-1], lol_mpc_3[:,-1])


def fig_6():
    #July, case 1
    case=1
    num_timesteps = int(24/0.5 * 7 * 4)
    max_trials = 50
    experiment_file = "Code/Experiments/case1.json" 
    random.seed(50)
    july_folder_path = "Data_L2/csv/july/"
    july_files = os.listdir(july_folder_path)
    july_files = ["{}{}".format(july_folder_path, july_file) for july_file in july_files]

    num_homes_stat = []
    path = 'Code/Results/case_' + str(case)
    for trial in range(1,max_trials+1):
        meter_pf = np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_trial_'+str(trial)+'.csv')
        num_homes_stat.append(meter_pf.shape[1])
    print('Mean homes', np.mean(num_homes_stat))
    print('Standard dev homes', np.std(num_homes_stat))

    lol_no_bess=np.zeros((max_trials, num_timesteps))
    lol_pf_1=np.zeros((max_trials, num_timesteps))
    lol_mpc_1=np.zeros((max_trials, num_timesteps))

    home_filenames = getFiles(july_files, trial=1)
    data = []
    #for file in home_filenames[0]:
        #file = july_folder_path + file
    file = july_folder_path + home_filenames[0][0]
    data.append(process_data(file, dt=1))
    for trial in range(1,max_trials+1):
        
        lol_pf_1[trial-1], x, lol_mpc_1[trial-1] = run_transformer_algo(data, experiment_file, trial, case)[2:5]

    #July, case 3 joint
    case=3   
    experiment_file = "Code/Experiments/case3.json" 
    random.seed(50)
    path = 'Code/Results/case_' + str(case)
    lol_pf_3=np.zeros((max_trials, num_timesteps))
    lol_mpc_3=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        #home_filenames = getFiles(july_files, trial)
        #data = []
        #for file in home_filenames[0]:
        #file = july_folder_path + home_filenames[0][0]
            #file = july_folder_path + file
            #data.append(process_data(file, dt=1))
        lol_pf_3[trial-1], x, lol_mpc_3[trial-1] = run_transformer_algo(data, experiment_file, trial, case)[2:5]

    #July, case 4 hybrid
    case=4   
    experiment_file = "Code/Experiments/case4.json" 
    random.seed(50)
    path = 'Code/Results/case_' + str(case)
    lol_pf_4=np.zeros((max_trials, num_timesteps))
    lol_mpc_4=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        #home_filenames = getFiles(july_files, trial)
        #data = []
        #for file in home_filenames[0]:
        #    file = july_folder_path + file
        #file = july_folder_path + home_filenames[0][0]
        #data.append(process_data(file, dt=1))
        lol_pf_4[trial-1], x, lol_mpc_4[trial-1] = run_transformer_algo(data, experiment_file, trial, case)[2:5]
    
    plot_fig_6(lol_pf_1[:,-1], lol_pf_3[:,-1], lol_pf_4[:,-1])

def fig_8():
    #Jan, case 1
    case=1
    num_timesteps = int(24/0.5 * 7 * 4)
    max_trials = 50
    experiment_file = "Code/Experiments/case1.json" 
    random.seed(50)
    jan_folder_path = "Data_L2/csv/july/"
    jan_files = os.listdir(jan_folder_path)
    jan_files = ["{}{}".format(jan_folder_path, jan_file) for jan_file in jan_files]

    num_homes_stat = []
    path = 'Code/Results/case_' + str(case)

    home_filenames = getFiles(jan_files, trial=1)
    data = []
    #for file in home_filenames[0]:
        #file = july_folder_path + file
    file = jan_folder_path + home_filenames[0][0]
    data.append(process_data(file, dt=1))
    for trial in range(1,max_trials+1):
        meter_pf = np.loadtxt(path  + '/f1/meter_d_1_ev_forecast_0_trial_'+str(trial)+'.csv')
        num_homes_stat.append(meter_pf.shape[1])
    print('Mean homes', np.mean(num_homes_stat))
    print('Standard dev homes', np.std(num_homes_stat))

    lol_no_bess=np.zeros((max_trials, num_timesteps))
    lol_pf_1=np.zeros((max_trials, num_timesteps))
    lol_mpc_1=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        # home_filenames = getFiles(jan_files, trial)
        # data = []
        # #for file in home_filenames[0]:
        #     #file = july_folder_path + file
        # file = jan_folder_path + home_filenames[0][0]
        # data.append(process_data(file, dt=1))
        lol_no_bess[trial-1], x, lol_pf_1[trial-1], x, lol_mpc_1[trial-1]  = run_transformer_algo(data, experiment_file, trial, case, start_jan=True)[:5]

    #July, case 3 joint
    case=3   
    experiment_file = "Code/Experiments/case3.json" 
    random.seed(50)
    path = 'Code/Results/case_' + str(case)
    lol_pf_3=np.zeros((max_trials, num_timesteps))
    lol_mpc_3=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        # home_filenames = getFiles(jan_files, trial)
        # data = []
        # #for file in home_filenames[0]:
        # file = jan_folder_path + home_filenames[0][0]
            #file = july_folder_path + file
            #data.append(process_data(file, dt=1))
        lol_pf_3[trial-1], x, lol_mpc_3[trial-1] = run_transformer_algo(data, experiment_file, trial, case,start_jan=True)[2:5]

    #July, case 4 hybrid
    case=4   
    experiment_file = "Code/Experiments/case4.json" 
    random.seed(50)
    path = 'Code/Results/case_' + str(case)
    lol_pf_4=np.zeros((max_trials, num_timesteps))
    lol_mpc_4=np.zeros((max_trials, num_timesteps))

    for trial in range(1,max_trials+1):
        # home_filenames = getFiles(jan_files, trial)
        # data = []
        # #for file in home_filenames[0]:
        # #    file = july_folder_path + file
        # file = jan_folder_path + home_filenames[0][0]
        # data.append(process_data(file, dt=1))
        lol_pf_4[trial-1], x, lol_mpc_4[trial-1] = run_transformer_algo(data, experiment_file, trial, case, start_jan = True)[2:5]
    
    plot_fig_8(lol_no_bess[:,-1], lol_pf_1[:,-1], lol_mpc_1[:, -1], lol_pf_3[:,-1], lol_mpc_3[:,-1], lol_pf_4[:,-1], lol_mpc_4[:,-1])
    

def main():
    
    #fig_3()
    #fig_4()
    fig_6()
    fig_8()





if __name__ == '__main__':
    main()


    


    
