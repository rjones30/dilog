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
using dilogs_map_t = std::map<std::string, dilog*>;

class dilog {
 public:
   class block {
    private:
      int lineno;
      int beginline;
      int endline;
      std::streampos gptr;
      std::streampos eptr;
      std::map<std::streampos, int> glist;
      std::vector<std::string> mlist;
      std::string prefix;
      std::string chan;
      std::string name;

    public:
      block()
       : lineno(0), beginline(0), endline(0), gptr(0), eptr(0)
      {}

      block(const std::string &channel, const std::string &blockname)
       : lineno(0), beginline(0), endline(0), gptr(0), eptr(0),
         chan(channel), name(blockname)
      {
         dilog &dlog = dilog::get(channel);
         block &top = *dlog.fBlocks.top();
         dlog.fBlocks.push(this);
         prefix = top.prefix + "/" + name;
         if (dlog.fLastBlock.prefix == prefix) {
            copy(dlog.fLastBlock);
         }
         else {
            lineno = top.lineno;
            beginline = top.lineno;
         }
         if (dlog.fWriting) {
            *dlog.fWriting << "[" << prefix << "[" << std::endl;
            ++lineno;
         }
         else {
            std::string mexpected = "[" + prefix + "[";
            std::string nextmsg;
            while (getline(*dlog.fReading, nextmsg, '\n')) {
               gptr = dlog.fReading->tellg();
               beginline = lineno;
               ++lineno;
               if (nextmsg.find(prefix) != 1)
                  continue;
               else if (nextmsg == mexpected)
                  break;
               else
                  throw std::runtime_error("dilog::block error: "
                        "expected new execution block \"" +
                        prefix + "\" at line " + std::to_string(lineno)
                        + " in " + channel + ".dilog");
            }
         }
      }

      ~block() {
         dilog &dlog = dilog::get(chan);
         if (dlog.fBlocks.top() != this)
            return;
         dlog.fBlocks.pop();
         block &top = *dlog.fBlocks.top();
         if (dlog.fWriting) {
            *dlog.fWriting << "]" << prefix << "]" << std::endl;
            top.lineno = ++lineno;
         }
         else {
            std::string mexpected = "]" + prefix + "]";
            std::string nextmsg;
            while (getline(*dlog.fReading, nextmsg, '\n')) {
               ++lineno;
               if (nextmsg.find(prefix) != 1)
                  continue;
               else if (nextmsg == mexpected)
                  break;
               else if (dlog.next_block())
                  continue;
               else
                  throw std::runtime_error("dilog::block error: "
                        "expected end of execution block \"" +
                        prefix + "\" at line " + std::to_string(lineno)
                        + " in " + chan + ".dilog");
            }
            glist.erase(gptr);
            gptr = dlog.fReading->tellg();
            if (gptr > eptr)
               eptr = gptr;
            endline = lineno;
            if (glist.size() > 0) {
               gptr = glist.begin()->first;
               lineno = glist.begin()->second;
               dlog.fReading->seekg(gptr);
            }
            else if (eptr > top.gptr) {
               top.lineno = endline;
               top.gptr = eptr;
            }
            else {
               top.lineno = lineno;
               top.gptr = gptr;
            }
            mlist.clear();
            dlog.fLastBlock.copy(*this);
            if (glist.size() > 0) {
               top.mlist.push_back("[[" + name);
               top.mlist.insert(top.mlist.end(), mlist.begin(), mlist.end());
               top.mlist.push_back("]]" + name);
            }
            dlog.fReading->seekg(top.gptr);
         }
      }

      void copy(const block &src) {
         lineno = src.lineno;
         beginline = src.beginline;
         endline = src.endline;
         gptr = src.gptr;
         eptr = src.eptr;
         glist = src.glist;
         mlist = src.mlist;
         prefix = src.prefix;
         chan = src.chan;
         name = src.name;
      }
      friend class dilog;
   };

   static dilog &get(const std::string &channel) {
      dilogs_map_t &dilogs = get_map();
      if (dilogs.find(channel) == dilogs.end())
         dilogs[channel] = new dilog(channel);
      return *dilogs[channel];
   }

   int printf(const char* fmt, ...) {
      const unsigned int max_message_size(999);
      char msg[max_message_size + 1];
      va_list args;
      va_start(args, fmt);
      int bytes = vsnprintf(msg, max_message_size, fmt, args);
      va_end(args);
      block &top = *fBlocks.top();
      std::string message = "[" + top.prefix + "]" + msg;
      if (message.back() != '\n')
         message += "\n";
      if (fWriting) {
         *fWriting << message;
         ++top.lineno;
      }
      else
         check_message(msg);
      return bytes;
   }

   int get_lineno() const {
      return fBlocks.top()->lineno;
   }

 protected:
   dilog() = delete;
   dilog(const std::string& channel) {
      dilogs_map_t &dilogs = get_map();
      dilogs[channel] = this;
      std::string fname(channel);
      fname += ".dilog";
      fReading = new std::ifstream(fname.c_str());
      if (fReading->good()) {
         fWriting = 0;
      }
      else {
         fReading = 0;
         fWriting = new std::ofstream(fname.c_str());
      }
      block *bot = new block;
      bot->prefix = channel;
      fBlocks.push(bot);
   }

   ~dilog() {
      if (fReading)
         delete fReading;
      if (fWriting)
         delete fWriting;
      delete fBlocks.top();
   }

   void check_message(std::string message) {
      int nl;
      while ((nl = message.find('\n')) != message.npos)
         message.erase(nl);
      block &top = *fBlocks.top();
      std::string mexpected = "[" + top.prefix + "]" + message;
      std::string nextmsg;
      while (getline(*fReading, nextmsg, '\n')) {
         ++top.lineno;
         if (nextmsg.find(top.prefix) != 1)
            continue;
         else if (nextmsg == mexpected) {
            if (fBlocks.size() > 1)
               top.mlist.push_back("[]" + message);
            return;
         }
         else if (fBlocks.size() > 1) {
            if (next_block())
               continue;
         }
         break;
      }
      throw std::runtime_error("dilog::printf error: "
            "expected dilog message \"" + message +
            "\" at line " + std::to_string(top.lineno)
            + " in " + top.chan + ".dilog");
   }

   bool next_block() {
      block &top = *fBlocks.top();
      std::string mexpected = "]" + top.prefix + "]";
      std::string nextmsg;
      while (getline(*fReading, nextmsg, '\n')) {
         ++top.lineno;
         if (nextmsg == mexpected)
            break;
      }
      top.glist[top.gptr] = top.beginline;
      auto bnext = ++top.glist.find(top.gptr);
      if (bnext != top.glist.end()) {
         top.gptr = bnext->first;
         top.lineno = bnext->second;
         top.beginline = bnext->second;
         fReading->seekg(top.gptr);
      }
      else {
         top.gptr = fReading->tellg();
         top.beginline = top.lineno;
      }
      mexpected = "[" + top.prefix + "[";
      while (getline(*fReading, nextmsg, '\n')) {
         ++top.lineno;
         if (nextmsg.find(top.prefix) != 1)
            continue;
         else if (nextmsg != mexpected)
            return false;
         else
            break;
      }
      try {
         std::map<std::string, block*> blocks;
         for (std::string msg : top.mlist) {
            if (msg.substr(0,2) == "[[")
               blocks[msg.substr(2)] = new block(top.chan, msg.substr(2));
            else if (msg.substr(0,2) == "]]")
               delete blocks[msg.substr(2)];
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
   std::stack<block*> fBlocks;
   block fLastBlock;

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
