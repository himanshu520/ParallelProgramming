import matplotlib.pyplot as plt

input_file_name = input().split(".")[0]
array_size = int(input())
no_of_iterations = int(input())
MAX_FUNCTIONS = int(input())

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']
function_names = []
avg_running_time = []
max_running_time = []
min_running_time = []

for function_no in range(MAX_FUNCTIONS):
    function_names.append(input())
    avg_running_time.append(float(input()))
    max_running_time.append(float(input()))
    min_running_time.append(float(input()))

plt.figure()    
plt.bar(range(1, MAX_FUNCTIONS + 1), avg_running_time)
plt.title(input_file_name + "  (array size = " + str(array_size) + ", no of iterations = " + str(no_of_iterations) + ")" )
plt.xticks(range(1, MAX_FUNCTIONS + 1), function_names)
plt.ylabel("Average Running time (seconds)")
plt.savefig(input_file_name + "_avg.png")

plt.figure()    
plt.bar(range(1, MAX_FUNCTIONS + 1), max_running_time)
plt.title(input_file_name + "  (array size = " + str(array_size) + ", no of iterations = " + str(no_of_iterations) + ")" )
plt.xticks(range(1, MAX_FUNCTIONS + 1), function_names)
plt.ylabel("Maximum Running time (seconds)")
plt.savefig(input_file_name + "_max.png")

plt.figure()    
plt.bar(range(1, MAX_FUNCTIONS + 1), avg_running_time)
plt.title(input_file_name + "  (array size = " + str(array_size) + ", no of iterations = " + str(no_of_iterations) + ")" )
plt.xticks(range(1, MAX_FUNCTIONS + 1), function_names)
plt.ylabel("Minimum Running time (seconds)")
plt.savefig(input_file_name + "_min.png")

plt.show()