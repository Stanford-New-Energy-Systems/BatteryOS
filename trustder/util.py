
def all_different(items) -> bool:
    return len(set(items)) == len(items)

def all_same(items) -> bool:
    return len(set(items)) == 1

class Time:
    def __init__(self):
        pass

    def __call__(self):
        raise NotImplementedError

import time as systime
class SystemTime:
    def __init__(self):
        super().__init__()

    def __call__(self):
        return systime.time()

class DummyTime:
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
