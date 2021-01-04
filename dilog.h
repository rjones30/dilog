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
#include <stack>

class dilog;
struct dilogs_block_t {
   int lineno;
   int beginline;
   int endline;
   std::streampos gptr;
   std::streampos eptr;
   std::map<std::streampos, int> glist;
   std::vector<std::string> mlist;
   std::string prefix;
   dilogs_block_t() : lineno(0), beginline(0), endline(0), gptr(0), eptr(0) {}
};
using dilogs_map_t = std::map<std::string, dilog*>;

class dilog {
 public:

   static dilog &get(const std::string &channel) {
      dilogs_map_t &dilogs = get_map();
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
         dilogs_block_t sblock;
         sblock.prefix = channel;
         me->fBlocks.push(sblock);
      }
      return *dilogs[channel];
   }

   int printf(const char* fmt, ...) {
      const unsigned int max_message_size(999);
      char msg[max_message_size + 1];
      va_list args;
      va_start(args, fmt);
      int bytes = vsnprintf(msg, max_message_size, fmt, args);
      va_end(args);
      dilogs_block_t &block = fBlocks.top();
      std::string message = "[" + block.prefix + "]" + msg;
      if (message.back() != '\n')
         message += "\n";
      if (fWriting) {
         *fWriting << message;
         ++block.lineno;
      }
      else
         check_message(message);
      return bytes;
   }

   void check_message(std::string message) {
      dilogs_block_t &block = fBlocks.top();
      std::string mexpected = "[" + block.prefix + "]" + message;
      std::string nextmsg;
      while (getline(*fReading, nextmsg, '\n')) {
         ++block.lineno;
         if (nextmsg.find(block.prefix) != 1)
            continue;
         else if (nextmsg == mexpected) {
            if (fBlocks.size() > 1)
               block.mlist.push_back("[]" + message);
            return;
         }
         else if (fBlocks.size() > 1) {
            if (! block_next())
               break;
         }
         break;
      }
      throw std::runtime_error("bad goat");
   }

   void block_begin(std::string name) {
      dilogs_block_t &block = fBlocks.top();
      if (fWriting) {
         dilogs_block_t newblock(block);
         newblock.prefix += "/" + name;
         *fWriting << "[" << newblock.prefix << "[" << std::endl;
         ++newblock.lineno;
         fBlocks.push(newblock);
      }
      else {
         std::streampos gptr = fReading->tellg();
         std::string mexpected = "[" + block.prefix + "/" + name + "[";
         std::string mexpected2 = "[" + block.prefix + "[";
         if (mexpected2.find(name + "[") == mexpected2.npos)
            mexpected2 = ">>>>><<<<<";
         std::string nextmsg;
         while (getline(*fReading, nextmsg, '\n')) {
            ++block.lineno;
            if (nextmsg.find(block.prefix) != 1)
               continue;
            else if (nextmsg == mexpected)
               break;
            else if (nextmsg == mexpected2)
               return;
            else
               throw std::runtime_error("bad elk");
         }
         dilogs_block_t newblock;
         newblock.prefix = block.prefix + "/" + name;
         newblock.gptr = block.gptr;
         newblock.beginline = block.lineno - 1;
         fBlocks.push(newblock);
      }
   }

   void block_end(std::string name) {
      dilogs_block_t &block = fBlocks.top();
      std::string stail = block.prefix + "[";
      if (stail.find("/" + name + "[") == stail.npos) {
         throw std::runtime_error("bad beef");
      }
      else if (fWriting) {
         int lineno = block.lineno;
         *fWriting << "]" << block.prefix << "]" << std::endl;
         fBlocks.pop();
         fBlocks.top().lineno = ++lineno;
      }
      else {
         std::string mexpected = "]" + block.prefix + "]";
         std::string nextmsg;
         while (getline(*fReading, nextmsg, '\n')) {
            ++block.lineno;
            if (nextmsg.find(block.prefix) != 1)
               continue;
            else if (nextmsg == mexpected)
               break;
            else if (block_next())
               continue;
            else
               throw std::runtime_error("bad rabbit");
         }
         std::streampos gptr = fReading->tellg();
         if (gptr > block.eptr) {
            block.eptr = gptr;
            block.endline = block.lineno;
         }
         block.glist.erase(block.gptr);
         if (block.glist.size() > 0) {
            block.gptr = block.glist.begin()->first;
            block.lineno = block.glist.begin()->second;
            fReading->seekg(block.gptr);
         }
         else {
            dilogs_block_t bfinished(block);
            fBlocks.pop();
            dilogs_block_t &bresumed = fBlocks.top();
            if (bfinished.eptr > gptr) {
               bresumed.lineno = bfinished.endline;
               bresumed.endline = bfinished.endline;
               bresumed.gptr = bfinished.eptr;
               bresumed.eptr = bfinished.eptr;
            }
            else {
               bresumed.lineno = bfinished.lineno;
               bresumed.endline= bfinished.lineno;
               bresumed.gptr = gptr;
               bresumed.eptr = gptr;
            }
            bresumed.mlist.push_back("[[" + name);
            bresumed.mlist.insert(bresumed.mlist.end(),
                                  bfinished.mlist.begin(),
                                  bfinished.mlist.end());
            bresumed.mlist.push_back("]]" + name);
            fReading->seekg(bresumed.gptr);
         }
      }
   }

   int get_lineno() {
      return fBlocks.top().lineno;
   }

 protected:
   dilog() {
   }

   ~dilog() {
      if (fReading)
         delete fReading;
      if (fWriting)
         delete fWriting;
   }

   bool block_next() {
      dilogs_block_t &block = fBlocks.top();
      std::string mexpected = "]" + block.prefix + "]";
      std::string nextmsg;
      while (getline(*fReading, nextmsg, '\n')) {
         ++block.lineno;
         if (nextmsg == mexpected)
            break;
      }
      block.glist[block.gptr] = block.beginline;
      auto bnext = ++block.glist.find(block.gptr);
      if (bnext != block.glist.end()) {
         block.gptr = bnext->first;
         block.lineno = bnext->second;
         block.beginline = bnext->second;
         fReading->seekg(block.gptr);
      }
      else {
         block.gptr = fReading->tellg();
         block.beginline = block.lineno;
      }
      mexpected = "[" + block.prefix + "[";
      while (getline(*fReading, nextmsg, '\n')) {
         ++block.lineno;
         if (nextmsg.find(block.prefix) != 1)
            continue;
         else if (nextmsg != mexpected)
            return false;
         else
            break;
      }
      try {
         for (std::string msg : block.mlist) {
            if (msg.substr(0,2) == "[[")
               block_begin(msg.substr(2));
            else if (msg.substr(0,2) == "]]")
               block_end(msg.substr(2));
            else
               printf(msg.substr(2).c_str());
         }
         return true;
      }
      catch (std::exception e) {
         return false;
      }
   }

   std::ifstream *fReading;
   std::ofstream *fWriting;
   std::stack<dilogs_block_t> fBlocks;

 private:
   class dilogs_holder {
    public:
      dilogs_map_t fDilogs;
      dilogs_holder() {}
      ~dilogs_holder() {
         dilogs_map_t &dilogs = get_map();
         for (auto iter : dilogs) {
            delete iter.second;
            dilogs.erase(iter.first);
         }
      }
   };

   static dilogs_map_t& get_map() {
       static dilogs_holder holder;
       return holder.fDilogs;
   }
};
