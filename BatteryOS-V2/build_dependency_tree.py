#!/usr/bin/env python3

import re 
import os
import sys
import networkx as nx
import matplotlib.pyplot as plt

def parseLine(line):
    if "#include" in line and '"' in line:
        line = line.split('"')
        return line[1]
    return ''

def findDependencies(filename):
    with open(filename, 'r') as file:
        dependencies = list(map(parseLine, file.readlines()))
        dependencies = [x for x in dependencies if x != '']
   
    if not dependencies:
        return []
     
    return [(x, filename) for x in dependencies]

if __name__ == "__main__":
    visited = set()
    graph   = nx.DiGraph()

    if len(sys.argv) != 2:
        sys.exit("python3 build_dependency_graph.py filename")
    
    visited.add(sys.argv[1])
    dependencies = findDependencies(sys.argv[1])
    tempList = [x for x in dependencies]

    print(tempList)
   
    while True:
        currDependencies = [findDependencies(x[0]) for x in tempList if x[0] not in visited]
        currDependencies = [element for sublist in currDependencies for element in sublist]
        
        for dependency in tempList:
            print("Dependency[0] = ", dependency[0])
            visited.add(dependency[0])

        if not currDependencies:
            break

        for x in currDependencies:
            dependencies.append(x)

        tempList.clear()
        tempList = [x for x in currDependencies]         
    
    graph.add_edges_from(dependencies)    
    
    if nx.is_directed_acyclic_graph(graph):
        print(list(nx.topological_sort(graph))) 
        
    #print(dependencies)
    #plt.tight_layout()
    #nx.draw_networkx(graph, arrows=True)
    #plt.savefig("graph.png", format="PNG")
    #plt.clf()    
