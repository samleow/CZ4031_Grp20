import queue
import json
import copy
import re

# node attribute keys
NODE_TYPE = 'Node Type'
RELATION_NAME = 'Relation Name'
SCHEMA = 'Schema'
ALIAS = 'Alias'
GROUP_KEY = 'Group Key'
SORT_KEY = 'Sort Key'
JOIN_TYPE = 'Join Type'
INDEX_NAME = 'Index Name'
HASH_COND = 'Hash Cond'
FILTER = 'Filter'
INDEX_COND = 'Index Cond'
MERGE_COND = 'Merge Cond'
RECHECK_COND = 'Recheck Cond'
JOIN_FILTER = 'Join Filter'
ACTUAL_TOTAL_TIME = 'Actual Total Time'
SUBPLAN_NAME = 'Subplan Name'
PLANS = 'Plans'
PLAN_ROWS = 'Plan Rows'

# Node class for each QEP step
class Node(object):
    def __init__(self, attributes):
        self.children = []
        self.attributes = attributes.copy()

    def add_children(self, child):
        self.children.append(child)

    def set_name(self, name):
        if "T" == name[0] and name[1:].isdigit():
            self.name = int(name[1:])
        else:
            self.name = name

    def get_name(self):
        if str(self.name).isdigit():
            return "T" + str(self.name)
        else:
            return self.name

    def set_step(self, step):
        self.step = step

# parse json into a tree
def parse_json(json_obj):
    current_tree = queue.Queue()
    parent_tree = queue.Queue()
    current_tree.put(json_obj[0]['Plan'])
    parent_tree.put(None)

    while not current_tree.empty():
        current_plan = current_tree.get()
        parent_plan = parent_tree.get()

        current_node = Node(current_plan)
        if SUBPLAN_NAME in current_node.attributes:
            if "returns" in current_node.attributes[SUBPLAN_NAME]:
                n = current_node.attributes[SUBPLAN_NAME]
                current_node.attributes[SUBPLAN_NAME] = n[n.index("$"):-1]

        if "Scan" in current_node.attributes[NODE_TYPE]:
            if "Index" in current_node.attributes[NODE_TYPE]:
                if current_node.attributes[RELATION_NAME]:
                    current_node.set_name(current_node.attributes[RELATION_NAME] + " with index " + current_node.attributes[INDEX_NAME])
            elif "Subquery" in current_node.attributes[NODE_TYPE]:
                current_node.set_name(current_node.attributes[ALIAS])
            else:
                current_node.set_name(current_node.attributes[RELATION_NAME])

        if parent_plan is not None:
            parent_plan.add_children(current_node)
        else:
            head_node = current_node

        if PLANS in current_plan:
            for item in current_plan[PLANS]:
                current_tree.put(item)
                parent_tree.put(current_node)

    return head_node

# returns the tree as a string
def tree_as_string(node):
    global steps, current_step, current_table_name, table_subquery_name_pair
    global current_plan_tree
    steps = ["The query is executed as follow."]
    current_step = 1
    current_table_name = 1
    table_subquery_name_pair = {}

    node_as_string(node)
    if " to get intermediate table" in steps[-1]:
        steps[-1] = steps[-1][:steps[-1].index(" to get intermediate table")] + ' to get the final result.'
    steps_str = ''.join(steps)
    return steps_str

# returns the node as a string
def node_as_string(node, skip=False):
    global steps, current_step, current_table_name
    increment = True
    if node.attributes[NODE_TYPE] in ["Unique", "Aggregate"] and len(node.children) == 1 \
            and ("Scan" in node.children[0].attributes[NODE_TYPE] or node.children[0].attributes[NODE_TYPE] == "Sort"):
        children_skip = True
    elif node.attributes[NODE_TYPE] == "Bitmap Heap Scan" and node.children[0].attributes[NODE_TYPE] == "Bitmap Index Scan":
        children_skip = True
    else:
        children_skip = False

    for child in node.children:
        if node.attributes[NODE_TYPE] == "Aggregate" and len(node.children) > 1 and child.attributes[NODE_TYPE] == "Sort":
            node_as_string(child, True)
        else:
            node_as_string(child, children_skip)

    if node.attributes[NODE_TYPE] == "Hash" or skip:
        return
    step = ""

    if "Join" in node.attributes[NODE_TYPE]:
        if node.attributes[JOIN_TYPE] == "Semi":
            node_type_list = node.attributes[NODE_TYPE].split()
            node_type_list.insert(-1, node.attributes[JOIN_TYPE])
            node.attributes[NODE_TYPE] = " ".join(node_type_list)
        else:
            pass

        if "Hash" in node.attributes[NODE_TYPE]:
            step += " and perform " + node.attributes[NODE_TYPE].lower() + " on "
            for i, child in enumerate(node.children):
                if child.attributes[NODE_TYPE] == "Hash":
                    child.set_name(child.children[0].get_name())
                    hashed_table = child.get_name()
                if i < len(node.children) - 1:
                    step += ("table " + child.get_name())
                else:
                    step += (" and table " + child.get_name())
            step = "hash table " + hashed_table + step + " under condition " + node.attributes[HASH_COND]

        elif "Merge" in node.attributes[NODE_TYPE]:
            step += "perform " + node.attributes[NODE_TYPE].lower() + " on "
            any_sort = False
            for i, child in enumerate(node.children):
                if child.attributes[NODE_TYPE] == "Sort":
                    child.set_name(child.children[0].get_name())
                    any_sort = True
                if i < len(node.children) - 1:
                    step += ("table " + child.get_name())
                else:
                    step += (" and table " + child.get_name())
            if any_sort:
                sort_step = "sort "
                for child in node.children:
                    if child.attributes[NODE_TYPE] == "Sort":
                        if i < len(node.children):
                            sort_step += ("table " + child.get_name())
                        else:
                            sort_step += (" and table " + child.get_name())
                step = sort_step + " and " + step

    elif "Scan" in node.attributes[NODE_TYPE]:
        if node.attributes[NODE_TYPE] == "Seq Scan":
            step += "perform sequential scan on table "
            step += node.get_name()
        elif node.attributes[NODE_TYPE] == "Bitmap Heap Scan":
            if "Bitmap Index Scan" in node.children[0].attributes[NODE_TYPE]:
                node.children[0].set_name(node.attributes[RELATION_NAME])
                step += "performs bitmap heap scan on table " + node.children[0].get_name() +\
                        " with index condition " + node.children[0].attributes[INDEX_COND]
        else:
            step += "perform " + node.attributes[NODE_TYPE].lower() + " on table "
            step += node.get_name()

        if INDEX_COND not in node.attributes and FILTER not in node.attributes:
            increment = False

    elif node.attributes[NODE_TYPE] == "Unique":
        if "Sort" in node.children[0].attributes[NODE_TYPE]:
            node.children[0].set_name(node.children[0].children[0].get_name())
            step = "sort " + node.children[0].get_name()
            if SORT_KEY in node.children[0].attributes:
                step += " with attribute " + node.attributes[SORT_KEY] + " and "
            else:
                step += " and "
        step += "perform unique on table " + node.children[0].get_name()

    elif node.attributes[NODE_TYPE] == "Aggregate":
        for child in node.children:
            if "Sort" in child.attributes[NODE_TYPE]:
                child.set_name(child.children[0].get_name())
                step = "sort " + child.get_name() + " and "
            if "Scan" in child.attributes[NODE_TYPE]:
                if child.attributes[NODE_TYPE] == "Seq Scan":
                    step = "perform sequential scan on " + child.get_name() + " and "
                else:
                    step = "perform " + child.attributes[NODE_TYPE].lower() + " on " + child.get_name() + " and "
        step += "perform aggregate on table " + node.children[0].get_name()
        if len(node.children) == 2:
            step += " and table " + node.children[1].get_name()

    elif node.attributes[NODE_TYPE] == "Sort":
        step += "perform sort on table "
        step += node.children[0].get_name()
        node_sort_key_str = ''.join(node.attributes[SORT_KEY])
        if "DESC" in node_sort_key_str:
            step += " with attribute " + node_sort_key_str.replace('DESC', '') + "in a descending order"
        else:
            step += " with attribute " + node_sort_key_str

    elif node.attributes[NODE_TYPE] == "Limit":
        step += "limit the result from table " + node.children[0].get_name() + " to " + str(
            node.attributes[PLAN_ROWS]) + " record(s)"

    else:
        step += "perform " + node.attributes[NODE_TYPE].lower() + " on"
        if len(node.children) > 1:
            for i, child in enumerate(node.children):
                if i < len(node.children) - 1:
                    step += (" table " + child.get_name() + ",")
                else:
                    step += (" and table " + child.get_name())
        else:
            step += " table " + node.children[0].get_name()

    if INDEX_COND in node.attributes:
        if ":" not in node.attributes[INDEX_COND]:
            node.attributes[INDEX_COND] = node.attributes[INDEX_COND][:-1]
        if "AND" in node.attributes[INDEX_COND] or  "OR" in node.attributes[INDEX_COND]:
            node.attributes[INDEX_COND] = node.attributes[INDEX_COND][1:-1]
        step += " and filtering on " + node.attributes[INDEX_COND].rsplit("::", 1)[0] + ")"
        
    if GROUP_KEY in node.attributes:
        if len(node.attributes[GROUP_KEY]) == 1:
            step += " with grouping on attribute " + node.attributes[GROUP_KEY][0]
        else:
            step += " with grouping on attribute "
            for i in node.attributes[GROUP_KEY][:-1]:
                step += i.replace("::text", "") + ", "
            step += node.attributes[GROUP_KEY][-1].replace("::text", "")
            
    if FILTER in node.attributes:
        if ":" not in node.attributes[FILTER]:
            node.attributes[FILTER] = node.attributes[FILTER][:-1]
        if "AND" in node.attributes[FILTER] or  "OR" in node.attributes[FILTER]:
            node.attributes[FILTER] = node.attributes[FILTER][1:-1]
        step += " and filtering on " + node.attributes[FILTER].rsplit("::", 1)[0] + ")"

    if JOIN_FILTER in node.attributes:
        if ":" not in node.attributes[FILTER]:
            node.attributes[FILTER] = node.attributes[FILTER][:-1]
        if "AND" in node.attributes[FILTER] or  "OR" in node.attributes[FILTER]:
            node.attributes[FILTER] = node.attributes[FILTER][1:-1]
        step += " while filtering on " + node.attributes[FILTER].rsplit("::", 1)[0] + ")"   

    if increment:
        node.set_name("T" + str(current_table_name))
        step += " to get intermediate table " + node.get_name()
        current_table_name += 1
    if SUBPLAN_NAME in node.attributes:
        table_subquery_name_pair[node.attributes[SUBPLAN_NAME]] = node.get_name()
    step = "\n\nStep " + str(current_step) + ", " + step + "."
    node.set_step(current_step)
    current_step += 1
    steps.append(step)

# for ete3
def convert_to_ete3(node):
    if len(node.children) == 0:
        return node.attributes[NODE_TYPE]
    lstr = "("

    for n in node.children:
        lstr += convert_to_ete3(n) + ","

    lstr = lstr[:-1]
    lstr += ")%s" % node.attributes[NODE_TYPE]
    return lstr
