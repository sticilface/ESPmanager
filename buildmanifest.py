#! /usr/bin/env python
import hashlib
import os
import json
import sys

def md5(fname):
	hash_md5 = hashlib.md5()
	with open(fname, "rb") as f:
		for chunk in iter(lambda: f.read(4096), b""):
			hash_md5.update(chunk)
	return hash_md5.hexdigest()


# run py buildmanifest.py pathtobin
# place SPIFFS files in pathtobin/data/
# you can copy the bin to the working sketch directory, and leave data as is.  

# available commands
# formatSPIFFS
# clearWiFi
# rooturi

# repo = Repo(os.getcwd())
# branch = repo.active_branch
# branch = branch.name


data = {}
data["files"] = {}


List = []

index = 0
a = 0
# Set the directory you want to start from..  all stuff for SPIFFS is in the ./data directory..

print("Python Start")

rootDir = sys.argv[1] + "/data"

for dirName, subdirList, fileList in os.walk(rootDir):
    if not dirName.startswith('.'):
        for fname in fileList:
            if not fname.startswith('.'):
                if not fname.endswith(".bin"):
                    relPath = os.path.relpath( dirName + "/" + fname, rootDir)
                    locPath = os.path.relpath( dirName + "/" + fname, sys.argv[1])
                    print("RelPath = " + relPath)
                    item = {}
                    item["index"] = index
                    index = index + 1
                    item["location"] =  "/" + locPath
                    item["isurl"] = False
                    item["md5"] = md5(dirName + "/" + fname)
                    item["saveto"] = "/" + relPath
                    List.append(item)
                else:
                    print(".bin = " + fname)
                    if fname == "firmware.bin":
                        index = index + 1
                        print("binary hit:" + dirName + "/" + fname + "(" + md5(dirName + "/" + fname) + ")")
                        binary = {}
                        binary["index"] = index
                        binary["location"] = "/data/firmware.bin"
                        binary["saveto"] = "sketch"
                        binary["md5"] = md5(dirName + "/" + fname)
                        List.append(binary)

data["files"] = List
data["filecount"] = index

# print(json.dumps(data, sort_keys=False, indent=4))
with open(sys.argv[2], 'w') as outfile:
	json.dump(data, outfile)

exit(0)
# json_data = json.dumps(data)
 # print(List)
#print '[%s]' % ', '.join(map(str, List))
