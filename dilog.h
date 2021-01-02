//
// dilog - A header-only C++ diagnostic class for finding the point
//         of divergence between two runs of an application that
//         should produce identical results each time it runs, bug
//         does not.
//
// author: richard.t.jones at uconn.edu
// date: january 2, 2021 [rtj]
//
// programmer's notes:
// 1) This class is only intended for diagnostic use once a discrepancy
//    has been found in the output of an application between repeated
//    runs with the same input data. It is not intended to be used in
//    regular producition, or become a permanent part of any application.
// 2) The first time you run your dilog-instrumented application in a
//    given directory, it will write its output file into the cwd. 
//    Running it a second time in the same directory will check the
//    dilog output against the original and produce a runtime exception
//    at the first appearance of differences.
// 3) Assuming the g++ compiler, you must include the -std=c++11 switch.
//
// usage example:
//     #include <dilog.h> 
//     ...
//     dilog::printf("myapp", "sheep %d in herd %s\n", isheep, herd);
//     ...

#include <stdio.h>
#include <stdarg.h>

#include <iostream>
#include <fstream>
#include <exception>
#include <sstream>
#include <string>
#include <thread>
#include <map>
#include <vector>

class dilog;
using dilogs_map = std::map<std::string, dilog*>;

class dilog {
 public:

   static dilog &get(const std::string &channel) {
      dilogs_map &dilogs = get_map();
      if (dilogs.find(channel) == dilogs.end()) {
         dilog *me = new dilog;
         dilogs[channel] = me;
         std::string fname(channel);
         fname += ".dilog";
         me->fReading = new std::ifstream(fname.c_str());
         if (me->fReading->good()) {
            me->fWriting = 0;
         }
         else {
            me->fReading = 0;
            me->fWriting = new std::ofstream(fname.c_str());
         }
         me->fBlock.push_back(channel);
      }
      return *dilogs[channel];
   }

   int myprintf(const char* fmt, ...) {
      const unsigned int max_message_size(999);
      char msg[max_message_size + 1];
      va_list args;
      va_start(args, fmt);
      int bytes = vsnprintf(msg, max_message_size, fmt, args);
      va_end(args);
      std::stringstream message;
      for (int i=0; i < fBlock.size(); ++i)
         message << ((i > 0)? "/" : "[") << fBlock[i];
      message << "] " << msg; 
      if (message.str().back() != '\n') {
printf("terminal character of message was %d, appending a nl\n", (int)message.str().back());
         message << std::endl;
      }
      if (fWriting) {
printf("supposed to be writing message %s", message.str().c_str());
         *fWriting << message.str();
      }
      else {
         check_message(message.str());
      }
      return bytes;
   }

   void check_message(std::string message) {
      printf("bad check\n");
   }

   void block_begin(std::string) {
   }

   void block_end(std::string) {
   }

 protected:
   dilog() {
   }

   ~dilog() {
      std::cout << "called dilog destructor" << std::endl;
      if (fReading)
         delete fReading;
      if (fWriting)
         delete fWriting;
   }

   std::ifstream *fReading;
   std::ofstream *fWriting;
   std::vector<std::string> fBlock;

 private:
   class dilogs_holder {
    public:
      dilogs_map fDilogs;
      dilogs_holder() {}
      ~dilogs_holder() {
         std::cout << "called dilogs_holder destructor" << std::endl;
         dilogs_map &dilogs = get_map();
         for (auto iter : dilogs) {
            delete iter.second;
            dilogs.erase(iter.first);
         }
      }
   };

   static dilogs_map& get_map() {
       static dilogs_holder holder;
       return holder.fDilogs;
   }
};
