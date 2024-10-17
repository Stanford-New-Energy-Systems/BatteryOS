import sys

def calculate_mean(avgs, n, total_batteries = 5):
    probs = [None] * total_batteries
    increment = 1 / total_batteries

    currProb = increment
    prevProb = 0 
    for i in range(total_batteries):
        probs[i] = (currProb ** n) - (prevProb ** n)
        prevProb = currProb
        currProb += increment
	
    return sum([avgs[i] * probs[i] for i in range(total_batteries)])
	

if __name__ == "__main__":
    avgs = [10.767, 13.751, 19.600, 31.724, 19.195]
    avgs.sort()
	
#	if len(sys.argv) != 2:
#		sys.exit("python3 stats.py battery_number") 	
	
    with open("ping_latencies.csv", 'w') as file:
        file.write("Number of Batteries,Expected Latency\n");
        for i in range(1, 1001):
            file.write("{},{}\n".format(str(i), str(calculate_mean(avgs, i))))


