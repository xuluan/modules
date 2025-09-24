#!/usr/bin/env python3

from enum import Enum
import array
import math
import argparse
from sys import argv

class DataFormat(Enum):
    FORMAT_ANY = 0
    FORMAT_1BIT = 1
    FORMAT_U8 = 2
    FORMAT_U16 = 3
    FORMAT_R32 = 4
    FORMAT_U32 = 5
    FORMAT_R64 = 6
    FORMAT_U64 = 7

def get_type_code(data_type):
    if data_type == DataFormat.FORMAT_U8.value:
        type_code = 'B'   # unsigned char
    elif data_type == DataFormat.FORMAT_U16.value:
        type_code = 'H'   # unsigned short
    elif data_type == DataFormat.FORMAT_U32.value:
        type_code = 'I'   # unsigned int
    elif data_type == DataFormat.FORMAT_U64.value:
        type_code = 'L'   # unsigned long
    elif data_type == DataFormat.FORMAT_R32.value:
        type_code = 'f'   # float
    elif data_type == DataFormat.FORMAT_R64.value:
        type_code = 'd'   # double
    else:
        raise Exception(f'data_type:{data_type} is not supported')
    
    return type_code

def read_from_file(filename, group_size, trace_length, data_type):

    trc_data_list = []
    
    try:
        fin = open(filename, "rb")

        for i in range(group_size):
            values = array.array(get_type_code(data_type))
            values.fromfile(fin, trace_length)
            trc_data_list.append(values.tolist())

        fin.close()
    except Exception as e:
        print("read_from_file() error: " + str(e))
        raise(e)

    return trc_data_list


def write_to_file(filename, trc_data_list, data_type):

    try:
        fout = open(filename, "wb")

        for i in range(len(trc_data_list)):
            values = array.array(get_type_code(data_type), trc_data_list[i])
            values.tofile(fout)

        fout.close()
    except Exception as e:
        print("ERROR: " + str(e))


def get_agc_data(data, dt, gate_length):
    v1 = data['result_data']
    data_width = len(v1)
    data_height = len(v1[0])

    output = {
        'result_data': [],
    }

    radius = max(1, int((gate_length + 0.00001) / dt / 2))

    for x in range(data_width):
        trace = [0] * data_height
        sum_ = 0
        n = 0

        for y in range(min(radius, data_height)):
            sum_ += abs(v1[x][y])
            n += 1

        for y in range(data_height):
            if y > radius:
                sum_ -= abs(v1[x][y - radius - 1])
                n -= 1
            if y + radius < data_height:
                sum_ += abs(v1[x][y + radius])
                n += 1

            if sum_ != 0 and n > 0:
                value = (v1[x][y] * n) / sum_
            else:
                value = 0

            trace[y] = value

        output['result_data'].append(trace)
    return output


def get_diverge_data(data, o1, dt, a, V):
    v1 = data['result_data']
    data_width = len(v1)
    data_height = len(v1[0])

    output = {
        'result_data': [],
    }

    times = [o1 + dt * y for y in range(data_height)]

    for x in range(data_width):
        trace = [0] * data_height
        for y in range(data_height):
            value = v1[x][y] * math.pow(times[y], a) * V

            trace[y] = value


        output['result_data'].append(trace)
    return output


def scale_factor(attrname, group_size, trace_length, data_type, factor=0.5):
    """
    Apply constant factor scaling, and save the results to a new file named ‘.FCT’.
    
    Args:
        attrname: attribute name
        group_size:
        trace_length: 
        datatype: 
        factor: scaling factor
    """


    trc_data_list = read_from_file(attrname+'.DAT', group_size, trace_length, data_type)

    print("orig: " + str(trc_data_list[0][:10]))

    for trc_data in trc_data_list:
        for i in range(len(trc_data)):
            if data_type ==  DataFormat.FORMAT_R64.value or data_type ==  DataFormat.FORMAT_R32.value:
                trc_data[i] = trc_data[i] * factor
            else:
                trc_data[i] = int(trc_data[i] * factor)

    print("factor: " + str(trc_data_list[0][:10]))

    write_to_file(attrname+'.FCT', trc_data_list ,data_type)


def scale_agc(attrname, group_size, trace_length, data_type,  sinterval,  window_size=500):
    """
    Apply AGC scaling, and save the results to a new file named ‘.AGC’.
    
    Args:
        attrname: attribute name
        group_size:
        trace_length: 
        datatype: 
        window_size: window length in ms

    """

    trc_data_list = read_from_file(attrname+'.DAT', group_size, trace_length, data_type)

    print("orig: " + str(trc_data_list[0][:10]))

    ret = get_agc_data({'result_data':trc_data_list}, sinterval, window_size)
    agc_data_list = ret['result_data']

    # agc_data_list[0][0] = 0.0

    print("agc: " + str(agc_data_list[0][:10]))

    write_to_file(attrname+'.AGC', agc_data_list ,data_type)


def scale_diverge(attrname, group_size, trace_length, data_type, tmin, sinterval, a=2.0, v=1.0):
    """
     Apply divergence scaling, and save the results to a new file named ‘.DVG’.
    
    Args:
        attrname: attribute name
        group_size:
        trace_length: 
        datatype: 
        a: user-defined exponent (typically 1.0-2.0, default 2.0)
        v: speed factor / magnification factor (default 1.0)

    """

    trc_data_list = read_from_file(attrname+'.DAT', group_size, trace_length, data_type)

    print("orig: " + str(trc_data_list[0][:10]))

    ret = get_diverge_data({'result_data':trc_data_list}, tmin, sinterval, a, v)
    dvg_data_list = ret['result_data']

    # agc_data_list[0][0] = 100.0
    print("diverge: " + str(dvg_data_list[0][:10]))


    write_to_file(attrname+'.DVG', dvg_data_list ,data_type)


if __name__ == "__main__":

    mode_choices = ['factor', 'agc', 'diverge']

    parser = argparse.ArgumentParser()

    parser.add_argument('-m', '--method', required=True, choices=mode_choices,
                        help='scaling method.')

    parser.add_argument('--attrname', required=True,
                        help='attribute name.')

    parser.add_argument('--group_size',
                    type=int, required=True,
                    help='group size.')

    parser.add_argument('--trace_length',
                    type=int, required=True,
                    help='trace data length.')

    parser.add_argument('--data_type',
                    type=int, required=True,
                    choices=[2, 3, 4, 5, 6 ,7],
                    help='date type. 2: unsigned char, 3: unsigned short, 4: float, 5: unsigned int, 6: double, 7: unsigned long')

    # parser.add_argument('-f', 
    #                 required=(mode_choices[0] in argv), 
    #                 default=0.5,
    #                 type=float, help='scaling factor, required if mode is "factor"')

    parser.add_argument('--sinterval',
                    required=(mode_choices[1] in argv or mode_choices[2] in argv), 
                    type=float, help='sinterval, required if mode in ["agc", "diverge"]')

    parser.add_argument('--tmin',
                    required=(mode_choices[2] in argv), 
                    type=float, help='tmin, required if mode is "diverge"')



    # parser.add_argument('-a',
    #                 required=(mode_choices[2] in argv), 
    #                 type=float, help='divergence compensation index, required if mode is "diverge"')

    # parser.add_argument('-v',
    #                 required=(mode_choices[2] in argv), 
    #                 type=float, help='velocity coefficient, required if mode is "diverge"')

    args = parser.parse_args()

    if args.method == 'factor':
    
        scale_factor(attrname=args.attrname, 
            group_size=args.group_size,
            trace_length=args.trace_length, 
            data_type=args.data_type,
            factor=0.5)
        
    elif args.method == 'agc':
        scale_agc(attrname=args.attrname, 
            group_size=args.group_size,
            trace_length=args.trace_length, 
            data_type=args.data_type,
            sinterval=args.sinterval,
            window_size=500)
    elif args.method == 'diverge':
        scale_diverge(attrname=args.attrname,
            group_size=args.group_size,
            trace_length=args.trace_length, 
            data_type=args.data_type,
            sinterval=args.sinterval,
            tmin=args.tmin,
            a=2.0, v=1.0)
    
    print("done!")
