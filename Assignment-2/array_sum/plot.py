import matplotlib.pyplot as plt

input_file_name = input().split(".")[0]
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

plt.title(input_file_name + "  (array size = " + str(array_size) + ")" )
plt.xlabel("No of threads")
plt.ylabel("Running time (seconds)")
plt.legend()
plt.savefig(input_file_name + ".png")
plt.show()