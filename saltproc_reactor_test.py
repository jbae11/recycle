"""
This python file contains fleet based unit tests for NO_INST
archetype. 

"""

import json
import re
import subprocess
import os
import sqlite3 as lite
import pytest
import copy
import glob
import sys
from matplotlib import pyplot as plt
import numpy as np
import h5py
from collections import OrderedDict


from nose.tools import assert_in, assert_true, assert_equals

# Delete previously generated files
direc = os.listdir('./')
hit_list = glob.glob('*.sqlite') + glob.glob('*.json')
for file in hit_list:
    os.remove(file)

ENV = dict(os.environ)
ENV['PYTHONPATH'] = ".:" + ENV.get('PYTHONPATH', '')

def get_cursor(file_name):
    """ Connects and returns a cursor to an sqlite output file

    Parameters
    ----------
    file_name: str
        name of the sqlite file

    Returns
    -------
    sqlite cursor3
    """
    con = lite.connect(file_name)
    con.row_factory = lite.Row
    return con.cursor()

TEMPLATE = {
    "simulation": {
        "archetypes": {
            "spec": [
                {"lib": "agents", "name": "NullRegion"},
                {"lib": "cycamore", "name": "Source"},
                {"lib": "cycamore", "name": "Sink"},
                {"lib": "agents", "name": "NullInst"},
                {"lib": "saltproc_reactor.saltproc_reactor", "name": "saltproc_reactor"}
            ]
        },
        "control": {"duration": "10", "startmonth": "1", "startyear": "2000"},
        "recipe": [
            {
                "basis": "mass",
                "name": "fresh_uox",
                "nuclide": [{"comp": "0.711", "id": "U235"}, {"comp": "99.289", "id": "U238"}]
            },
            {
                "basis": "mass",
                "name": "spent_uox",
                "nuclide": [{"comp": "50", "id": "Kr85"}, {"comp": "50", "id": "Cs137"}]
            }
        ],
        "facility": [{
            "config": {"Source": {"outcommod": "init_fuel",
                                  "outrecipe": "fresh_uox",
                                  "throughput": "3000"}},
            "name": "source"
        },
        {
                    "config": {"Source": {"outcommod": "fill",
                                      "outrecipe": "fresh_uox",
                                      "throughput": "3000"}},
                "name": "fill_source"
        },
        {
            "config": {"Sink": {"in_commods": {"val": ["waste", "end_fuel"]}, 
                                "max_inv_size": "1e6"}},
            "name": "sink"
        },
        {
            "config": {"saltproc_reactor": OrderedDict({'init_fuel_commod': 'init_fuel',
                                                        'final_fuel_commod': 'fuel_out',
                                                        'fill_commod': 'fill',
                                                        'fissile_out_commod': 'fissile_out',
                                                        'waste_commod': 'waste',
                                                        'db_path': './testdb.hdf5',
                                                        'power_cap': '100'})
            },
            "name": "reactor",
            "lifetime": 5
        }]
    }
}

# add region
TEMPLATE["simulation"].update({"region": {
    "config": {"NullRegion": "\n      "},
    "institution": {
        "config": {
            "NullInst": {},
        },
    "initialfacilitylist": {'entry': [{'prototype': 'fill_source', 'number':'1'},
                                      {'prototype': 'source', 'number': '1'},
                                      {'prototype': 'sink', 'number': '1'}, 
                                      {'prototype': 'reactor', 'number': '1'} ]
                           },
    "name": "source_inst",
    },
    "name": "SingleRegion"
}})

def test_run_input():
    output_file = 'test_output.sqlite'
    input_file = output_file.replace('.sqlite', '.json')
    with open(input_file, 'w') as f:
        json.dump(TEMPLATE, f)
    s = subprocess.check_output(['cyclus', '-o', output_file, input_file],
                             universal_newlines=True)
    assert('Cyclus run successful!' in s)
