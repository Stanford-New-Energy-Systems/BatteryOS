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
public:
    /**
     * Node of the battery topology graph
     * @param battery the actual battery class
     * @param parents the parental nodes
     * @param children the children nodes
     */
    struct BatteryGraphNode {
        std::unique_ptr<Battery> battery;
        std::list<BatteryGraphNode*> parents;
        std::list<BatteryGraphNode*> children;
        BatteryGraphNode(std::unique_ptr<Battery> battery) : battery(std::move(battery)) {}
    };
    /** a mapping from name (string) to the actual battery node */
    std::map<std::string, BatteryGraphNode> name_map;

    BOSDirectory() {}
    BOSDirectory(const BOSDirectory &) = delete;
    BOSDirectory(BOSDirectory &&) = delete;
    BOSDirectory operator=(const BOSDirectory &) = delete;
    BOSDirectory operator=(BOSDirector &&) = delete; 

    BatteryGraphNode *add_battery(std::unique_ptr<Battery> &&battery) {
        std::string name = battery->get_name();
        if (name_map.count(name) > 0) {
            warning("battery name: ", name, " already exists, battery failed to add");
            return;
        }
        name_map[name] = BatteryGraphNode(std::move(battery));  // note: named rvalue is lvalue! 
        return &(name_map[name]);
    }

    // bool remove_battery(const std::string &name) {
    //     decltype(this->name_map)::iterator iter = name_map.find(name);
    //     if (iter == name_map.end()) {
    //         warning("battery name: ", name, " does not exist");
    //         return false;
    //     }
    //     BatteryGraphNode *node = &(iter->second);
    //     remove_battery_recursive(node);
    //     return true;
    // }

    BatteryGraphNode *get_node(const std::string &name) {
        auto iter = name_map.find(name);
        if (iter == name_map.end()) {
            warning("battery name: ", name, " does not exist");
            return nullptr;
        }
        return &(iter->second);
    }

    Battery *get_battery(const std::string &name) {
        BatteryGraphNode *node = get_node(name);
        if (!node) return nullptr;
        return node->battery.get();
    }

    bool name_exists(const std::string &name) {
        return (name_map.count(name) > 0);
    }

    bool add_edge(const std::string &from_name, const std::string &to_name) {
        auto from_iter = name_map.find(from_name);
        auto to_iter = name_map.find(to_name);
        if (from_iter == name_map.end()) {
            warning("battery name: ", from_name, " does not exist");
            return false;
        }
        if (to_iter == name_map.end()) {
            warning("battery name: ", to_name, " does not exist");
            return false;
        }
        from_iter->second.children.push_back(&(to_iter->second));
        to_iter->second.parents.push_back(&(from_iter->second));
        return true;
    }

    std::vector<std::string> get_names() {
        std::vector<std::string> names;
        for (std::pair<std::string, BatteryGraphNode> &p : name_map) {
            names.push_back(p.first);
        }
        return names;
    }

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

