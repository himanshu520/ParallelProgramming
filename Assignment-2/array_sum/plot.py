import matplotlib.pyplot as plt

open("data.txt", "r")

global_sum = int(input())
array_size = int(input())
MAX_FUNCTIONS = int(input())
MAX_THREADS = int(input())

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']    
running_time = []
function_names = []

for function_no in range(MAX_FUNCTIONS):
    function_names.append(input())
    running_time.append([])
    for thread_no in range(MAX_THREADS):
        running_time[function_no].append(input())
    plt.plot(range(1, MAX_FUNCTIONS + 1), running_time[function_no], colors[function_no], label=function_names[function_no])

plt.show()