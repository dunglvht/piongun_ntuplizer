#!/bin/bash
cd /afs/cern.ch/work/v/vle/CMSSW_15_0_19/src/testanalyzer/testanalyzer/python/
export PATH=/bin/:$PATH
export LD_LIBRARY_PATH=/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/lib64/:$LD_LIBRARY_PATH
python3 submit_jobs.py