
def all_different(items) -> bool:
    return len(set(items)) == len(items)

def all_same(items) -> bool:
    return len(set(items)) == 1

class Time:
    '''
    An abstract time class used by the BOS interpreter.
    TODO: This along with SystemTime, DummyTime, and bos_time
          belongs in a different file (e.g. Interpreter.py).
    '''
    
    def __init__(self):
        pass

    def __call__(self):
        raise NotImplementedError

import time as systime
class SystemTime:
    ''' This reports the real system time. This is what you want when testing on a real testbed. '''
    def __init__(self):
        super().__init__()

    def __call__(self):
        return systime.time()

class DummyTime:
    ''' This reports a fake, manipulatable time. This is only useful for testing purely virtual 
        topologies. 
    '''
    def __init__(self, time=0):
        super().__init__()
        self._time = time 

    def __call__(self):
        return self._time

    def set_time(self, time):
        assert self._time <= time
        self._time = time

    def tick(self, duration):
        assert duration >= 0
        self._time += duration

bos_time = SystemTime()

verbose = False
