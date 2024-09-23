#!/usr/bin/env python3
#
# rootdiff.py - runs a recursive comparison on all root objects
#               within a two root files. For the files to be
#               counted equal, they must have the same directory
#               structure, and all of the objects contained in
#               each directory must have identical structure and
#               contents. Visualization options like line color
#               and shading are not included in the comparison.
#
# author: richard.t.jones at uconn.edu
# version: april 5, 2021

import ROOT
import numpy as np
import sys

def usage():
   print("Usage: rootdiff.py [-q] <file1.root> <file2.root>")

datatype = {"TH1C": np.byte,   "TH2C": np.byte,   "TH3C": np.byte,
            "TH1D": np.double, "TH2D": np.double, "TH3D": np.double,
            "TH1F": np.single, "TH2F": np.single, "TH3F": np.single,
            "TH1I": np.intc,   "TH2I": np.intc,   "TH3I": np.intc,
            "TH1S": np.short,  "TH2S": np.short,  "TH3S": np.short,
            "TProfile": np.double,
            "TProfile2D": np.double,
            "TProfile3D": np.double,
           }
            
def TAxis_equal(ax1, ax2):
   """
   Runs a comparison between two TAxis objects in memory, and returns
   false if any differences are found, otherwise true.
   """
   if ax1.GetName() != ax2.GetName():
      if not quiet:
         print("histogram name mismatch: \"" +
               ax1.GetName() + "\" != \"" + ax2.GetName())
      return False
   elif ax1.GetTitle() != ax2.GetTitle():
      if not quiet:
         print("histogram title mismatch: \"" +
               ax1.GetTitle() + "\" != \"" + ax2.GetTitle())
      return False
   elif ax1.GetNbins() != ax2.GetNbins():
      if not quiet:
         print("histogram bin count mismatch:",
               ax1.GetNbins(), "!=", ax2.GetNbins())
      return False
   elif ax1.IsAlphanumeric() != ax2.IsAlphanumeric():
      if not quiet:
         print("histogram axis type mismatch")
         return False
   elif ax1.IsVariableBinSize() != ax2.IsVariableBinSize():
      if not quiet:
         print("histogram axis division mismatch")
      return False
   elif ax1.IsAlphanumeric():
      for i in range(0, ax1.GetNbins()):
         if ax1.GetBinLabel(i+1) != ax2.GetBinLabel(i+1):
            if not quiet:
               print("histogram axis label mismatch: \"" +
                     ax1.GetBinLabel(i+1) + "\" != \"" + ax2.GetBinLabel(i+1))
            return False
   elif ax1.IsVariableBinSize():
      for i in range(0, ax1.GetNbins()+1):
         if ax1.GetBinLowEdge(i+1) != ax2.GetBinLowEdge(i+1):
            if not quiet:
               print("histogram axis division mismatch: \"" +
                     ax1.GetBinLowEdge(i+1) + "\" != \"" + ax2.GetBinLowEdge(i+1))
            return False
   elif ax1.GetXmax() != ax2.GetXmax():
      if not quiet:
         print("histogram axis upper limit mismatch:",
               ax1.GetXmax(), "!=", ax2.GetXmax())
      return False
   elif ax1.GetXmin() != ax2.GetXmin():
      if not quiet:
         print("histogram axis lower limit mismatch:",
               ax1.GetXmin(), "!=", ax2.GetXmin())
      return False
   return True

def TH1_equal(h1, h2):
   """
   Runs a comparison between two TH1 objects in memory, and returns
   false if any differences are found, otherwise true. This function
   covers all of the histgram types in ROOT, eg. TH1I, TH2Poly, TH2D,
   TProfile3D, etc.
   """
   if h1.ClassName() != h2.ClassName():
      print("histogram class mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      print("   " + h1.ClassName(), "!=", h2.ClassName())
      return False
   elif h1.GetName() != h2.GetName():
      print("histogram name mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      print("   " + h1.GetName(), "!=", h2.GetName())
      return False
   elif h1.GetTitle() != h2.GetTitle():
      print("histogram title mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      print("   " + h1.GetTitle(), "!=", h2.GetTitle())
      return False
   shape = []
   axes = ((h1.GetXaxis(), h1.GetXaxis(), h1.GetZaxis()),
           (h2.GetXaxis(), h2.GetXaxis(), h2.GetZaxis()))
   for ax in range(0,2):
      if axes[0][ax] and axes[1][ax]:
         if not TAxis_equal(axes[0][ax], axes[1][ax]):
            print("histogram axis mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
            print("   axis:", ax)
            return False
      elif axes[0][ax] or axes[1][ax]:
         print("histogram axis misalignment, type=", h1.ClassName(), "name=" + h1.GetName())
         print("   axis:", ax)
         return False
   if h1.GetEntries() != h2.GetEntries():
      print("histogram entries mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      print("   {}".format(h1.GetEntries()), "!= {}".format(h2.GetEntries()))
      return False
   if h1.GetSumw2N() != h2.GetSumw2N():
      print("histogram sumw2N mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      print("   {}".format(h1.GetSumw2N()), "!= {}".format(h2.GetSumw2N()))
      return False
   if not h1.ClassName() in datatype:
      print("unsupported histogram type", h1.ClassName(), "name=", h1.GetName())
      return False
   dtype = datatype[h1.ClassName()]
   v1 = np.frombuffer(h1.GetArray(), dtype=dtype, count=h1.GetNcells())
   v2 = np.frombuffer(h2.GetArray(), dtype=dtype, count=h2.GetNcells())
   if not np.allclose(v1, v2, rtol=1e-15, atol=1e-30):
      print("histogram contents mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      for i in range(0, len(v1)):
         if v1[i] != v2[i]:
            print("   cell {0}: {1} != {2}".format(i, v1[i], v2[i]))
      return False
   e1 = np.frombuffer(h1.GetSumw2().GetArray(), dtype=dtype, count=h1.GetSumw2N())
   e2 = np.frombuffer(h2.GetSumw2().GetArray(), dtype=dtype, count=h2.GetSumw2N())
   if not np.allclose(e1, e2, rtol=1e-15, atol=1e-30):
      print("histogram errors mismatch, type=", h1.ClassName(), "name=" + h1.GetName())
      return False
   return True

def TDirectory_equal(d1, d2):
   """
   Runs a recursive comparison between two TDirectory objects in a file,
   and returns false if any differences are found, otherwise true.
   """
   keys = [d1.GetListOfKeys(), d2.GetListOfKeys()]
   if len(keys[0]) != len(keys[1]):
      if not quiet:
         print("directory contents mismatch:",
               len(keys[0]), "!=", len(keys[1]))
         for key in keys[0]:
            print(" *  " + key.GetName())
         for key in keys[1]:
            print(" o " + key.GetName())
      return False
   for pair in zip(sorted(keys[0]), sorted(keys[1])):
      o1 = pair[0].ReadObj()
      o2 = pair[1].ReadObj()
      if o1.ClassName() != o2.ClassName():
         if not quiet:
            print("directory contents mismatch:",
                  o1.ClassName(), "!=", o2.ClassName(),
                  o1.GetName(), "!=", o2.GetName())
         return False
      elif o1.GetName() != o2.GetName():
         if not quiet:
            print("object name mismatch:",
                  o1.GetName(), "!=", o2.GetName())
         return False
      elif o1.InheritsFrom("TDirectory"):
         if not quiet:
            print("descending into directory", o1.GetPath(), "==", o2.GetPath())
         if not TDirectory_equal(o1, o2):
            return False
         if not quiet:
            print("back in directory", d1.GetPath(), "==", d2.GetPath())
      elif o1.InheritsFrom("TH1"):
         if not quiet:
            print("running comparison on histogram", o1.GetName(), "==", o2.GetName())
         if not TH1_equal(o1, o2):
            diffs.append(o1.GetDirectory().GetPath() + "/" + o1.GetName())
            #return False
            continue
      else:
         if not quiet:
            print("TDirectory_equal error -",
                  "no support for comparison of objects of class",
                  o1.ClassName())
         #return False
   return True

def TFile_equal(f1, f2):
   """
   Runs a recursive comparison between two TFiles named f1 and f2,
   and returns false if any differences are found, otherwise true.
   """
   rootf1 = ROOT.TFile(f1)
   rootf2 = ROOT.TFile(f2)
   global diffs
   diffs = []
   if TDirectory_equal(rootf1, rootf2):
      if len(diffs) == 0:
         print("files are identical")
      else:
         print("files are similar, with {0} differences".format(len(diffs)))
         for th1 in diffs:
            print("   " + th1)
   else:
      print("files are different")

quiet = False
while len(sys.argv) > 1 and sys.argv[1][:1] == "-":
   if sys.argv[1] == "-q":
      quiet = True
      del sys.argv[1]
   else:
      break
if len(sys.argv) == 3:
   if TFile_equal(sys.argv[1], sys.argv[2]):
      sys.exit(0)
   else:
      sys.exit(1)
else:
   usage()
