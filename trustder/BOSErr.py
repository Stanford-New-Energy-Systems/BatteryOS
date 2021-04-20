class BOSErr(Exception): pass
class NameTaken(BOSErr): pass
class BadName(BOSErr): pass
class VoltageMismatch(BOSErr): pass
class NoBattery(BOSErr): pass
class BatteryInUse(BOSErr): pass
class InvalidArgument(BOSErr): pass
class CurrentOutOfRange(BOSErr): pass