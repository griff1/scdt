with open("times.txt") as f:
    content = f.readlines();
content = [float(i) for i in content]
print("Average: ")
print(sum(content)/len(content))
