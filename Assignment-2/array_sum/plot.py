import matplotlib.pyplot as plt

input_file_name = input().split(".")[0]
global_sum = int(input())
array_size = int(input())
MAX_FUNCTIONS = int(input())
MAX_THREADS = int(input())

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']

plt.figure(1)
plt.figure(2)
plt.figure(3)

for function_no in range(MAX_FUNCTIONS):
    function_name = input()
    avg_running_time = []
    max_running_time = []
    min_running_time = []
    for thread_no in range(MAX_THREADS):
        avg_running_time.append(float(input()))
    for thread_no in range(MAX_THREADS):
        max_running_time.append(float(input()))
    for thread_no in range(MAX_THREADS):
        min_running_time.append(float(input()))
    plt.figure(1)
    plt.plot(range(1, MAX_THREADS + 1), avg_running_time, colors[function_no], label=function_name)
    plt.figure(2)
    plt.plot(range(1, MAX_THREADS + 1), max_running_time, colors[function_no], label=function_name)
    plt.figure(3)
    plt.plot(range(1, MAX_THREADS + 1), min_running_time, colors[function_no], label=function_name)

plt.figure(1)
plt.title(input_file_name + "  (array size = " + str(array_size) + ")" )
plt.xlabel("No of threads")
plt.ylabel("Average Running time (seconds)")
plt.legend()
plt.savefig(input_file_name + "_avg.png")

plt.figure(2)
plt.title(input_file_name + "  (array size = " + str(array_size) + ")" )
plt.xlabel("No of threads")
plt.ylabel("Maximum Running time (seconds)")
plt.legend()
plt.savefig(input_file_name + "_max.png")

plt.figure(3)
plt.title(input_file_name + "  (array size = " + str(array_size) + ")" )
plt.xlabel("No of threads")
plt.ylabel("Minimum Running time (seconds)")
plt.legend()
plt.savefig(input_file_name + "_min.png")

plt.show()