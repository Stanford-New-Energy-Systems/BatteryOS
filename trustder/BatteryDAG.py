import BatteryInterface
import BOSErr

class BatteryDAG:
    def __init__(self):
        self._children = set()
        self._parents = set()
        self._batteries = dict()

    def _name_taken(self, name: str) -> bool:
        return name in self._batteries

    def _check_name_free(self, name: str):
        if self._name_taken(name):
            raise BOSErr.NameTaken

    def _check_name_taken(self, name: str):
        if not self._name_taken:
            raise BOSErr.BadName

    def _check_add_parent(self, parent: str):
        self._check_name_taken(parent)
        if parent in self._children:
            raise BOSErr.BatteryInUse

    def _check_add_child(self, child: str):
        self._check_name_free(child)

    def _check_add_children(self, children: [str]):
        for child in children:
            self._check_add_child(child)
        if not all_different(children):
            raise BOSErr.InvalidArgument

    def _check_add_parents(self, parents: [str]):
        for parent in parents:
            self._check_add_parent(parent)
        if not all_different(parents):
            raise BOSErr.InvalidArgument

        
    def add(self, child: str, parent: str, battery: BatteryInterface.Battery):
        self._check_add_child(child)
        self._check_add_parent(parent)

        self._parents[child] = set(parent)
        self._children[parent] = set(child)
        self._batteries[child] = battery


    def add_children(self, children, parent):
        # children is dict from name to battery
        self._check_add_children(children)
        self._check_add_parent(parent)

        self._children[parent] = set(children.keys())
        for child in children:
            self._parents[child] = set(parent)
            self._batteries[child] = children[child]

            
    def add_parents(self, child: str, parents: [str], battery):
        self._check_add_child(child)
        self._check_add_parents(parents)
        
        for parent in parents:
            self._children[parent] = child
        self._parents[child] = set(parents)
        self._batteries[child] = battery


    def remove_children(self, name: str):
        self._check_name_taken(name)

        if name not in self._children: return
        children = self._children[name]

        # check if children are leaves
        for child in children
            if child in self._children:
                raise BOSErr.BatteryInUse

        for child in children
            assert self._parents.get(child) == {name}
            del self._parents[child]
            del self._batteries[child]

        del self._children[name]

        
    def get_parents(self, name: str) -> [str]:
        self._check_name_taken(name)
        return self._parents.get(name, set())

    def get_children(self, name: str) -> [str]:
        self._check_name_taken(name)
        return self._children.get(name, set())

    def get_battery(self, name: str) -> BatteryInterface.Battery:
        self._check_name_taken(name)
        return self._batteries[name]

    
    def rename(self, oldname: str, newname: str):
        self._check_name_taken(oldname)
        if oldname == newname: return
        self._check_name_free(newname)
        if oldname in self._children:
            children = self._children[oldname]
            del self._children[oldname]
            self._children[newname] = children
            for child in children:
                parents = self._parents[child]
                parent.remove(oldname)
                parent.add(newname)
        if oldname in self._parents:
            parents = self._parents[oldname]
            del self._parents[oldname]
            self._parents[newname] = parents
            for parent in parents:
                children = self._children[parent]
                children.remove(oldname)
                children.add(newname)

    def serialize(self) -> str:
        names = set(self._batteries.keys())
        assert names == (set(self._parents.keys()) | set(self._children.keys())) # correctness

        entries = dict()
        for name in names:
            entry = dict()
            entry["parents"] = list(self._parents.get(name, set()))
            entry["children"] = list(self._children.get(name, set()))
            entry["battery"] = self._battery[name].serialize()
            entries[name] = entry
        
        return json.dumps(entries)
