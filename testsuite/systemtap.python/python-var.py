import sys
import types

# Try to create instances of (most) every python type.

def ret0():
    return 0

class xqueue:
    def __init__(self):
        self.items = []
    def insert(self, item):
        self.items.append(item)
    def remove(self):
        return self.items.pop()

#
# Base types: None, type, and bool
#
g_none = None
g_type = type(g_none)
g_bool = True

#
# Numeric types: int, long, float, and complex
#
# Note that in python 2, there are 2 integer types 'int' and
# 'long'. To test both, we've got two integer variables, one with a
# small value, one with a large value. In python 3, there is no 'int'
# type.
g_int = 9
g_long = 0x7eadbeeffeedbabe
g_float = 0.5
g_complex = 1.2 + 12.34J

#
# Sequence types: strings, unicode strings, lists, tuples, and xrange
# (python 2 only)
#
# In python 2, there are 2 string types 'string' and 'unicode'. We'll
# test both. In python 3, there are really just unicode strings, but
# this should still work.
g_string = 'regular string'
g_unicode = u'unicode string'
g_tuple = (0, 'abc', 2)
g_list = [0, 1, 2, 3]
# The 'xrange' type only exists in python 2.
if sys.version_info[0] == 2:
    g_xrange = xrange(10)

#
# Mapping types: dictionary
#
g_dictionary = {'Bacon': 1, 'Ham': 0, 'Ribs': 2}

#
# Callable objects: functions, methods, classes, class instances,
# unbound class methods
#
g_func = ret0
g_class = xqueue
g_unbound_method = xqueue.insert
g_instance = xqueue()
g_method = g_instance.insert

#
# Misc types: zip, file, ellipsis
#
g_zip = zip(g_list, [4, 5, 6])
g_file = sys.stdout
g_ellipsis = Ellipsis

def main():
    l_none = None
    l_type = type(g_bool)
    l_bool = False

    l_int = 15
    l_long = 0x7eedfacecafebeef
    l_float = 0.33
    l_complex = 2.33 + 23.45J

    l_string = 'another regular string'
    l_unicode = u'another unicode string'
    l_tuple = ('hello', 99, 'there', 0xbeef)
    l_list = [4, 5, 6, 7, 8, 9, 10]
    if sys.version_info[0] == 2:
        l_xrange = xrange(10)

    # Notice this dictionary has mixed numeric and string indices.
    l_dictionary = {1: 'numeric index', 'abc': 3}

    l_func = ret0
    l_class = xqueue
    l_unbound_method = xqueue.remove
    l_instance = xqueue()
    l_method = g_instance.remove

    l_zip = zip(l_list, g_list, [11, 12, 13, 14, 15])
    l_file = sys.stderr
    l_ellipsis = Ellipsis
    return 0

if __name__ == "__main__":
    sys.exit(main())

