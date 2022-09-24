import sys

f = open("./project.aeng", "w")
f.write(sys.argv[1])
f.close()