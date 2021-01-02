# dilog
A header-only C++ diagnostic class for finding the point of divergence between two runs of an application
that should produce identical results each time it runs, but does not.

## Description
A standard expectation of data processing applications is that repeated runs with the same input data, using
the same hardware and the same binaries, should produce the same output. Surprisingly, in complex applications
this a frequently not the case, and fixing it so that it does provides a useful constraint on the correctness
of the algorithm. Divergence happens for a variety of reasons. If the code is multi-threaded, there might be a
race condition between the threads that accounts for the difference. If there is just a single processing thread,
the reason might be related to differences in the addresses that are assigned to objects allocated on the heap,
which can vary from one run to the next. Whatever the source, experience teaches that you ignore this at your
own peril. Such divergences almost always reveal some unintended interaction between bits of your application
that should be independent from each other, but are not. I wrote this tool to help me quickly track down the
point in the execution of the application where the divergence takes place.

## Design
To use dilog, you need to be able to recompile your application from sources, at least the components where
the divergence is happening. You need to go into the C++ source and insert a dilog message at the relevant
points that mark the progress of the execution stream. The dilog message is any plain text string that will
indicate what is happening in the execution stream at that point. Rebuild the application and run it multiple
times over the same input data. The first time its runs, one or more dilog files (see below for the names)
will be written into the working directory. Every time after that, instead of writing new dilog output files,
it will read the messages from the exiting dilog files and compare with the message stream from the running
program. As soon as any divergence is found, a runtime exception is generated with a message indicating the
point where the divengence first appears. One can further instrument the application code with an exception
handler and print out more information from the runtime context at the point of divergence.

## Usage
The only thing you need to do is to include the dilog::printf statements at the relevant places in your
code. No object allocation is needed at program/thread startup, nor is any explicit cleanup required at
exit.

    `#include <dilog.h> 
    ...
    dilog::printf("myapp", "looking at sheep %d in herd %s\n", isheep, herd);
    ...`

## Execution threads and blocks
The main problem to be overcome in the implementation of dilog is to avoid false reports of divergence
coming from reordering of loops and arbitrary ordering of output from threads. To help dilog recognize
and eliminate these false positives, the user must insert special markers into the dilog message stream
using the `dilog::block_begin()` and `dilog::block_end()` messages. Each block should be tagged by a
unique name so that it can be distinguished from other blocks. The same mechanism is used both for
unordered loops and for threads. Any messages that are emitted within a block are required to be exactly
the same and in the same order to avoid being flagged, but the order of the blocks is arbitrary.
Blocks can be nested to arbitrary order.

Here is an example of a loop over a std::map with a pointer for its key, illustrating how the
`block_begin` and `block_end` messages are used.

    `#include <dilog.h> 
    ...
    std::map<*farm, std::vector<sheep> > herds;
    ...
    for (auto herd : herds) {
        dilog::block_begin("myapp", "loop over farms in herds");
        ...
        for (int isheep=0; isheep < herd.second.size(); ++isheep) {
           ...
           dilog::printf("myapp", "looking at sheep %d in herd %s\n", isheep, herd.first->name);
           ...
        }
        dilog::block_end("myapp", "loop over farms in herds");
     }
    ...`

The above dilog instrumentation of the application code recognizes that the processing order of the
herds container elements will vary from one run to the next, but the order of sheep within each herd
is expected to be invariant.

## Test conditions
I developed and tested this initial release of the code with g++ under gcc 4.8.5, but it should work with
any of the more recent gcc releases. Multithreading support requires -std=c++11 in order to use std::mutex.

## Bugs, comments, and suggestions
To communicate with the author, please use the address richard.t.jones at uconn.edu.
