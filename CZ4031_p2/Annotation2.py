import queue
import json
import copy
import re

# Node Attribute Keys
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

class Node(object):
    def __init__(self, attributes):
        self.children = []
        self.attributes = attributes.copy()

    def add_children(self, child):
        self.children.append(child)

    def set_output_name(self, output_name):
        if "T" == output_name[0] and output_name[1:].isdigit():
            self.output_name = int(output_name[1:])
        else:
            self.output_name = output_name

    def get_output_name(self):
        if str(self.output_name).isdigit():
            return "T" + str(self.output_name)
        else:
            return self.output_name

    def set_step(self, step):
        self.step = step

def parse_json(json_obj):
    q = queue.Queue()
    q_node = queue.Queue()
    plan = json_obj[0]['Plan']
    q.put(plan)
    q_node.put(None)

    while not q.empty():
        current_plan = q.get()
        parent_node = q_node.get()

        current_node = Node(current_plan)
        if SUBPLAN_NAME in current_node.attributes:
            if "returns" in current_node.attributes[SUBPLAN_NAME]:
                n = current_node.attributes[SUBPLAN_NAME]
                current_node.attributes[SUBPLAN_NAME] = n[n.index("$"):-1]

        if "Scan" in current_node.attributes[NODE_TYPE]:
            if "Index" in current_node.attributes[NODE_TYPE]:
                if current_node.attributes[RELATION_NAME]:
                    current_node.set_output_name(current_node.attributes[RELATION_NAME] + " with index " + current_node.attributes[INDEX_NAME])
            elif "Subquery" in current_node.attributes[NODE_TYPE]:
                current_node.set_output_name(current_node.attributes[ALIAS])
            else:
                current_node.set_output_name(current_node.attributes[RELATION_NAME])

        if parent_node is not None:
            parent_node.add_children(current_node)
        else:
            head_node = current_node

        if PLANS in current_plan:
            for item in current_plan[PLANS]:
                # push child plans into queue
                q.put(item)
                # push parent for each child into queue
                q_node.put(current_node)

    return head_node

def textVersion(node):
    global steps, cur_step, cur_table_name, table_subquery_name_pair
    global current_plan_tree
    steps = ["The query is executed as follow."]
    cur_step = 1
    cur_table_name = 1
    table_subquery_name_pair = {}

    to_text(node)
    if " to get intermediate table" in steps[-1]:
        steps[-1] = steps[-1][:steps[-1].index(" to get intermediate table")] + ' to get the final result.'

    steps_str = ''.join(steps)
    return steps_str

def to_text(node, skip=False):
    global steps, cur_step, cur_table_name
    increment = True
    # skip the child if merge it with current node
    if node.attributes[NODE_TYPE] in ["Unique", "Aggregate"] and len(node.children) == 1 \
            and ("Scan" in node.children[0].attributes[NODE_TYPE] or node.children[0].attributes[NODE_TYPE] == "Sort"):
        children_skip = True
    elif node.attributes[NODE_TYPE] == "Bitmap Heap Scan" and node.children[0].attributes[NODE_TYPE] == "Bitmap Index Scan":
        children_skip = True
    else:
        children_skip = False

    # recursive
    for child in node.children:
        if node.attributes[NODE_TYPE] == "Aggregate" and len(node.children) > 1 and child.attributes[NODE_TYPE] == "Sort":
            to_text(child, True)
        else:
            to_text(child, children_skip)

    if node.attributes[NODE_TYPE] == "Hash" or skip:
        return

    step = ""

    # generate natural language for various QEP operators
    if "Join" in node.attributes[NODE_TYPE]:

        # special preprocessing for joins
        if node.attributes[JOIN_TYPE] == "Semi":
            # add the word "Semi" before "Join" into node.node_type
            node_type_list = node.attributes[NODE_TYPE].split()
            node_type_list.insert(-1, node.attributes[JOIN_TYPE])
            node.attributes[NODE_TYPE] = " ".join(node_type_list)
        else:
            pass

        if "Hash" in node.attributes[NODE_TYPE]:
            step += " and perform " + node.attributes[NODE_TYPE].lower() + " on "
            for i, child in enumerate(node.children):
                if child.attributes[NODE_TYPE] == "Hash":
                    child.set_output_name(child.children[0].get_output_name())
                    hashed_table = child.get_output_name()
                if i < len(node.children) - 1:
                    step += ("table " + child.get_output_name())
                else:
                    step += (" and table " + child.get_output_name())
            # combine hash with hash join
            step = "hash table " + hashed_table + step + " under condition " + node.attributes[HASH_COND]

        elif "Merge" in node.attributes[NODE_TYPE]:
            step += "perform " + node.attributes[NODE_TYPE].lower() + " on "
            any_sort = False  # whether sort is performed on any table
            for i, child in enumerate(node.children):
                if child.attributes[NODE_TYPE] == "Sort":
                    child.set_output_name(child.children[0].get_output_name())
                    any_sort = True
                if i < len(node.children) - 1:
                    step += ("table " + child.get_output_name())
                else:
                    step += (" and table " + child.get_output_name())
            # combine sort with merge join
            if any_sort:
                sort_step = "sort "
                for child in node.children:
                    if child.attributes[NODE_TYPE] == "Sort":
                        if i < len(node.children):
                            sort_step += ("table " + child.get_output_name())
                        else:
                            sort_step += (" and table " + child.get_output_name())
                step = sort_step + " and " + step

    elif "Scan" in node.attributes[NODE_TYPE]:
        if node.attributes[NODE_TYPE] == "Seq Scan":
            step += "perform sequential scan on table "
            step += node.get_output_name()
        elif node.attributes[NODE_TYPE] == "Bitmap Heap Scan":
            if "Bitmap Index Scan" in node.children[0].attributes[NODE_TYPE]:
                node.children[0].set_output_name(node.attributes[RELATION_NAME])
                step += "performs bitmap heap scan on table " + node.children[0].get_output_name() +\
                        " with index condition " + node.children[0].attributes[INDEX_COND]
        else:
            step += "perform " + node.attributes[NODE_TYPE].lower() + " on table "
            step += node.get_output_name()

        # if no table filter, remain original table name
        if INDEX_COND not in node.attributes and FILTER not in node.attributes:
            increment = False

    elif node.attributes[NODE_TYPE] == "Unique":
        # combine unique and sort
        if "Sort" in node.children[0].attributes[NODE_TYPE]:
            node.children[0].set_output_name(node.children[0].children[0].get_output_name())
            step = "sort " + node.children[0].get_output_name()
            if SORT_KEY in node.children[0].attributes:
                step += " with attribute " + node.attributes[SORT_KEY] + " and "
            else:
                step += " and "

        step += "perform unique on table " + node.children[0].get_output_name()

    elif node.attributes[NODE_TYPE] == "Aggregate":
        for child in node.children:
            # combine aggregate and sort
            if "Sort" in child.attributes[NODE_TYPE]:
                child.set_output_name(child.children[0].get_output_name())
                step = "sort " + child.get_output_name() + " and "
            # combine aggregate with scan
            if "Scan" in child.attributes[NODE_TYPE]:
                if child.attributes[NODE_TYPE] == "Seq Scan":
                    step = "perform sequential scan on " + child.get_output_name() + " and "
                else:
                    step = "perform " + child.attributes[NODE_TYPE].lower() + " on " + child.get_output_name() + " and "

        step += "perform aggregate on table " + node.children[0].get_output_name()
        if len(node.children) == 2:
            step += " and table " + node.children[1].get_output_name()

    elif node.attributes[NODE_TYPE] == "Sort":
        step += "perform sort on table "
        step += node.children[0].get_output_name()
        node_sort_key_str = ''.join(node.attributes[SORT_KEY])
        if "DESC" in node_sort_key_str:
            step += " with attribute " + node_sort_key_str.replace('DESC', '') + "in a descending order"

        else:
            step += " with attribute " + node_sort_key_str

    elif node.attributes[NODE_TYPE] == "Limit":
        step += "limit the result from table " + node.children[0].get_output_name() + " to " + str(
            node.attributes[PLAN_ROWS]) + " record(s)"

    else:
        step += "perform " + node.attributes[NODE_TYPE].lower() + " on"
        # binary operator
        if len(node.children) > 1:
            for i, child in enumerate(node.children):
                if i < len(node.children) - 1:
                    step += (" table " + child.get_output_name() + ",")
                else:
                    step += (" and table " + child.get_output_name())
        # unary operator
        else:
            step += " table " + node.children[0].get_output_name()

    # add conditions
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

        # set intermediate table name
    if increment:
        node.set_output_name("T" + str(cur_table_name))
        step += " to get intermediate table " + node.get_output_name()
        cur_table_name += 1
    if SUBPLAN_NAME in node.attributes:
        table_subquery_name_pair[node.attributes[SUBPLAN_NAME]] = node.get_output_name()
        

    step = "\n\nStep " + str(cur_step) + ", " + step + "."
    node.set_step(cur_step)
    cur_step += 1

    steps.append(step)



def generate_tree(tree, node, _prefix="", _last=True):
    if _last:
        tree = "{}|--- {}\n".format(_prefix, node.attributes[NODE_TYPE])
    else:
        tree = "{}|--- {}\n".format(_prefix, node.attributes[NODE_TYPE])

    _prefix += "|   " if _last else "|  "
    child_count = len(node.children)
    for i, child in enumerate(node.children):
        _last = i == (child_count - 1)
        tree = tree + generate_tree(tree, child, _prefix, _last)
    return tree

def convert_tree_string(node):
    if len(node.children) == 0:
        #print(node.attributes[NODE_TYPE])
        return node.attributes[NODE_TYPE]

    lstr = "("

    for n in node.children:
        #print(node.attributes[NODE_TYPE])
        lstr += convert_tree_string(n) + ","

    lstr = lstr[:-1]

    lstr += ")%s" % node.attributes[NODE_TYPE]
    #print(lstr)
    return lstr
