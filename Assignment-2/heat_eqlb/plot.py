import matplotlib.pyplot as plt

input_file_name = input().split(".")[0]
array_size = int(input())
no_of_iterations = int(input())
MAX_FUNCTIONS = int(input())

colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']    
function_names = []
running_time = []

for function_no in range(MAX_FUNCTIONS):
    function_names.append(input())
    running_time.append(float(input()))
    
plt.bar(range(1, MAX_FUNCTIONS + 1), running_time)
plt.title(input_file_name + "  (array size = " + str(array_size) + ", no of iterations = " + str(no_of_iterations) + ")" )
plt.xticks(range(1, MAX_FUNCTIONS + 1), function_names)
plt.ylabel("Running time (seconds)")
plt.savefig(input_file_name + ".png")
plt.show()