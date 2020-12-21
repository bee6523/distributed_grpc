# Assignment 4

- Student ID: 20150686
- Your Name: Jeong Seonghoon
- Submission date and time: 2020/12/21, 17:00 pm

## Ethics Oath
I pledge that I have followed all the ethical rules required by this course (e.g., not browsing the code from a disallowed source, not sharing my own code with others, so on) while carrying out this assignment, and that I will not distribute my code to anyone related to this course even after submission of this assignment. I will assume full responsibility if any violation is found later.

Name: ___Jeong Seonghoon___
Date: ___2020/12/21___

## Brief description
Briefly describe here how you implemented the project.

At first tried to implement asynchronous single-thread server, but changed into Synchronous multi-threaded server.
However, some grpc requests(TranslateChunk in Childnode and Supernode) are asynchronous, therefore it can send multiple grpc to child in parellel.
caches are simply implemented using std::vector, replacement policy is LRU.

build order:
mkdir -p cmake/build
pushd ./cmake/build
cmake ../..
make -j
popd

stub files(assign4.grpc.pb.cc, assign4.grpc.pb.h, etc.) should be generated in cmake/build directory to properly included in source files.
binary files are also created in cmake/build directory(probably not necessary).
after process, result.txt should be created at main directory.
currently it suffers in handling large file. working towards it.

# Misc
Describe here whatever help (if any) you received from others while doing the assignment.
all the information from grpc.io document&reference,
googled about how to implement LRU cache.
some debug information from stackoverflow.

How difficult was the assignment? (1-5 scale)
4

How long did you take completing? (in hours)
more than 40 hours