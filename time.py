with open("times.txt") as f:
    content = f.readlines();
content = [float(i) for i in content]
print("Max time: ")
print(max(content))
print("time between sending and receiving: ")
print(max(content) - 301)
print("Average: ")
avglst = []
for item in content:
  if item != 0:
    avglst.append(item - 301)

print(sum(avglst)/len(avglst))
