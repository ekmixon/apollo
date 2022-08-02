import numpy as np
import pandas as pd

from collections import defaultdict


def check_int(s):
    return s[1:].isdigit() if s[0] is ('-', '+') else s.isdigit()


class Loader(object):
    def __init__(self, filename, names):
        self.filename = filename
        self.names = names
        self._data = None


    def get_data(self):
        """
        Return loaded data.

        :returns: A numpy array containing all loaded data. The array will have
                    field names specified by the dtype used to load the data.
        """
        return self._data


class PandasInstructionLoader(Loader):
    def __init__(self, filename):
        imaps = []
        for line in open(filename):
            loop = line.split(",")[0].split(':')[1].lstrip()
            imap = {'loop': str(loop)}
            for instructioncount in line.split(",")[1:]:
                instruction, count = instructioncount.split(":")
                imap[instruction] = int(count)

            imaps.append(imap)

        data = pd.DataFrame(imaps)
        data.drop_duplicates(subset=['loop'],inplace=True)
        data = data[data.loop != 0]
        super(PandasInstructionLoader, self).__init__(filename, list(data))
        self._data = data


class PandasCaliperLoader(Loader):
    def __init__(self, filename, id_column='loop_id'):
        maps = []
        max_features = None

        for line in open(filename):
            fmap = {}
            line = line.rstrip()

            num_features = len(line.split(','))

            if max_features is None:
                max_features = num_features

            if num_features < max_features:
                print "Only %d features in line %s " % (num_features, line)
                exit(-1)

            for feature in line.split(","):
                f = feature.split('=')[0]
                v = feature.split('=')[1]

                if check_int(v):
                    fmap[f] = int(v)
                else:
                    fmap[f] = v

            maps.append(fmap)

        data = pd.DataFrame(maps)
        super(PandasCaliperLoader, self).__init__(filename, list(data))
        self._data = data

class PandasCsvLoader(Loader):
    def __init__(self, filename, id_column='loop_id'):
        data = pd.read_csv(filename)
        super(PandasCsvLoader, self).__init__(filename, list(data))
        self._data = data


def load_csv(app_data, instruction_data=None):
    ldata = PandasCsvLoader(app_data, id_column='loop_count').get_data()

    if instruction_data:
        idata = PandasInstructionLoader(instruction_data).get_data().fillna(0)
        return ldata, idata
    else:
        return ldata

def load(app_data, instruction_data=None):
    ldata = PandasCaliperLoader(app_data, id_column='loop_count').get_data()

    if instruction_data:
        idata = PandasInstructionLoader(instruction_data).get_data().fillna(0)
        return ldata, idata
    else:
        return ldata


def load_app(app_data):
    return PandasCaliperLoader(app_data, id_column='loop_count').get_data()


def load_inst(instruction_data):
    return PandasInstructionLoader(instruction_data).get_data().fillna(0)
