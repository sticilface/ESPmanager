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

# Import the os module, for the os.walk function


# available commands
# formatSPIFFS
# clearWiFi
# rooturi


data = {}
data["Lib"] = os.environ.get('MY_NAME')
data["Version"] = os.environ.get('MY_VERSION')
data["URL"] = os.environ.get('MY_URL')
data["rooturi"] = os.environ.get('MY_ROOT')
data["author"] = os.environ.get('MY_AUTHOR')
data["files"] = {}
data["branch"] = os.environ.get('TRAVIS_BRANCH')
data["commit"] = os.environ.get('TRAVIS_COMMIT')
data["tag"] = os.environ.get('TRAVIS_TAG')
data["slug"] = os.environ.get('TRAVIS_REPO_SLUG')


List = []

index = 0; 

# Set the directory you want to start from


rootDir = sys.argv[1]
for dirName, subdirList, fileList in os.walk(rootDir):
	if not dirName.startswith('.'):
		for fname in fileList:
			if not fname.startswith('.'):
				relPath = os.path.relpath( dirName + "/" + fname, rootDir)
				# print("RelPath = " + relPath)
				item = {}
				item["index"] = index    		
				index = index + 1
				item["location"] = relPath
				item["isurl"] = False
				item["md5"] = md5(dirName + "/" + fname) 
				if not fname.endswith(".bin"):
					item["saveto"] = "/" + relPath   			    				 
				else:
					item["saveto"] = "sketch"
				List.append(item) 

data["files"] = List 

print(json.dumps(data, sort_keys=False, indent=4))
with open(sys.argv[2], 'w') as outfile:
	json.dump(data, outfile)

exit(0)
# json_data = json.dumps(data)
 # print(List)
#print '[%s]' % ', '.join(map(str, List))
