#!/usr/bin/python

fileName = "gzip.h"

file = open(fileName)
writeStruct = open("StructName.txt","r+")
writeVar = open("VarName.txt","r+")


word = []
brace = []
structName = []
varName = []

def StructName(brace, lastLine, line):
    while len(brace) > 0:
        lastLine = line
        line = file.readline()
        i = 0
        while i < line.count("{"):
            brace.insert(0, "{")
            i += 1
        i = 0
        while i < line.count("}"):
            brace = brace[:-1]
            i += 1
    word = line.split()
    for item in word:
        if ";" in item:
            structName.insert(0, item[:-1])

while 1:
    line = file.readline()
    lastLine = line
    if not line:
        break
    else:
        while line and line[0] == '#' and fileName in line:
            lastLine = line
            line = file.readline()
            while line and line[0] != '#':
                word = line.split()
                if len(word) > 0:
                    if "{" in line:
                        if "struct" in line:
                            if "typedef" in line:
                                if "extern" in line:
                                    if len(word) > 3:
                                        if "{" in word[3] and len(word[3]) > 1:
                                            structName.insert(0, word[3][:-1])
                                        elif len(word) > 4 and "{" in word[4]:
                                            structName.insert(0, word[3])
                                        else:
                                            brace.insert(0, "{")
                                            StructName(brace, lastLine, line)
                                            brace = brace[:-1]
                                    else:
                                        brace.insert(0, "{")
                                        StructName(brace, lastLine, line)
                                        brace = brace[:-1]
                                elif len(word) > 2:
                                    if "{" in word[2] and len(word[2]) > 1:
                                        structName.insert(0, word[2][:-1])
                                    elif len(word) > 3 and "{" in word[3]:
                                        structName.insert(0, word[2])
                                    else:
                                        brace.insert(0, "{")
                                        StructName(brace, lastLine, line)
                                        brace = brace[:-1]
                                else:
                                    brace.insert(0, "{")
                                    StructName(brace, lastLine, line)
                                    brace = brace[:-1]
                            else:
                                if len(word) > 2:
                                    structName.insert(0, word[1])
                                elif len(word) == 2 and len(word[1][:-1]) > 0:
                                    structName.insert(0, word[1][:-1])
                                else:
                                    brace.insert(0, "{")
                                    while len(brace) > 0:
                                        lastLine = line
                                        line = file.readline()
                                        i = 0
                                        while i < line.count("{"):
                                            brace.insert(0, "{")
                                            i += 1
                                        i = 0
                                        while i < line.count("}"):
                                            brace = brace[:-1]
                                            i += 1
                                    word = line.split()
                                    for item in word:
                                        if ";" in item:
                                            if "[" in item:
                                                item = item.split("[")
                                                varName.insert(0, item[0])
                                            else:
                                                varName.insert(0, item[:-1])
                        else:
                            brace.insert(0, "{")
                            while len(brace) > 0:
                                lastLine = line
                                line = file.readline()
                                i = 0
                                while i < line.count("{"):
                                    brace.insert(0, "{")
                                    i += 1
                                i = 0
                                while i < line.count("}"):
                                    brace = brace[:-1]
                                    i += 1
                    elif "struct" in line and "typedef" in line and ";" not in line:
                        lastLine = line
                        line = file.readline()
                        if "{" in line:
                            brace.insert(0, "{")
                            StructName(brace, lastLine, line)
                            brace = brace[:-1]
                    elif "extern" in line and "(" not in line:
                        if ";" in line and "{" not in line:
                            for item in word:
                                if ";" in item:
                                    if "[" in item:
                                        item = item.split("[")
                                        varName.insert(0, item[0])
                                    else:
                                        varName.insert(0, item[:-1])

                lastLine = line
                line = file.readline()

print "varName:"
print varName
print "structName:"
print structName
writeStruct.read()
for item in structName:
    writeStruct.writelines(item)
    writeStruct.writelines('\n')
writeStruct.close()

writeVar.read()
for item in varName:
    writeVar.writelines(item)
    writeVar.writelines('\n')
writeVar.close()