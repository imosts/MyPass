#!/usr/bin/python

fileName = "main.c"

file = open(fileName)
writeStruct = open("StructName.txt","a+")
writeVar = open("VarName.txt","a+")


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

#TODO： 还有union 的名称没实现记录，并且在结构体定义中的union的名称需要特殊处理

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
                        brace.insert(0, "{")
                        if "struct" in line:
                            if "typedef" in line:
                                if "extern" in line:
                                    if len(word) > 3:
                                        if "{" in word[3] and len(word[3]) > 1:
                                            structName.insert(0, word[3][:-1])
                                        elif len(word) > 4 and "{" in word[4]:
                                            structName.insert(0, word[3])
                                        else:
                                            StructName(brace, lastLine, line)

                                    else:
                                        StructName(brace, lastLine, line)

                                elif len(word) > 2:
                                    if "{" in word[2] and len(word[2]) > 1:
                                        structName.insert(0, word[2][:-1])
                                    elif len(word) > 3 and "{" in word[3]:
                                        structName.insert(0, word[2])
                                    else:
                                        StructName(brace, lastLine, line)

                                else:
                                    StructName(brace, lastLine, line)

                            else:
                                if len(word) > 2:
                                    structName.insert(0, word[1])
                                elif len(word) == 2 and len(word[1][:-1]) > 0:
                                    structName.insert(0, word[1][:-1])
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
                        if len(brace) != 0:
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
                    elif len(brace) == 0 and len(word) == 2 and "(" not in line and ";" in line:
                        if len(word[1][:-1]) > 0:
                            item = word[1][:-1].split("[")
                            if "[" in word[1]:
                                item = word[1][:-1].split("[")
                                varName.insert(0, item[0])
                            else:
                                varName.insert(0, word[1][:-1])
                    elif len(brace) == 0 and len(word) == 3 and "(" not in line and ";" in line:
                        if len(word[2][:-1]) == 0:
                            item = word[1].split("[")
                            if "[" in word[1]:
                                item = word[1].split("[")
                                varName.insert(0, item[0])
                            else:
                                varName.insert(0, word[1])

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