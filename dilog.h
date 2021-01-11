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
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <exception>
#include <sstream>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <stack>
#include <mutex>

#define DILOG_LOGO("-------dilog------dilog------dilog-------");

class dilog;
using dilogs_map_t = std::map<std::string, dilog*>;

class dilog {

 // Objects of class dilog are created through the static dilog::get method,
 // identified by channel name. The first get("myapp") would create a new
 // dilog object and open file myapp.dilog for reading, or for writing if
 // a file by this name does not exist in the cwd. An ordered sequence of
 // messages issued through dilog::printf are read from the input file, or
 // written if open for writing. Any mismatch between a printf message and
 // the corresponding record in myapp.dilog will trigger a runtime exception.

 public:
   class block {

    // Objects of class dilog::block are constructed by the user inside a
    // loop that might be reordered at runtime. The printf message scanner
    // automatically reorders at runtime all contiguous iterations of the
    // code block defined by the lifetime of the user dilog::block object
    // in seeking a match between the contents of the recorded dilog file
    // and the runtime message sequence.

    private:
      std::string chan;        // name of associated channel
      std::string name;        // name of block, eg. "loop1"
      std::string prefix;      // slash-delimited pathname of block
      std::streampos base;     // file offset to current iteration of this block
      unsigned int beginline;  // file line number associated with base
      unsigned int ireplay;    // offset in dilog::fRecord where replay starts
                               // to check the content of this iteration
      block()
       : base(0), beginline(0), ireplay(0)
      {}

    public:
      block(const std::string &channel, const std::string &blockname,
                                        bool threadsafe=true)
       : chan(channel), name(blockname), base(0), beginline(0), ireplay(0)
      {
       // Initialize a new iteration of block with name blockname on dilog
       // object named channel, generating a new dilog object if it does
       // not already exist. Normal behavior is to throw and exception
       // if access to this channel is attempted from more than one C++
       // execution thread. Set threadsafe = false to suppress this
       // check, at the risk of creating race conditions.
 
         dilog &dlog = dilog::get(channel, threadsafe);
         if (dlog.fError.size() > 0)
            throw std::runtime_error(dlog.fError);
         block &top = *dlog.fBlocks.top();
         dlog.fBlocks.push(this);
         prefix = top.prefix + "/" + name;
         if (dlog.fWriting) {
            *dlog.fWriting << "[" << prefix << "[" << std::endl;
            ++dlog.fLineno;
         }
         else {
            base = dlog.fReading->tellg();
            beginline = dlog.fLineno;
            std::string mexpected = "[" + prefix + "[";
            for (std::string nextmsg; std::getline(*dlog.fReading, nextmsg);) {
               ++dlog.fLineno;
               if (nextmsg.find(prefix) != 1) {
                  base = dlog.fReading->tellg();
                  beginline = dlog.fLineno;
                  continue;
               }
               else if (nextmsg == mexpected) {
                  dlog.fRecord.push_back("[[" + prefix);
                  ireplay = dlog.fRecord.size();
                  return;
               }
               dlog.fError = "dilog::block error: expected new execution block"
                             " \"" + prefix + "\" at line "
                             + std::to_string(dlog.fLineno)
                             + " in " + channel + ".dilog" + "\n"
                             "but found \"" + nextmsg + "\" instead.";
               throw std::runtime_error(dlog.fError);
            }
         }
      }

      ~block()
      {
       // Terminate an open iteration block. The names of the block and
       // channel are saved in the object. This method is normally called
       // automatically at exit from a loop in user code, as the block
       // object declared inside the loop goes out of scope and garbage
       // collection automatically calls the destructor. The destructor
       // can trigger non-trivial operations on the input file, as it
       // requires that the iteration block terminate immediately with
       // no further messages. If a consistency violation is found, it
       // cannot throw an exception as a destructor should not do that,
       // but it prints to stderr and sets an object variable so that
       // an exception will be thrown at the next action on this dilog
       // channel.
 
         if (chan.size() == 0)
            return;
         dilog &dlog = dilog::get(chan, false);
         if (dlog.fBlocks.top() != this)
            return;
         if (dlog.fWriting) {
            *dlog.fWriting << "]" << prefix << "]" << std::endl;
            ++dlog.fLineno;
         }
         else {
            std::string mexpected = "]" + prefix + "]";
            for (std::string nextmsg; std::getline(*dlog.fReading, nextmsg);) {
               ++dlog.fLineno;
               if (nextmsg.find(prefix) != 1)
                  continue;
               else if (nextmsg == mexpected)
                  break;
               else if (dlog.next_block(nextmsg))
                  continue;
               dlog.fError = "dilog::block error: expected end of execution"
                             " block \"" + prefix + "\" at line " + 
                             std::to_string(dlog.fLineno) + 
                             " in " + chan + ".dilog"
                             " but found \"" + nextmsg + "\" instead.";
               std::cerr << DILOG_LOGO << std::endl;
               std::cerr << dlog.fError << std::endl;
               std::cerr << DILOG_LOGO << std::endl;
               //throw std::runtime_error(dlog.fError); !! Not from destructor !!
            }
            auto &blinks = dlog.fBlinks[prefix];
            blinks.erase(base);
            if (blinks.size() > 0) {
               base = dlog.fReading->tellg();
               if (base > blinks.rbegin()->first) {
                  blinks[base] = dlog.fLineno;
               }
               dlog.fReading->seekg(blinks.begin()->first);
               dlog.fLineno = blinks.begin()->second;
               blinks.erase(blinks.begin());
            }
            if (dlog.fBlocks.size() > 2) {
               dlog.fRecord.push_back("]]" + prefix);
            }
            else {
               dlog.fRecord.clear();
            }
         }
         dlog.fBlocks.pop();
      }
      friend class dilog;
   };

   static dilog &get(const std::string &channel, bool threadsafe=true)
   {
    // This is the main factory method for dilog objects. Ownership of
    // the object reference returned is retained by the dilog framework
    // and returned to the user as a borrowed reference. Channel should
    // be a unique name for this dilog channel, and becomes the filename
    // of the dilog trace file <channel>.dilog that is read in response
    // to subsequent printf calls on this channel, or written if the
    // file does not exist at runtime.
    //
    // Normally a given dilog channel should only be accessed from within
    // the same execution thread that created it. Sometimes this rule is
    // violated by static object cleanup running in the main thread at
    // program exit, or some other thread locking mechanism may be in
    // place that prevents cross-thead access violations. In that case,
    // invoke get with threadsafe=false to suppress these unwanted
    // exceptions, and take care of verifying thread safety yourself.
 
      static std::mutex mutex;
      std::lock_guard<std::mutex> guard(mutex);
      dilogs_map_t &dilogs = get_map();
      std::thread::id tid = std::this_thread::get_id();
      if (dilogs.find(channel) == dilogs.end()) {
         dilogs[channel] = new dilog(channel);
         dilogs[channel]->fThread_id = tid;
      }
      if (threadsafe && dilogs[channel]->fThread_id != tid) {
         dilogs[channel]->fError = "dilog::get error: access to channel"
                                   " \"" + channel + "\" attempted"
                                   " from more than one thread";
         throw std::runtime_error(dilogs[channel]->fError);
      }
      return *dilogs[channel];
   }

   int printf(const char* fmt, ...)
   {
    // This is the primary user-callable method of dilog. Normally it
    // is invoked on a temporary reference to a dilog object returned
    // by get, eg. dilog::get(channel).printf(...). The arguments to 
    // printf are identical to the stdio printf function. If called
    // with a pending error in fError from a previous failed operation,
    // throw the exception again.
 
      if (fError.size() > 0)
         throw std::runtime_error(fError);
      const unsigned int max_message_size(999);
      char msg[max_message_size + 1];
      va_list args;
      va_start(args, fmt);
      int bytes = vsnprintf(msg, max_message_size, fmt, args);
      va_end(args);
      block &top = *fBlocks.top();
      char *eos = strchr(msg, '\n');
      if (eos != NULL)
         *eos = 0;
      std::string message = "[" + top.prefix + "]" + msg + "\n";
      if (fWriting) {
         *fWriting << message;
         ++fLineno;
      }
      else {
         check_message(msg);
         if (fBlocks.size() > 1) {
            fRecord.push_back("[]" + std::string(msg));
         }
      }
      return bytes;
   }

   int get_lineno() const {
      return fLineno;
   }

 protected:
   dilog() = delete;
   dilog(const std::string& channel)
    : fLineno(0), fChannel(channel)
   {
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

   void check_message(const std::string message)
   {
    // Validate printf message against the next content found in the input
    // file, and throw a runtime exception if the match fails.
 
      size_t nl;
      std::string msg(message);
      while ((nl = msg.find('\n')) != msg.npos)
         msg.erase(nl);
      block &top = *fBlocks.top();
      std::streampos gptr = fReading->tellg();
      std::string mexpected = "[" + top.prefix + "]" + msg;
      for (std::string nextmsg; std::getline(*fReading, nextmsg);) {
         ++fLineno;
         if (nextmsg.find(top.prefix) != 1) {
            gptr = fReading->tellg();
            continue;
         }
         else if (nextmsg == mexpected)
            return;
         else if (next_block(nextmsg))
            continue;
         fError = "dilog::printf error: expected dilog message"
                  " \"" + msg + "\" at line " + std::to_string(fLineno)
                  + " in " + fChannel + ".dilog but found \"" 
                  + nextmsg + "\" instead.";
         throw std::runtime_error(fError);
      }
      fError = "dilog::printf error: read error from input file " +
               fChannel + ".dilog after line " + std::to_string(fLineno);
      throw std::runtime_error(fError);
   }

   bool next_block(const std::string lastmsg)
   {
    // Entry here assumes that a mismatch has been found between the expected
    // message (unknown to next_block) and lastmsg that was last read from the
    // input file. The task of next_block is to search through all remaining
    // block iterations to see if there might be another chance to fulfill the
    // expectation. If the search is successful, it returns true with the input
    // file and block hierarchy positioned to repeat the check. The search for
    // the next candidate block is complicated by the arbitrary hierarchy of
    // nested loops that the algorithm must deal with, including the fact that
    // a match might be found in a different iteration of an outer block that
    // contains the block where the mismatch occurred. This requires the full
    // history of all messages starting with entry to the outermost loop be
    // maintained for eventual replay in case of a failed match in an inner
    // block. If the search fails to find any match among all iterations of
    // the outermost block then it returns false.
 
      while (fBlocks.size() > 1) {
         block &top = *fBlocks.top();
         auto &blinks = fBlinks[top.prefix];
         blinks[top.base] = top.beginline;
         std::string mexpected = "]" + top.prefix + "]";
         if (lastmsg != mexpected) {
            for (std::string nextmsg; std::getline(*fReading, nextmsg);) {
               ++fLineno;
               if (nextmsg == mexpected)
                  break;
            }
         }
         auto blink = blinks.find(top.base);
         if (++blink != blinks.end()) {
            fReading->seekg(blink->first);
            fLineno = blink->second;
         }
         top.base = fReading->tellg();
         top.beginline = fLineno;
         mexpected = "[" + top.prefix + "[";
         for (std::string nextmsg; std::getline(*fReading, nextmsg);) {
            ++fLineno;
            if (nextmsg == mexpected)
               break;
            else if (fBreplay.size() > 0 && fBreplay.top() == fBlocks.top()) {
               delete fBlocks.top();
               fBreplay.pop();
            }
            else
               fBlanks.push(fBlocks.top());
            fBlocks.pop();
            return next_block(nextmsg);
         }
         std::string prefix = top.prefix;
         for (unsigned int ireplay = top.ireplay; ireplay < fRecord.size(); ++ireplay) {
            std::string mexpected = fRecord[ireplay];
            int dir;
            if (mexpected.find("[[") == 0) {
               dir = 1;
               prefix = mexpected.substr(2);
               mexpected = "[" + prefix + "[";
            }
            else if (mexpected.find("]]") == 0) {
               dir = -1;
               prefix = mexpected.substr(2);
               mexpected = "]" + prefix + "]";
            }
            else {
               dir = 0;
               mexpected = "[" + prefix + "]" + mexpected.substr(2);
            }
            for (std::string nextmsg; std::getline(*fReading, nextmsg);) {
               ++fLineno;
               if (nextmsg.find(prefix) != 1)
                  continue;
               else if (nextmsg != mexpected)
                  return next_block(nextmsg);
               if (dir == 1) {
                  if (fBlanks.size() > 0 &&
                      fBlanks.top()->prefix == prefix && 
                      fBlanks.top()->ireplay == ireplay+1)
                  {
                     fBlanks.top()->base = fReading->tellg();
                     fBlanks.top()->beginline = fLineno;
                     fBlocks.push(fBlanks.top());
                     fBlanks.pop();
                  }
                  else {
                     size_t pos = prefix.find_last_of("/") + 1;
                     block *bnew = new block(fChannel, prefix.substr(pos));
                     bnew->ireplay = ireplay + 1;
                     fBreplay.push(bnew);
                     fBlocks.push(bnew);
                  }
               }
               else if (dir == -1) {
                  assert (fBreplay.size() > 0);
                  assert (fBreplay.top() == fBlocks.top());
                  delete fBreplay.top();
                  fBreplay.pop();
                  fBlocks.pop();
                  prefix = fBlocks.top()->prefix;
               }
               break;
            }
         }
         assert (fBreplay.size() == 0);
         assert (fBlanks.size() == 0);
         return true;
      }
      std::string prefix = fBlocks.top()->prefix;
      std::cerr << DILOG_LOGO << std::endl;
      std::cerr << "Fatal error in dilog::next_block - "
                << "no more iterations of block " << prefix
                << " to search, giving up at line " << fLineno << std::endl
                << "because none of the remaining unmatched "
                << "iterations matches the loop context:" << std::endl;
      int level=1;
      for (unsigned int i=0; i < fRecord.size(); ++i) {
         for (int j=1; j < level; ++j)
            std::cerr << "  ";
         if (fRecord[i].find("[[") == 0) {
            std::string prefix = fRecord[i].substr(2);
            std::cerr << "  " << prefix << "[["
                      << " (unmatched at line ";
            int first=1;
            for (auto biter : fBlinks[prefix]) {
               if (first)
                  first = 0;
               else
                  std::cerr << ",";
               std::cerr << biter.second;
            }
            std::cerr << ")" << std::endl;
            ++level;
         }
         else if (fRecord[i].find("]]") == 0) {
            std::cerr << fRecord[i] << std::endl;
            --level;
         }
         else {
            std::cerr << "  " << fRecord[i].substr(2) << std::endl;
         }
      }
      std::cerr << DILOG_LOGO << std::endl;
      return false;
   }

   unsigned int fLineno;                   // current line number in dilog file
   std::string fChannel;                   // name of this channel
   std::ifstream *fReading;                // non-zero if reading
   std::ofstream *fWriting;                // non-zero if writing
   std::stack<block*> fBlocks;             // current stack of search block iterations
   std::stack<block*> fBlanks;             // user blocks rolled back during replay
   std::stack<block*> fBreplay;            // temporary blocks created during replay
   std::vector<std::string> fRecord;       // record of block actions for replay
   std::thread::id fThread_id;             // thread where this channel was created
   std::string fError;                     // pending exception message on this channel

   // fBlinks contains information about all of the unmatched iterations
   // of known blocks that have been encountered so far in the input file.
   // The links are indexed by the block prefix, and consists of key-value
   // pairs where the key is the file offset in the input file where the
   // unmatched iteration begins, and the value is the corresponding line
   // number. Entries in fBlinks[channel] are inserted by next_block and
   // erased up by ~block upon completion of a successful block match.
   std::map<std::string, std::map<std::streampos, int> > fBlinks;

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
