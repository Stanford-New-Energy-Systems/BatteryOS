import BatteryInterface
import BOS

class BOSControl:
    """
    Control API for BOS: virtual battery management, etc.
    """

    def __init__(self):
        pass

    # Add a physical battery to BOS
    # Returns the battery ID (used for removal, update, etc.)
    def add_physical_battery(self, battery: BatteryInterface.Battery) -> int:
        pass

    # Remove a physical battery
    # Return whether the removal was successful
    # TODO: What to do when removing the battery would oversubscribe
    def remove_physical_battery(self, bid: int) -> bool:
        pass

    # Update a physical battery atomically
    # Returns whether the update occurred (bool) and if it did, what the battery ID is.
    def update_physical_battery(self, bid: int, new_battery: BatteryInterface.Battery) -> (bool, int):
        pass

    # Add a virtual battery
    # Returns whether it was successfully added and, if so, the battery ID
    def add_virtual_battery(self, vb: BOS.VirtualBattery) -> (bool, int):
        pass

    # Remove a virtual battery
    # Returns whether removal was successful (might not be if invalid battery ID)
    def remove_virtual_battery(self, bid: int) -> bool:
        pass

    # Update a virtual battery
    # Returns whether the update occurred (bool) and if so, the new battery ID
    def update_virtual_battery(self, bid: int, new_battery: BOS.VirtualBattery) -> (bool, int):
        pass

    # Get a full list of physical batteries, as a map from battery IDs to battery objects
    def get_physical_batteries(self) -> dict[int, BatteryInterface.Battery]:
        pass

    # Get a full list of virtual batteries, as a map from battery IDs to battery objects
    def get_virtual_batteries(self) -> dict[int, BOS.VirtualBattery]:
        pass

    # Get the JSON representation of the current configuration of physical & virtual batteries
    def get_configuration(self) -> str:
        pass

    # Set the current configuration of physical & virtual batteries given JSON representation
    # Returns whether successful
    def set_configuration(self, json: str) -> bool:
        pass
        
