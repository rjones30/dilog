# dilog
A header-only C++ diagnostic class for finding the point of divergence between two runs of an application
that should produce identical results each time it runs, but does not.

## Description
A standard expectation of data processing applications is that repeated runs with the same input data, using
the same hardware and the same binaries, should produce the same output. Surprisingly, in complex applications
this a frequently not the case, and making sure that that it does provides a useful constraint on the correctness
of the algorithm. Divergence happens for a variety of reasons. If the code is multi-threaded, there might be a
race condition between the threads that accounts for the difference. If there is just a single processing thread,
the reason might be related to differences in the addresses that are assigned to objects allocated on the heap,
which can vary from one run to the next. Whatever the source, experience teaches that you ignore this at your
own peril. Such divergences almost always reveal some unintended interaction between bits of your application
that should be independent from each other, but are not. I wrote this tool to help me quickly track down the
point in the execution of my applications where the divergence takes place.

## Design
To use dilog, you need to be able to recompile your application from sources, at least the components where
the divergence is happening. You need to go into the C++ source and insert a dilog message at the relevant
points that mark the progress of the execution stream. A dilog message is any plain text string that will
indicate what is happening in the execution stream at that point. Rebuild the application and run it multiple
times over the same input data. The first time its runs, one or more dilog output files (see below for the names)
will be written into the working directory. Every time after that, instead of writing new dilog output files,
it will read the messages from the exiting dilog files and compare with the message stream from the running
program. As soon as any divergence is found, a runtime exception is generated with a message indicating the
point where the divengence first appears. It is left to the user to further instrument the application code
with an exception handler in case more information is desired regarding the runtime context at the point
of divergence.

## Usage
The only thing you need to do is to include the dilog::printf statements at the relevant places in your
code. No object allocation is needed at program/thread startup, nor is any explicit cleanup required at
exit.

    #include <dilog.h> 
    ...
    dilog::get("sheepcounter").printf("looking at sheep %d in herd %s\n", isheep, herd);
    ...

Rebuild and run your application. After the first time, you will see a new output file sheepcounter.dilog
in your cwd. Run your application from the same data a second time and the data in myapp.dilog will be
used to check the execution for any divergences in the dilog message sequence that occur relative to
the first time it ran. A runtime exception will be generated as soon as a divergence is detected.

## Execution blocks
The main problem to be overcome in the implementation of dilog is to avoid false reports of divergence
coming from reordering of loops and arbitrary ordering threads execution. To help dilog recognize
and eliminate these false positives, the user must insert special markers into the dilog message stream
using the `dilog::block` constructor and destructor. Each block must be assigned to a dilog stream and
be given a unique name so that it can be distinguished from other blocks. A block of a given name begins
when the dilog::block is constructed and exits by the destructor. A block may be executed an arbitrary
number of times, but all iterations of a block must follow one after another with nothing in between,
and they cannot overlap. The same execution block mechanism is used to handle reordering issues both
for unordered loops and for threads. Because their execution can overlap, special considerations
should be used to handle blocks within threads that can execute simultaneously, as described in the
next section. Blocks can be nested to arbitrary order.

Here is an example of a loop over a std::map with a pointer for its key, illustrating how the
`dilog::block` object works.

    #include <dilog.h> 
    ...
    std::map<farm*, std::vector<sheep> > herds;
    ...
    for (auto herd : herds) {
        dilog::block myloop("sheepcounter", "farmloop");
        ...
        for (int isheep=0; isheep < herd.second.size(); ++isheep) {
           ...
           dilog::get("sheepcounter").printf("looking at sheep %d in herd %s\n", isheep, herd.first->name);
           ...
        }
     }
    ...

The above loop will execute its iterations in a different order from one run to the next, but the
fact that the dilog messages take place within the context of a dilog::block means that no dilog
exception will be generated simply because they are emitted in a different order. Any number of
dilog messages would be allowed within the block. Note that dynamic allocation of ``myloop`` on the
stack automatically calls the destructor whenever `myloop` goes out of scope, so explicit delete of
the dilog::block objects is not necessary.

## Multithread strategy

## Segmentation strategy
In some cases involving a very many iterations of a block, it might take a very long time for dilog
to find that none of the iterations recorded in the input dilog file contain a match to the latest
message it has received. In that case, a better strategy might be to assign a unique name for each
iteration of the loop and pass it to the `dilog.get("event_i").printf` statements instead of enclosing
them all inside a block. This segmentation strategy will result in separate files `event_i.dilog`
being written in the cwd, with a different i for each iteration of the loop. But then the dilog
input scanner will have a unique input file to check against each iteration of the loop, and so
it will run with very little cpu overhead.

## Test conditions
I developed and tested this initial release of the code with g++ under gcc 4.8.5, but it should work with
any of the more recent gcc releases. Multithreading support requires -std=c++11 in order to use std::mutex.

## Bugs, comments, and suggestions
To communicate with the author, please use the address richard.t.jones at uconn.edu.
