#ifndef BOSDIRECTORY_HPP
#define BOSDIRECTORY_HPP

#include <map>
#include <deque>
#include <list>
#include "BatteryInterface.hpp"

/**
 * A graph of the battery topology
 */
class BOSDirectory {
    std::map<std::string, std::unique_ptr<Battery>> name_storage_map;
    std::map<std::string, std::list<Battery*>> parent_map;
    std::map<std::string, std::list<Battery*>> children_map;
    std::list<Battery*> temp;
public:
    BOSDirectory() {}
    BOSDirectory(BOSDirectory &) = delete;
    BOSDirectory &operator=(BOSDirectory &) = delete;
    BOSDirectory(BOSDirectory &&other) : 
        name_storage_map(std::move(other.name_storage_map)),
        parent_map(std::move(other.parent_map)),
        children_map(std::move(other.children_map)) {}
    BOSDirectory &operator=(BOSDirectory &&other) {
        name_storage_map.swap(other.name_storage_map);
        parent_map.swap(other.parent_map);
        children_map.swap(other.children_map);
        return (*this);
    } 

    Battery *add_battery(std::unique_ptr<Battery> &&battery) {
        std::string name = battery->get_name();
        if (this->name_storage_map.count(name) > 0) {
            WARNING() << "battery name: " << name << " already exists, battery failed to add";
            return nullptr;
        }
        std::pair<decltype(this->name_storage_map)::iterator, bool> result = 
            this->name_storage_map.insert(std::make_pair(name, std::move(battery)));

        Battery *ptr = result.first->second.get();
        parent_map.insert(std::make_pair(name, std::list<Battery*>()));
        children_map.insert(std::make_pair(name, std::list<Battery*>()));

        return ptr;  // be careful to avoid the copy constructor and the default constructor! 
    }

    const std::list<Battery*> &get_children(const std::string &name) {
        auto iter = this->children_map.find(name);
        if (iter == this->children_map.end()) {
            WARNING() << "battery name: " << name << " does not exist";
            return temp;
        }
        return iter->second;
    }

    const std::list<Battery*> &get_parents(const std::string &name) {
        auto iter = this->parent_map.find(name);
        if (iter == this->parent_map.end()) {
            WARNING() << "battery name: " << name << " does not exist";
            return temp;
        }
        return iter->second;
    }

    bool add_edge(const std::string &from_name, const std::string &to_name) {
        auto from_iter = this->name_storage_map.find(from_name);
        auto to_iter = this->name_storage_map.find(to_name);
        if (from_iter == this->name_storage_map.end()) {
            WARNING() << "battery name: " << from_name << " does not exist";
            return false;
        }
        if (to_iter == this->name_storage_map.end()) {
            WARNING() << "battery name: " << to_name << " does not exist";
            return false;
        }
        Battery *from_ptr = from_iter->second.get();
        Battery *to_ptr = to_iter->second.get();

        // add locks here

        VirtualBattery *from_vb = dynamic_cast<VirtualBattery*>(from_ptr);
        VirtualBattery *to_vb = dynamic_cast<VirtualBattery*>(to_ptr);
        if (from_vb != nullptr) { from_vb->mutex_lock(); }
        if (to_vb != nullptr) { to_vb->mutex_lock(); }
        this->children_map[from_name].push_back(to_ptr);
        this->parent_map[to_name].push_back(from_ptr);
        if (from_vb != nullptr) { from_vb->mutex_unlock(); }
        if (to_vb != nullptr) { to_vb->mutex_unlock(); }
        return true;
    }


    Battery *get_battery(const std::string &name) {
        auto iter = this->name_storage_map.find(name);
        if (iter == this->name_storage_map.end()) {
            WARNING() << "name " << name << " does not exist";
            return nullptr;
        }
        return iter->second.get();
    }

    bool name_exists(const std::string &name) {
        return (this->name_storage_map.count(name) > 0);
    }

    std::vector<std::string> get_names() {
        std::vector<std::string> names;
        for (auto &p : this->name_storage_map) {
            names.push_back(p.first);
        }
        return names;
    }


    // bool remove_battery(const std::string &name) {
    //     decltype(this->name_map)::iterator iter = name_map.find(name);
    //     if (iter == name_map.end()) {
    //         WARNING() << "battery name: " << name << " does not exist";
    //         return false;
    //     }
    //     BatteryGraphNode *node = &(iter->second);
    //     remove_battery_recursive(node);
    //     return true;
    // }

    // void remove_battery_recursive(BatteryGraphNode *node) {
    //     for (BatteryGraphNode *p : node->parents) {
    //         p->children.remove(node);
    //     }
    //     for (BatteryGraphNode *c : node->children) {
    //         if (c->parents.size() == 1) {
    //             remove_battery_recursive(c);
    //         } else {
    //             c->parents.remove(node);
    //         }
    //     }
    //     std::string name = node->battery->get_name();
    //     name_map.erase(name);
    // }
};

#endif // ! BOSDIRECTORY_HPP

