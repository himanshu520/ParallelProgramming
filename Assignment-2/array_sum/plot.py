import matplotlib.pyplot as plt

global_sum = int(input())
array_size = int(input())
MAX_FUNCTIONS = int(input())
MAX_THREADS = int(input())

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']    

for function_no in range(MAX_FUNCTIONS):
    function_name = input()
    running_time = []
    for thread_no in range(MAX_THREADS):
        running_time.append(float(input()))
    plt.plot(range(1, MAX_THREADS + 1), running_time, colors[function_no], label=function_name)

plt.xlabel("No of threads")
plt.ylabel("Running time (seconds)")
plt.legend()
plt.show()