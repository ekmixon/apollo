#!/usr/bin/env python3

# Copyright (c) 2019, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
#
# This file is part of Apollo.
# OCEC-17-092
# All rights reserved.
#
# Apollo is currently developed by Chad Wood, wood67@llnl.gov, with the help
# of many collaborators.
#
# Apollo was originally created by David Beckingsale, david@llnl.gov
#
# For details, see https://github.com/LLNL/apollo.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.


import os
import sys
import time
import pickle

from ssos import SSOS

import apollo.trees as trees
import apollo.query as query
import apollo.utils as utils

from apollo.debug import log
from apollo.config import VERBOSE
from apollo.config import DEBUG
from apollo.config import FRAME_INTERVAL
from apollo.config import ONCE_THEN_EXIT

##########

def main():
    controller_start = time.time()
    SOS = SSOS()
    SOS.init()

    sos_host = "localhost"
    sos_port = os.environ.get("SOS_CMD_PORT")

    step = 0
    prior_frame_max = 0

    log(1, "Online.")
    print("CREATE VIEW", flush=True) # ggout
    query.createApolloView(SOS, sos_host, sos_port)

    #log(1, "Wiping all prior data in the SOS database...")
    print("WIPE DATA", flush=True) # ggout
    query.wipeAllExistingData(SOS, sos_host, sos_port)
    prev_dtree_def = None

    triggers = 0
    while (os.environ.get("SOS_SHUTDOWN") != "TRUE"):
        # Clearing prior training data
        # query.wipeTrainingData(SOS, sos_host, sos_port, prior_frame_max)
        prior_frame_max    = query.waitForMoreRowsUsingSQL(
                                SOS, sos_host, sos_port,
                                prior_frame_max)
        data, region_names = query.getTrainingData(SOS, sos_host, sos_port, row_limit=0);
        #print('data', data)
        #print('region_names', region_names)
        dataset_guid = SOS.get_guid()
        data.to_pickle("./output/models/step.%d.trainingdata.pickle" % prior_frame_max)
        with open(("./output/models/step.%d.region_names.pickle" % prior_frame_max), "wb") as f:
            pickle.dump(region_names, f)
        print("Pickled step ",prior_frame_max) # ggout
        continue
            ##### return to top of loop until shut down #####


    ########## end of controller.py  ##########
    log(1, "Done.")
    return

#########



if __name__ == "__main__":
    main()
