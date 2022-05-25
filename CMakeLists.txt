cmake_minimum_required(VERSION 3.22)
project(LAB6 C)

set(CMAKE_C_STANDARD 99)


add_executable(mypipe task0/a/mypipe.c)
add_executable(mypipeline task0/b/mypipeline.c task0/b/LineParser.c)
add_executable(myShell task1/myshell.c task1/LineParser.c)
add_executable(myShell2 task2/myshell.c task2/LineParser.c)
add_executable(myShell3 task3/myshell.c task3/LineParser.c)

