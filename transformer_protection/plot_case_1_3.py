import cvxpy as cp
import numpy as np
import matplotlib.pyplot as plt
import datetime

#plotting note!
# 0 blue, 1 pink, 2 green, 3 light blue, 4 gold
# 0 blue = base no bess
# 1 pink individual deterministic
# 2 green individual mpc
# 3 gold joint deterministic
# 4 light blue joint mpc

def plot_case_1_3(self, cost_list_3_no_bess, cost_list_mpc_1, cost_list_determ_1, cost_list_mpc_3, cost_list_determ_3,
                  filename = "price_bar"):
    homes_list = []
    for home in range(self.num_homes):
        homes_list.append('Home ' + str(home+1))

    # homes_list[5] += '*'
    # homes_list[6] += '*'


    # cost_list_3_no_bess = np.append(cost_list_3_no_bess[:self.num_homes-2], np.average(cost_list_3_no_bess[:self.num_homes]))
    # cost_list_determ_1 = np.append(cost_list_determ_1[:self.num_homes-2], np.average(cost_list_determ_1[:self.num_homes]))
    # cost_list_mpc_1_1 = np.append(cost_list_mpc_1[:self.num_homes-2], np.average(cost_list_mpc_1[:self.num_homes]))

    cost_list_3_no_bess = np.append(cost_list_3_no_bess[:self.num_homes], np.average(cost_list_3_no_bess[:self.num_homes]))
    cost_list_determ_1 = np.append(cost_list_determ_1[:self.num_homes], np.average(cost_list_determ_1[:self.num_homes]))
    cost_list_mpc_1_1 = np.append(cost_list_mpc_1[:self.num_homes], np.average(cost_list_mpc_1[:self.num_homes]))

    #cost_list_mpc_1_2 = np.append(cost_list_mpc_1[self.num_homes:self.num_homes*2-2], np.average(cost_list_mpc_1[self.num_homes:]))

    # cost_list_determ_3 = np.append(cost_list_determ_3[:self.num_homes-2], np.average(cost_list_determ_3[:self.num_homes]))
    # cost_list_mpc_3_1 = np.append(cost_list_mpc_3[:self.num_homes-2], np.average(cost_list_mpc_3[:self.num_homes]))

    cost_list_determ_3 = np.append(cost_list_determ_3[:self.num_homes], np.average(cost_list_determ_3[:self.num_homes]))
    cost_list_mpc_3_1 = np.append(cost_list_mpc_3[:self.num_homes], np.average(cost_list_mpc_3[:self.num_homes]))

    #cost_list_mpc_3_2 = np.append(cost_list_mpc_3[self.num_homes:self.num_homes*2-2], np.average(cost_list_mpc_3[self.num_homes:]))
  

    control_types = {
        'No Battery': cost_list_3_no_bess, 
        'Individual MPC': cost_list_mpc_1_1,
        'Individual PF': cost_list_determ_1,
       # 'Individual MPC - Conservative Forecast': cost_list_mpc_1_2,
    }

    control_types_1 = {
        'No Battery': cost_list_3_no_bess,
        'Hybrid MPC': cost_list_mpc_3_1,
        'Hybrid PF': cost_list_determ_3,

        #'Joint MPC - Conservative Forecast': cost_list_mpc_3_2,
    }

    homes_list.append('Average')
    x = np.arange(len(homes_list))  # the label locations

    width = 0.25  # the width of the bars
    multiplier = 0

    fig, ax = plt.subplots(1,1, figsize=(16,3))
    colorset_1 = [self.colorset[0] , self.colorset[1] , self.colorset[2] , self.colorset[5] ]
    colorset_2 = [self.colorset[0] , self.colorset[3] , self.colorset[4] , self.colorset[6] ]
    
    for attribute, measurement in control_types.items():
        offset = width * multiplier
        rects = ax.bar(x + offset, measurement, width, label=attribute, color = colorset_1[multiplier])
        ax.bar_label(rects, labels=np.round(measurement,2), fontsize=10.5, padding=3)
        multiplier += 1
    



    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Cost [$]', fontsize = 14)
    ax.tick_params(axis='y', which='major', labelsize=11)
    ax.tick_params(axis='x', which='major', labelsize=12)
    ax.set_xticks(x + width , homes_list)
    ax.legend(ncols=2, fontsize=10.5, loc='upper right')
    ax.set_ylim(0, max(cost_list_3_no_bess)*1.13)

    plt.savefig("Figures/{}.pdf".format(filename), bbox_inches='tight')
    plt.savefig("Figures/{}.png".format(filename), bbox_inches='tight')
    # plt.savefig('Figures/price_bar.pdf', bbox_inches='tight')
    # plt.savefig('Figures/price_bar.png', bbox_inches='tight')
    # plt.show()


    fig, ax = plt.subplots(1,1, figsize=(16,3))
    multiplier = 0
    for attribute, measurement in control_types_1.items():
        offset = width * multiplier
        rects = ax.bar(x + offset, measurement, width, label=attribute, color = colorset_2[multiplier])
        ax.bar_label(rects, labels=np.round(measurement,2), fontsize=10.5, padding=3)
        multiplier += 1
    


    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Cost [$]', fontsize = 14)
    ax.tick_params(axis='y', which='major', labelsize=11)
    ax.tick_params(axis='x', which='major', labelsize=12)
    ax.set_xticks(x + width , homes_list)
    ax.legend(ncols=2, fontsize=10.5, loc='upper right')
    ax.set_ylim(0, max(cost_list_3_no_bess)*1.13)

    plt.savefig('Figures/{}_2.pdf'.format(filename), bbox_inches='tight')
    plt.savefig('Figures/{}_2.png'.format(filename), bbox_inches='tight')
    # plt.savefig('Figures/price_bar_2.pdf', bbox_inches='tight')
    # plt.savefig('Figures/price_bar_2.png', bbox_inches='tight')
    # plt.show()
    return
