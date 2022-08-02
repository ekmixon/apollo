#!/usr/bin/env python

import sys
import subprocess
import time
import os
from ssos import SSOS

def queryUniqueNames():
    SOS = SSOS()

    sos_host = "localhost"
    sos_port = os.environ.get("SOS_CMD_PORT")

    SOS = SSOS()

    SOS.init()
    SOS = SSOS()

    sql_string = """
    SELECT
    DISTINCT value_name 
    FROM viewCombined
    ;
    """
    results, col_names = SOS.query(sql_string, sos_host, sos_port)

    numeric_fields = {'name': [el[0] for el in results]}
    name_count = len(numeric_fields['name'])

    SOS = SSOS()

    SOS = SSOS()

    SOS.finalize();
    SOS = SSOS()

    print 
    #############
  
if __name__ == "__main__":
    queryUniqueNames()
    #############



