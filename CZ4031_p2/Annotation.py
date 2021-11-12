import queue
import json
import copy
import re

class Node(object):
    def __init__(self, node_type, relation_name, schema, alias, group_key, sort_key, join_type, index_name,
                 hash_cond, table_filter, index_cond, merge_cond, recheck_cond, join_filter, subplan_name,
                 actual_time):
        self.node_type = node_type
        self.children = []
        self.relation_name = relation_name
        self.schema = schema
        self.alias = alias
        self.group_key = group_key
        self.sort_key = sort_key
        self.join_type = join_type
        self.index_name = index_name
        self.hash_cond = hash_cond
        self.table_filter = table_filter
        self.index_cond = index_cond
        self.merge_cond = merge_cond
        self.recheck_cond = recheck_cond
        self.join_filter = join_filter
        self.subplan_name = subplan_name
        self.actual_time = actual_time

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

        relation_name = schema = alias = group_key = sort_key = join_type = index_name = hash_cond = table_filter \
            = index_cond = merge_cond = recheck_cond = join_filter = subplan_name = actual_time = None
        if 'Relation Name' in current_plan:
            relation_name = current_plan['Relation Name']
        if 'Schema' in current_plan:
            schema = current_plan['Schema']
        if 'Alias' in current_plan:
            alias = current_plan['Alias']
        if 'Group Key' in current_plan:
            group_key = current_plan['Group Key']
        if 'Sort Key' in current_plan:
            sort_key = current_plan['Sort Key']
        if 'Join Type' in current_plan:
            join_type = current_plan['Join Type']
        if 'Index Name' in current_plan:
            index_name = current_plan['Index Name']
        if 'Hash Cond' in current_plan:
            hash_cond = current_plan['Hash Cond']
        if 'Filter' in current_plan:
            table_filter = current_plan['Filter']
        if 'Index Cond' in current_plan:
            index_cond = current_plan['Index Cond']
        if 'Merge Cond' in current_plan:
            merge_cond = current_plan['Merge Cond']
        if 'Recheck Cond' in current_plan:
            recheck_cond = current_plan['Recheck Cond']
        if 'Join Filter' in current_plan:
            join_filter = current_plan['Join Filter']
        if 'Actual Total Time' in current_plan:
            actual_time = current_plan['Actual Total Time']
        if 'Subplan Name' in current_plan:
            if "returns" in current_plan['Subplan Name']:
                name = current_plan['Subplan Name']
                subplan_name = name[name.index("$"):-1]
            else:
                subplan_name = current_plan['Subplan Name']

        current_node = Node(current_plan['Node Type'], relation_name, schema, alias, group_key, sort_key, join_type,
                            index_name, hash_cond, table_filter, index_cond, merge_cond, recheck_cond, join_filter,
                            subplan_name, actual_time)
        print(current_node.node_type)

        if "Limit" == current_node.node_type:
            current_node.plan_rows = current_plan['Plan Rows']

        if "Scan" in current_node.node_type:
            if "Index" in current_node.node_type:
                if relation_name:
                    current_node.set_output_name(relation_name + " with index " + index_name)
            elif "Subquery" in current_node.node_type:
                current_node.set_output_name(alias)
            else:
                current_node.set_output_name(relation_name)

        if parent_node is not None:
            parent_node.add_children(current_node)
        else:
            head_node = current_node

        if 'Plans' in current_plan:
            for item in current_plan['Plans']:
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
    if node.node_type in ["Unique", "Aggregate"] and len(node.children) == 1 \
            and ("Scan" in node.children[0].node_type or node.children[0].node_type == "Sort"):
        children_skip = True
    elif node.node_type == "Bitmap Heap Scan" and node.children[0].node_type == "Bitmap Index Scan":
        children_skip = True
    else:
        children_skip = False

    # recursive
    for child in node.children:
        if node.node_type == "Aggregate" and len(node.children) > 1 and child.node_type == "Sort":
            to_text(child, True)
        else:
            to_text(child, children_skip)

    if node.node_type in ["Hash"] or skip:
        return

    step = ""

    # generate natural language for various QEP operators
    if "Join" in node.node_type:

        # special preprocessing for joins
        if node.join_type == "Semi":
            # add the word "Semi" before "Join" into node.node_type
            node_type_list = node.node_type.split()
            node_type_list.insert(-1, node.join_type)
            node.node_type = " ".join(node_type_list)
        else:
            pass

        if "Hash" in node.node_type:
            step += " and perform " + node.node_type.lower() + " on "
            for i, child in enumerate(node.children):
                if child.node_type == "Hash":
                    child.set_output_name(child.children[0].get_output_name())
                    hashed_table = child.get_output_name()
                if i < len(node.children) - 1:
                    step += ("table " + child.get_output_name())
                else:
                    step += (" and table " + child.get_output_name())
            # combine hash with hash join
            # step = "hash table " + hashed_table + step + " under condition " + "hash cond"
            step = "hash table " + hashed_table + step + " under condition " + node.hash_cond

        elif "Merge" in node.node_type:
            step += "perform " + node.node_type.lower() + " on "
            any_sort = False  # whether sort is performed on any table
            for i, child in enumerate(node.children):
                if child.node_type == "Sort":
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
                    if child.node_type == "Sort":
                        if i < len(node.children):
                            sort_step += ("table " + child.get_output_name())
                        else:
                            sort_step += (" and table " + child.get_output_name())
                step = sort_step + " and " + step

    # elif node.node_type == "Bitmap Heap Scan":
        # combine bitmap heap scan and bitmap index scan
        # if "Bitmap Index Scan" in node.children[0].node_type:
        #     node.children[0].set_output_name(node.relation_name)
        #     step = " with index condition " + node.index_cond.replace("::text", "")
        # step += "perform bitmap heap scan on table " + node.children[0].get_output_name() + step

    elif "Scan" in node.node_type:
        if node.node_type == "Seq Scan":
            step += "perform sequential scan on table "
            step += node.get_output_name()
        elif node.node_type == "Bitmap Heap Scan":
            if "Bitmap Index Scan" in node.children[0].node_type:
                node.children[0].set_output_name(node.relation_name)
                step += "performs bitmap heap scan on table " + node.children[0].get_output_name() +\
                        " with index condition " + node.children[0].index_cond
        else:
            step += "perform " + node.node_type.lower() + " on table "
            step += node.get_output_name()

        # if no table filter, remain original table name
        if not node.index_cond and not node.table_filter:
            increment = False

    elif node.node_type == "Unique":
        # combine unique and sort
        if "Sort" in node.children[0].node_type:
            node.children[0].set_output_name(node.children[0].children[0].get_output_name())
            step = "sort " + node.children[0].get_output_name()
            if node.children[0].sort_key:
                step += " with attribute " + node.sort_key + " and "
            else:
                step += " and "

        step += "perform unique on table " + node.children[0].get_output_name()

    elif node.node_type == "Aggregate":
        for child in node.children:
            # combine aggregate and sort
            if "Sort" in child.node_type:
                child.set_output_name(child.children[0].get_output_name())
                step = "sort " + child.get_output_name() + " and "
            # combine aggregate with scan
            if "Scan" in child.node_type:
                if child.node_type == "Seq Scan":
                    step = "perform sequential scan on " + child.get_output_name() + " and "
                else:
                    step = "perform " + child.node_type.lower() + " on " + child.get_output_name() + " and "

        step += "perform aggregate on table " + node.children[0].get_output_name()
        if len(node.children) == 2:
            step += " and table " + node.children[1].get_output_name()

    elif node.node_type == "Sort":
        step += "perform sort on table "
        step += node.children[0].get_output_name()
        node_sort_key_str = ''.join(node.sort_key)
        if "DESC" in node_sort_key_str:
##            if ":" not in node_sort_key_str:
##                if ")" in node_sort_key_str:
##                    node_sort_key_str = node_sort_key_str[:-1]
##            if "AND" in node_sort_key_str or  "OR" in node_sort_key_str:
##                node_sort_key_str = node_sort_key_str[1:-1]
##
##            node_sort_key_str = re.sub("\::[^)]*\)", '', node_sort_key_str, flags=re.DOTALL)
            step += " with attribute " + node_sort_key_str.replace('DESC', '') + "in a descending order"

        else:
            step += " with attribute " + node_sort_key_str
##            if ":" not in node_sort_key_str:
##                node_sort_key_str = node_sort_key_str[:-1]
##            if "AND" in node_sort_key_str or  "OR" in node_sort_key_str:
##                node_sort_key_str = node_sort_key_str[1:-1]
##            
##            step += " with attribute " + re.sub("\::[^)]*\)", '', node_sort_key_str, flags=re.DOTALL) + ")"

    elif node.node_type == "Limit":
        step += "limit the result from table " + node.children[0].get_output_name() + " to " + str(
            node.plan_rows) + " record(s)"

    else:
        step += "perform " + node.node_type.lower() + " on"
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
    if node.index_cond:
        if ":" not in node.index_cond:
            node.index_cond = node.index_cond[:-1]
        if "AND" in node.index_cond or  "OR" in node.index_cond:
            node.index_cond = node.index_cond[1:-1]

        step += " and filtering on " + node.index_cond.rsplit("::", 1)[0] + ")"
        #step += " and filtering on " + re.sub("\::[^)]*\)", '', node.index_cond, flags=re.DOTALL) + ")"
        
    if node.group_key:
        if len(node.group_key) == 1:
            step += " with grouping on attribute " + node.group_key[0]
        else:
            step += " with grouping on attribute "
            for i in node.group_key[:-1]:
                step += i.replace("::text", "") + ", "
            step += node.group_key[-1].replace("::text", "")
            
    if node.table_filter:
        if ":" not in node.table_filter:
            node.table_filter = node.table_filter[:-1]
        if "AND" in node.table_filter or  "OR" in node.table_filter:
            node.table_filter = node.table_filter[1:-1]


        step += " and filtering on " + node.table_filter.rsplit("::", 1)[0] + ")"
        #step += " and filtering on " + re.sub("\::[^)]*\)", '', node.table_filter, flags=re.DOTALL) + ")"
    if node.join_filter:
        if ":" not in node.table_filter:
            node.table_filter = node.table_filter[:-1]
        if "AND" in node.table_filter or  "OR" in node.table_filter:
            node.table_filter = node.table_filter[1:-1]

        step += " while filtering on " + node.table_filter.rsplit("::", 1)[0] + ")"   
        #step += " while filtering on " + re.sub("\::[^)]*\)", '', node.table_filter, flags=re.DOTALL) + ")"

        # set intermediate table name
    if increment:
        node.set_output_name("T" + str(cur_table_name))
        step += " to get intermediate table " + node.get_output_name()
        cur_table_name += 1
    if node.subplan_name:
        table_subquery_name_pair[node.subplan_name] = node.get_output_name()
        

    step = "\n\nStep " + str(cur_step) + ", " + step + "."
    node.set_step(cur_step)
    cur_step += 1

    steps.append(step)



def generate_tree(tree, node, _prefix="", _last=True):
    if _last:
        #tree = "{}`-- {}\n".format(_prefix, node.node_type)
        tree = "{}|--- {}\n".format(_prefix, node.node_type)
    else:
        #tree = "{}|-- {}\n".format(_prefix, node.node_type)
        tree = "{}|--- {}\n".format(_prefix, node.node_type)

    _prefix += "|   " if _last else "|  "
    child_count = len(node.children)
    for i, child in enumerate(node.children):
        _last = i == (child_count - 1)
        tree = tree + generate_tree(tree, child, _prefix, _last)
    return tree


def convert_tree_string(node):
    if len(node.children) == 0:
        print(node.node_type)
        return node.node_type

    lstr = "("

    for n in node.children:
        print(node.node_type)
        lstr += convert_tree_string(n) + ","

    lstr = lstr[:-1]

    lstr += ")%s" % node.node_type
    print(lstr)
    return lstr




# def convert_tree_string(node):
#     if len(node.children) == 0:
#         print(node.node_type)
#         return node.node_type
#
#     lstr = "("
#
#     for n in node.children:
#         print(node.node_type)
#         lstr += convert_tree_string(n) + ","
#
#     lstr = lstr[:-1]
#
#     lstr += ")%s" % node.node_type
#     print(lstr)
#     return lstr

# def convert_tree_graphviz(node):
#     if len(node.children) == 0:
#         print(node.node_type)
#         return node.node_type
#
#     lstr = ""
#     list = []
#     new_str =""
#
#     for n in node.children:
#         #print(node.node_type)
#         #print("node children: "+ n.node_type)
#         lstr = convert_tree_graphviz(n) + " -> " + lstr
#         # remove the last repeated " -> "
#         lstr = lstr[:-4]
#         lstr = ("%s" % node.node_type) + " -> " + lstr
#         list.append(lstr + ";")
#         print("ended")
#
#     # # remove the last repeated " -> "
#     # lstr = lstr[:-4]
#     #
#     # lstr = ("%s" % node.node_type) + " -> " + lstr
#
#     for i in list:
#         #print("list: " + i)
#         new_str = new_str + "," + i
#
#     #print(lstr)
#     new_str = new_str[1:] # remove first ,
#     #print( "new str: "+new_str)
#     return new_str

