with open("times.txt") as f:
    content = f.readlines();
content = [float(i) for i in content]
print("Max time: ")
print(max(content))
print("time between sending and receiving: ")
print(max(content) - 301)
print("Average: ")
print(sum(content)/len(content))
