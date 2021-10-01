#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <math.h>
#include <tuple>

using namespace std;

#define DISK_SIZE           100000000
#define BLOCK_SIZE          100
#define BLOCKS_IN_DISK      (DISK_SIZE/BLOCK_SIZE)
#define RECORD_SIZE         sizeof(Record)
#define RECORDS_PER_BLOCK   ((BLOCK_SIZE-2*sizeof(int))/RECORD_SIZE)
#define POINTER_SIZE        sizeof(uintptr_t)//4
#define DATA_FILE           "dataTest.tsv"
// TODO: change back N
const static int N = 2;// floor((BLOCK_SIZE - POINTER_SIZE) / (POINTER_SIZE + sizeof(int)));
#define RECORDS_PER_BUCKET  ((BLOCK_SIZE - (2*sizeof(int) + sizeof(bool)))/sizeof(uintptr_t) - 1)

struct Record
{
    int id;
    float avg_rating;
    int num_of_votes;

    Record()
    {
        id = -1;
        avg_rating = -1.0f;
        num_of_votes = -1;
    }
    Record(int id, float avg_rating, int num_of_votes)
    {
        this->id = id;
        this->avg_rating = avg_rating;
        this->num_of_votes = num_of_votes;
    }

    int getRecordSize()
    {
        return sizeof(id) + sizeof(avg_rating) + sizeof(num_of_votes);
    }

    string toString()
    {
        stringstream ss;
        ss << fixed << setprecision(1) << "Record " << id << " :\t" << avg_rating << "\t| " << num_of_votes;
        return ss.str();
    }

};

struct Disk_Block
{
    // header includes id
    int id = -1;
    int size = 0;

    Record records[RECORDS_PER_BLOCK];
};

class Node
{
public:
    uintptr_t* ptr;
    int* key, size;
    bool isLeaf;

    Node()
    {
        ptr = new uintptr_t[N + 1]{NULL};
        key = new int[N] {NULL};
        isLeaf = false;
        size = 0;
    }

    ~Node()
    {
        // TODO: fix delete
        //delete[] ptr;
        //delete[] key;
    }

    void print()
    {
        for (int i = 0; i < size; i++)
        {
            cout << key[i] << "\t";
        }
        cout << endl;
    }
};

class Bucket
{
public:
    uintptr_t* ptr;
    int key;
    int size;
    bool overflowed;

    Bucket()
    {
        ptr = new uintptr_t[RECORDS_PER_BUCKET+1] {NULL};
        key = -1;
        size = 0;
        overflowed = false;
    }

    Bucket(int key)
    {
        ptr = new uintptr_t[RECORDS_PER_BUCKET+1]{ NULL };
        this->key = key;
        size = 0;
        overflowed = false;
    }

    ~Bucket()
    {
        // TODO: fix delete
        //delete[] ptr;
    }
};

class BPlusTree
{
public:

    Node* root;

    // minimum keys
    // floor((N + 1) / 2)
    int min_key_in_leaf = floor((N + 1) / 2);
    // floor(N / 2)
    int min_key_in_nonleaf = floor(N / 2);
    // height of tree (inclusive of leaf level)
    int height = 0;
    // number of nodes in the tree
    int num_of_nodes = 0;
    // number of buckets in the tree
    int num_of_buckets = 0;

    BPlusTree()
    {
        root = NULL;
    }

    ~BPlusTree()
    {
        // TODO: this might be improper deletion
        // did not run through the whole tree and delete child nodes
        //delete root;
    }

    // insert record into existing bucket with pos in leaf node
    void insertIntoBucket(Node* n, int pos, Record* r)
    {
        Bucket* b = reinterpret_cast<Bucket*>(n->ptr[pos]);
        while (b->overflowed)
        {
            b = reinterpret_cast<Bucket*>(b->ptr[RECORDS_PER_BUCKET]);
        }

        // check if bucket is not full
        if (b->size < RECORDS_PER_BUCKET)
        {
            b->ptr[b->size] = (uintptr_t)r;
            b->size++;
        }
        else
        {
            // create new overflow bucket
            Bucket* nb = new Bucket(r->num_of_votes);
            num_of_buckets++;
            nb->ptr[0] = (uintptr_t)r;
            nb->size++;

            b->ptr[RECORDS_PER_BUCKET] = (uintptr_t)nb;
            b->overflowed = true;
        }

    }

    // insert record into leaf node that has space and returns input position
    int insertIntoLeaf(Node* n, Record* r)
    {
        bool is_set = false;

        for (int i = 0; i < n->size; i++)
        {
            if (r->num_of_votes == n->key[i])
            {
                insertIntoBucket(n, i, r);
                return i;
            }
            else if (r->num_of_votes < n->key[i])
            {
                // shifts all keys after i to the right
                for(int j = N - 1; j > i; j--)
                {
                    n->ptr[j] = n->ptr[j - 1];
                    n->key[j] = n->key[j - 1];
                }

                // create a new bucket to store record
                Bucket* nb = new Bucket(r->num_of_votes);
                num_of_buckets++;
                nb->ptr[0] = (uintptr_t)r;
                nb->size++;

                // sets i to record
                n->ptr[i] = (uintptr_t)nb;
                n->key[i] = r->num_of_votes;
                n->size++;
                is_set = true;
                return i;
            }
        }

        // if record is bigger than the current last key
        if (!is_set)
        {

            // create a new bucket to store record
            Bucket* nb = new Bucket(r->num_of_votes);
            num_of_buckets++;
            nb->ptr[0] = (uintptr_t)r;
            nb->size++;

            n->ptr[n->size] = (uintptr_t)nb;
            n->key[n->size] = r->num_of_votes;
            n->size++;
        }
        return n->size - 1;
    }

    // searches through node recursively to get leaf node of record
    // returns the parent node
    Node* searchForLeafNodeWithKey(Node** n, int key)
    {
        Node* p = NULL;

        // advances down all levels of tree
        // TODO: try find alternative to goto
        loop:
        while (!(*n)->isLeaf)
        {
            p = (*n);
            for (int i = 0; i < (*n)->size; i++)
            {
                // record under the left pointer of key
                if (key < (*n)->key[i])
                {
                    (*n) = reinterpret_cast<Node*>((*n)->ptr[i]);
                    goto loop;
                    //break;
                }
            }

            if ((*n)->isLeaf)
                break;
            else
            {
                // record is larger than the last key
                (*n) = reinterpret_cast<Node*>((*n)->ptr[(*n)->size]);
            }
        }
        return p;
    }

	Node* searchAndPrintExperimentThree(Node** n, int key)
    {
        Node* p = NULL;
        Node* nodeArray[5];
        int nodeCounter = 0;

        // advances down all levels of tree
        // TODO: try find alternative to goto
        loop:
        while (!(*n)->isLeaf)
        {
            p = (*n);
            if(nodeCounter < 5)
            {
                nodeArray[nodeCounter] = p;
                nodeCounter++;
            }
            for (int i = 0; i < (*n)->size; i++)
            {
                // record under the left pointer of key
                if (key < (*n)->key[i])
                {
                    (*n) = reinterpret_cast<Node*>((*n)->ptr[i]);
					goto loop;
                    //break;
                }
            }

            if ((*n)->isLeaf)
			{
                break;
            }
            else
            {
                // record is larger than the last key
                (*n) = reinterpret_cast<Node*>((*n)->ptr[(*n)->size]);
            }
        }

        if(nodeCounter < 5)
        {
            nodeArray[nodeCounter] = (*n);
            nodeCounter++;
        }

        for(int i = 0; i < nodeCounter; i++)
        {
            cout << "\n- Node " << i+1 << " -" << endl;
            for(int j = 0; j < nodeArray[i]->size; j++)
            {
                cout << "Index Keys " << j+1 << ": " << nodeArray[i]->key[j] << endl;
				}
        }
        return p;
    }

	// searches through node recursively to get left leaf sibling of record
    // returns the parent node
    Node* searchForLeftLeafSiblingOfKey(Node** n, int key)
    {
        Node* p = NULL;

        // advances down all levels of tree
        // TODO: try find alternative to goto
		loop:
        while (!(*n)->isLeaf)
        {
            p = (*n);
            for (int i = (*n)->size - 1; i >= 0; i--)
            {
                if (key > (*n)->key[i])
                {
                    (*n) = reinterpret_cast<Node*>((*n)->ptr[i+1]);
					goto loop;
                    //break;
                }
            }

            if ((*n)->isLeaf)
				break;
            else
            {
                // record is smaller than the first key
                (*n) = reinterpret_cast<Node*>((*n)->ptr[0]);
				}
        }
        return p;
    }

    // insert a child node into a parent node
    void insertChildNode(Node* p, Node* c, int key)
    {
        for (int i = 0; i < p->size; i++)
        {
            if (key < p->key[i])
            {
                // shifts all keys after i to the right
                for (int j = p->size; j > i; j--)
                {
                    p->ptr[j + 1] = p->ptr[j];
                    p->key[j] = p->key[j - 1];
                }
                // sets i to child
                p->ptr[i + 1] = (uintptr_t)c;
                p->key[i] = key;
                p->size++;

                return;
            }
        }

        // if child key is bigger than the last parent key
        p->ptr[p->size + 1] = (uintptr_t)c;
        p->key[p->size] = key;
        p->size++;
    }

    // splits the full leaf node into two and create a slot for insertion
    // returns the node to insert into and the input position
    tuple<Node*, int> splitFullLeafNodeForInsert(Node* curr, Node* n, int key)
    {
        int input_pos = -1;
        // get input position of key
        for (int i = 0; i < N; i++)
        {
            if (curr->key[i] <= key && key < curr->key[i + 1])
            {
                input_pos = i + 1;
            }
        }

        // if input key is smallest key
        if (key < curr->key[0])
            input_pos = 0;

        // if input key is largest key
        if (input_pos == -1)
        {
            // shift all keys after minimum num from current to new node
            for (int i = min_key_in_leaf; i < N; i++)
            {
                n->ptr[i - min_key_in_leaf] = curr->ptr[i];
                n->key[i - min_key_in_leaf] = curr->key[i];
                n->size++;
                curr->ptr[i] = NULL;
                curr->key[i] = NULL;
                curr->size--;
            }

            // set record into last key in new node
            return make_tuple(n, n->size);
        }
        // if input key is placed in between current keys and shifted
        else
        {
            // if need shift current node's input position to new node
            if (input_pos >= min_key_in_leaf)
            {
                // shift all keys after minimum num from current to new node
                for (int i = min_key_in_leaf; i < N; i++)
                {
                    // offset for keys after input position
                    int o = 0;
                    o = (i >= input_pos) ? 1 : 0;

                    n->ptr[i - min_key_in_leaf + o] = curr->ptr[i];
                    n->key[i - min_key_in_leaf + o] = curr->key[i];
                    n->size++;
                    curr->ptr[i] = NULL;
                    curr->key[i] = NULL;
                    curr->size--;
                }

                // set record into input position of new node
                return make_tuple(n, input_pos + n->size - N);
            }
            // if input position is to stay in current node
            else
            {
                for (int i = N - 1; i >= input_pos; i--)
                {
                    // if keys need to be shifted to new node
                    // how I got (i - min_key + 1):
                    // n pos starts from the rightmost shift key
                    // n->ptr[i + shift_amount(N - min_key + 1) - N]
                    // where i is iterator moving left, shift_amount is the amount to shift right and N being the size of the node
                    if (i - min_key_in_leaf + 1 >= 0)
                    {
                        n->ptr[i - min_key_in_leaf + 1] = curr->ptr[i];
                        n->key[i - min_key_in_leaf + 1] = curr->key[i];
                        n->size++;
                        curr->ptr[i] = NULL;
                        curr->key[i] = NULL;
                        curr->size--;
                    }
                    // shift current node keys for record's new key position
                    else
                    {
                        curr->ptr[i + 1] = curr->ptr[i];
                        curr->key[i + 1] = curr->key[i];
                    }
                }

                // set record into input position of current node
                return make_tuple(curr, input_pos);
            }
        }
    }

    // splits the full nonleaf node into two and insert node c
    void insertIntoFullNonleafNode(Node** curr, Node** n, int key, Node* c)
    {
        int input_pos = -1;
        // get input position of key
        for (int i = 0; i < N; i++)
        {
            if ((*curr)->key[i] <= key && key < (*curr)->key[i + 1])
            {
                input_pos = i + 1;
            }
        }

        // if input key is smallest key
        if (key < (*curr)->key[0])
            input_pos = 0;

        // if input key is largest key
        if (input_pos == -1)
        {
            // shift all keys after minimum num from current to new node
            for (int i = N - min_key_in_nonleaf; i < N; i++)
            {
                (*n)->ptr[i - (N - min_key_in_nonleaf) + 1] = (*curr)->ptr[i + 1];
                (*n)->key[i - (N - min_key_in_nonleaf)] = (*curr)->key[i];
                (*n)->size++;
                (*curr)->ptr[i + 1] = NULL;
                (*curr)->key[i] = NULL;
                (*curr)->size--;
            }

            // insert child node c into n
            (*n)->key[(*n)->size] = key;
            (*n)->ptr[(*n)->size + 1] = (uintptr_t)c;
            (*n)->size++;

        }
        // if input key is placed in between current keys and shifted
        else
        {
            // if need shift current node's input position to new node
            if (input_pos >= N - min_key_in_nonleaf)
            {
                // shift all keys after minimum num from current to new node
                for (int i = N - min_key_in_nonleaf; i < N; i++)
                {
                    // offset for keys after input position
                    int o = 0;
                    // compare with insertion if is greater than or lesser than
                    o = (i >= input_pos) ? 1 : 0;

                    (*n)->ptr[i - (N - min_key_in_nonleaf) + o + 1] = (*curr)->ptr[i + 1];
                    (*n)->key[i - (N - min_key_in_nonleaf) + o] = (*curr)->key[i];
                    (*n)->size++;
                    (*curr)->ptr[i + 1] = NULL;
                    (*curr)->key[i] = NULL;
                    (*curr)->size--;
                }

                // insert child node c into n
                (*n)->key[input_pos + (*n)->size - N] = key;
                (*n)->ptr[input_pos + (*n)->size - N + 1] = (uintptr_t)c;
                (*n)->size++;

            }
            // if input position is to stay in current node
            else
            {
                for (int i = N - 1; i >= input_pos; i--)
                {
                    // if keys need to be shifted to new node
                    // how I got (i - (N - min_key_in_nonleaf) + 1):
                    // n pos starts from the rightmost shift key
                    // n->ptr[i + shift_amount(N - (N - min_key_in_nonleaf) + 1) - N]
                    // where i is iterator moving left, shift_amount is the amount to shift right and N being the size of the node
                    if (i - (N - min_key_in_nonleaf) + 1 >= 0)
                    {
                        (*n)->ptr[i - (N - min_key_in_nonleaf) + 2] = (*curr)->ptr[i + 1];
                        (*n)->key[i - (N - min_key_in_nonleaf) + 1] = (*curr)->key[i];
                        (*n)->size++;
                        (*curr)->ptr[i + 1] = NULL;
                        (*curr)->key[i] = NULL;
                        (*curr)->size--;
                    }
                    // shift current node keys for record's new key position
                    else
                    {
                        (*curr)->ptr[i + 2] = (*curr)->ptr[i + 1];
                        (*curr)->key[i + 1] = (*curr)->key[i];
                    }
                }

                // insert child node c into n
                (*curr)->key[input_pos] = key;
                (*curr)->ptr[input_pos + 1] = (uintptr_t)c;
                (*curr)->size++;

            }

        }

    }

    // updates all parent nodes recursively on insertion
    void insertParentUpdate(Node* p, Node* c, int key)
    {
        // if parent node still have space
        if (p->size < N)
        {
            // shift keys in parent node to insert key
            insertChildNode(p, c, key);
        }
        else
        {
            // split parent nodes and shift keys
            // recursively loops until new parent node is root

            // new node for split
            Node* n = new Node();
            num_of_nodes++;

            // splitting
            insertIntoFullNonleafNode(&p, &n, key, c);

            // if parent is the root, make new parent and set new root
            if (p == root)
            {
                // move left most key of new node to parent
                Node* np = new Node();
                num_of_nodes++;
                np->key[0] = n->key[0];
                np->ptr[0] = (uintptr_t)p;
                np->ptr[1] = (uintptr_t)n;
                np->size++;
                root = np;
                height++;

                // shift new node's keys to the left by 1
                for (int i = 0; i < n->size; i++)
                {
                    n->key[i] = n->key[i + 1];
                    n->ptr[i] = n->ptr[i + 1];
                    n->key[i + 1] = NULL;
                    n->ptr[i + 1] = NULL;
                }
                n->ptr[n->size] = n->ptr[n->size + 1];
                n->ptr[n->size + 1] = NULL;
                n->size--;
            }
            else
            {
                // parent of p and n
                key = n->key[0];
                // shift new node's keys to the left by 1
                for (int i = 0; i < n->size; i++)
                {
                    n->key[i] = n->key[i + 1];
                    n->ptr[i] = n->ptr[i + 1];
                    n->key[i + 1] = NULL;
                    n->ptr[i + 1] = NULL;
                }
                n->ptr[n->size] = n->ptr[n->size + 1];
                n->ptr[n->size + 1] = NULL;
                n->size--;

                // recursively loops for all parents
                insertParentUpdate(getParentNode(root, p), n, key);
            }
        }
    }

    // adds a record into the B+ tree
    // configured to use "num_of_votes" as key
    void addRecord(Record *r)
    {
        // if there is no root
        // i.e. tree is not built yet
        if (!root)
        {
            Node* n = new Node();
            num_of_nodes++;
            root = n;
            height++;
            n->isLeaf = true;

            // create a new bucket to store record
            Bucket* nb = new Bucket(r->num_of_votes);
            num_of_buckets++;
            nb->ptr[0] = (uintptr_t)r;
            nb->size++;

            n->ptr[0] = (uintptr_t )nb;
            n->key[0] = r->num_of_votes;
            n->size++;
        }
        else
        {
            // initialise current node pointer to root of tree
            Node* curr = root;

            // searches through tree till it hit the leaf node w the key
            Node* p = searchForLeafNodeWithKey(&curr, r->num_of_votes);

            int key_pos = -1;
            for (int i = 0; i < curr->size; i++)
            {
                if (curr->key[i] == r->num_of_votes)
                {
                    key_pos = i;
                    break;
                }
            }

            // if key already exists in tree
            if (key_pos != -1)
                insertIntoBucket(curr, key_pos, r);
            // if leaf node have space
            else if (curr->size < N)
                // inserting a record into a leaf node with space
                insertIntoLeaf(curr, r);
            else
            {
                // creating a new leaf node and splitting records equally
                Node* n = new Node();
                num_of_nodes++;
                curr->ptr[N] = (uintptr_t)n;
                n->isLeaf = true;

                Node* t; int input_pos;
                // split full node equally between curr and n nodes
                tie(t, input_pos) = splitFullLeafNodeForInsert(curr, n, r->num_of_votes);

                // create a new bucket to store record
                Bucket* nb = new Bucket(r->num_of_votes);
                num_of_buckets++;
                nb->ptr[0] = (uintptr_t)r;
                nb->size++;

                // sets record into input position
                t->ptr[input_pos] = (uintptr_t)nb;
                t->key[input_pos] = r->num_of_votes;
                t->size++;

                // if current leaf node have no parent, create parent and set as root
                if (curr == root)
                {
                    p = new Node();
                    num_of_nodes++;
                    p->ptr[0] = (uintptr_t)curr;
                    p->key[0] = n->key[0];
                    p->ptr[1] = (uintptr_t)n;
                    p->size++;
                    root = p;
                    height++;
                }
                else
                {
                    if (!p)
                    {
                        cout << "ERROR : B+ Tree linkage problem! Child Node with first Key " << n->key[0] << " will not be inserted into the tree." << endl;
                        return;
                    }

                    insertParentUpdate(p, n, n->key[0]);

                }
            }

        }
    }
	
    // get the bucket with given key
    Bucket* getBucket(int key, bool print)
    {
        if (!root)
            return NULL;

        Node* curr = root;

        if(!print)
            searchForLeafNodeWithKey(&curr, key);
        else
            searchAndPrintExperimentThree(&curr, key);
        /*if (!curr)
            return NULL;*/

        for (int i = 0; i < curr->size; i++)
        {
            // returns record if key match
            if (key == curr->key[i])
                return reinterpret_cast<Bucket*>(curr->ptr[i]);
        }

        return NULL;
    }

    // updates all parent nodes recursively on key change
    void changeKeyParentUpdate(Node* p, int key, int new_key)
    {
        // get key pos
        int key_pos = getKeyPositionInNode(p, key);

        // if key_pos is -1 (no key, ptr is first ptr[0])
        // need to loop on parent until key_pos >= 0
        // if found parent with key_pos >= 0, update key
        // if hit root on loop, update root key
        if (key_pos != -1)
        {
            p->key[key_pos] = new_key;
        }
        else
        {
            if(p != root)
                changeKeyParentUpdate(getParentNode(root, p), key, new_key);
        }

    }

    // returns the key position in a node
    // if key not found, returns -1
    int getKeyPositionInNode(Node* n, int key)
    {
        for (int i = 0; i < n->size; i++)
        {
            if (n->key[i] == key)
                return i;
        }
        return -1;
    }

    // returns the sibling nodes of child node
    tuple<Node*, Node*> getLeafSiblings(Node* p, Node* c)
    {
        // sibling nodes
        Node* sl = NULL; Node* sr = NULL;
        // get right sibling
        if (c->ptr[N])
            sr = reinterpret_cast<Node*>(c->ptr[N]);
        
        // get left sibling
        int key_pos = getKeyPositionInNode(p, c->key[0]);
        // if left sibling is under parent
        if (key_pos != -1)
        {
            sl = reinterpret_cast<Node*>(p->ptr[key_pos]);
            return make_tuple(sl, sr);
        }
        else
        {
            // find left sibling
            sl = root;
            searchForLeftLeafSiblingOfKey(&sl, c->key[0]);

            // if child node is first leaf node from the left
            if (sl == c)
                sl = NULL;

            return make_tuple(sl, sr);
        }
    }

    // deletes all records with given key
    // returns true if bucket of records found and successfully deleted
    bool deleteRecord(int key)
    {
        // no tree
        if (!root)
            return false;

        Bucket* b = NULL;
        Node* curr = root;
        // parent of leaf node
        Node* p = searchForLeafNodeWithKey(&curr, key);

        /*if (!curr)
            return NULL;*/

        // bucket position
        int delete_pos = 0;
        for (delete_pos = 0; delete_pos < curr->size; delete_pos++)
        {
            // returns record if key match
            if (key == curr->key[delete_pos])
            {
                b = reinterpret_cast<Bucket*>(curr->ptr[delete_pos]);
                break;
            }
        }
        // no bucket found
        if(!b)
            return false;

        // TODO: Update parents and merge nodes based on scenarios
        // 3 scenarios:
        //  1. Simple deletion (leaf node still hits minimum num of keys after deletion)
        //      Need to update parent if left most key affected
        //  2. Borrow key from sibling (leaf node lacks min keys, borrows from sibling that has more than min keys)
        //      Needs to update parent after borrow and loop
        //  3. Merge with sibling (leaf node lacks min keys, sibling only has min keys)
        //      Needs to update parent after merge and loop

        // Simple deletion
        if (curr->size - 1 >= min_key_in_leaf)
        {
            cout << "SIMPLE DELETION" << endl;

            // delete bucket from node
            for (int i = curr->size - 1; i > delete_pos; i--)
            {
                curr->key[i - 1] = curr->key[i];
                curr->ptr[i - 1] = curr->ptr[i];
            }
            curr->key[curr->size - 1] = NULL;
            curr->ptr[curr->size - 1] = NULL;
            curr->size--;
            delete b;
            num_of_buckets--;

            // if whole node is deleted, eg. N = 2
            if (curr->size == 0)
            {
                // if curr is the only node in the tree
                if (curr == root)
                {
                    // delete whole tree
                    delete[] curr->key;
                    delete[] curr->ptr;
                    delete curr;
                    root = NULL;
                    return true;
                }

                // TODO: delete leaf node and update parents
                // might need to recursively update parents as parent nodes may merge as well
                // might need to set root as well


            }
            // if the first key of the leaf node is deleted
            if (delete_pos == 0)
            {
                // update parent
                changeKeyParentUpdate(p, key, curr->key[0]);
            }
        }
        // Borrow key from sibling
        else
        {
            // sibling nodes
            Node* sl = NULL; Node* sr = NULL;
            tie(sl, sr) = getLeafSiblings(p, curr);
            // sibling's parent
            Node* sp = NULL;

            // debugging
            if (sl)
            {
                cout << "sl\t";
                sl->print();
            }
            if (sr)
            {
                cout << "sr\t";
                sr->print();
            }

            if (sl && sl->size-1 >= min_key_in_leaf)
            {
                // borrow from left sibling
                cout << "BORROWING FROM LEFT" << endl;

                // delete bucket from node
                for (int i = curr->size - 1; i > delete_pos; i--)
                {
                    curr->key[i - 1] = curr->key[i];
                    curr->ptr[i - 1] = curr->ptr[i];
                }
                curr->key[curr->size - 1] = NULL;
                curr->ptr[curr->size - 1] = NULL;
                curr->size--;
                delete b;
                num_of_buckets--;

                // shifting curr node keys
                for (int i = 0; i < curr->size; i++)
                {
                    curr->key[i + 1] = curr->key[i];
                    curr->ptr[i + 1] = curr->ptr[i];
                }
                // transferring sibling key
                curr->key[0] = sl->key[sl->size-1];
                curr->ptr[0] = sl->ptr[sl->size - 1];
                curr->size++;
                sl->key[sl->size - 1] = NULL;
                sl->ptr[sl->size - 1] = NULL;
                sl->size--;

                // update parent keys
                sp = getLeafParent(root, sl);
                changeKeyParentUpdate(sp, key, curr->key[0]);
            }
            else if (sr && sr->size-1 >= min_key_in_leaf)
            {
                // borrow from right sibling
                cout << "BORROWING FROM RIGHT" << endl;

                // delete bucket from node
                for (int i = curr->size - 1; i > delete_pos; i--)
                {
                    curr->key[i - 1] = curr->key[i];
                    curr->ptr[i - 1] = curr->ptr[i];
                }
                curr->key[curr->size - 1] = NULL;
                curr->ptr[curr->size - 1] = NULL;
                curr->size--;
                delete b;
                num_of_buckets--;

                // transferring sibling key
                curr->key[curr->size] = sr->key[0];
                curr->ptr[curr->size] = sr->ptr[0];
                curr->size++;
                // shifting sibling node keys
                for (int i = 0; i < sr->size - 1; i++)
                {
                    sr->key[i] = sr->key[i + 1];
                    sr->ptr[i] = sr->ptr[i + 1];
                }
                // deleting last key in sibling
                sr->key[sr->size - 1] = NULL;
                sr->ptr[sr->size - 1] = NULL;
                sr->size--;

                // update parent keys
                sp = getLeafParent(root, sr);
                changeKeyParentUpdate(sp, curr->key[curr->size-1], sr->key[0]);
            }
            else
            {
                // merge with sibling node
                cout << "MERGING" << endl;

                // delete bucket from node
                for (int i = curr->size - 1; i > delete_pos; i--)
                {
                    curr->key[i - 1] = curr->key[i];
                    curr->ptr[i - 1] = curr->ptr[i];
                }
                curr->key[curr->size - 1] = NULL;
                curr->ptr[curr->size - 1] = NULL;
                curr->size--;
                delete b;
                num_of_buckets--;

                // TODO: merging of nodes
                // might need to recursively update parents as parent nodes may merge as well
                // might need to set root as well

                // merge with left sibling
                if (p == getLeafParent(root, sl))
                {
                    cout << "MERGE W LEFT" << endl;

                    // TODO:


                }
                // merge with right sibling
                else if (p == getLeafParent(root, sr))
                {
                    cout << "MERGE W RIGHT" << endl;

                    // store position of key in parent for updating
                    int k = getKeyPositionInNode(p, sr->key[0]);

                    // shift right sibling keys over to curr
                    for (int i = 0; i < sr->size; i++)
                    {
                        curr->key[curr->size] = sr->key[i];
                        curr->ptr[curr->size] = sr->ptr[i];
                        curr->size++;
                    }

                    // link curr to right sibling's right sibling
                    curr->ptr[N] = sr->ptr[N];

                    // delete right sibling node
                    delete[] sr->key;
                    delete[] sr->ptr;
                    delete sr;

                    // shift all parent's key after right sibling to the left
                    for (int i = k; i < p->size - 1; i++)
                    {
                        p->key[i] = p->key[i + 1];
                        p->ptr[i + 1] = p->ptr[i + 2];
                    }
                    p->key[p->size - 1] = NULL;
                    p->ptr[p->size] = NULL;
                    p->size--;

                    // need to further update parent keys
                    changeKeyParentUpdate(p, key, curr->key[0]);
                    
                    // check if need loop merging of parent nodes
                    if (p->size < min_key_in_nonleaf)
                    {
                        cout << "NEED MERGE PARENT NODES" << endl;

                        // TODO: 


                    }

                }
                else
                {
                    cout << "ERROR GETTING SIBLING TO MERGE !!" << endl;
                }


            }
        }

        return true;
    }

// TODO: remove references etc
// from https://www.programiz.com/dsa/b-plus-tree
#pragma region Referenced from online

    Node* getParentNode(Node* tree, Node* child)
    {
        Node* parent = NULL;
        if (tree->isLeaf || reinterpret_cast<Node*>(tree->ptr[0])->isLeaf)
            return NULL;
        for (int i = 0; i <= tree->size; i++) {
            if ((reinterpret_cast<Node*>(tree->ptr[i])) == child) {
                parent = tree;
                return parent;
            }
            else {
                parent = getParentNode(reinterpret_cast<Node*>(tree->ptr[i]), child);
                if (parent)
                    return parent;
            }
        }
        return parent;
    }

    Node* getLeafParent(Node* tree, Node* child)
    {
        Node* parent = NULL;
        if (tree->isLeaf)
            return NULL;
        for (int i = 0; i <= tree->size; i++) {
            if ((reinterpret_cast<Node*>(tree->ptr[i])) == child) {
                parent = tree;
                return parent;
            }
            else {
                parent = getLeafParent(reinterpret_cast<Node*>(tree->ptr[i]), child);
                if (parent)
                    return parent;
            }
        }
        return parent;
    }

    void displayTree(Node* n)
    {
        if (n != NULL)
        {
            cout << "\t";
            for (int i = 0; i < n->size; i++)
            {
                if(n->key[i] == 0)
                    cout << "-\t ";
                else
                    cout << n->key[i] << "\t ";
            }
            cout << endl;
            if (!n->isLeaf)
            {
                for (int i = 0; i < n->size + 1; i++)
                {
                    cout << "c" << i << ":\n";
                    displayTree(reinterpret_cast<Node*>(n->ptr[i]));
                }
            }
        }
    }

#pragma endregion

};

int getTotalRecordCount() {

    ifstream myfile;
    myfile.open(DATA_FILE);

    string line;
    int line_count = 0;
    bool skippedFirstLine = false;

    while (getline(myfile, line))
    {
        if (!skippedFirstLine)
            skippedFirstLine = true;
        else {
            line_count++;
        }
    }

    myfile.close();
    return line_count;
}

void retrieveData(Disk_Block *disk, int blocks_utilized)
{
    ifstream myfile;
    myfile.open(DATA_FILE);

    string line;
    bool skippedFirstLine = false;
    int rec_counter = 0;
    int blk_counter = 0;

    while (getline(myfile, line))
    {
        if (!skippedFirstLine)
            skippedFirstLine = true;
        else
        {
            if (rec_counter >= RECORDS_PER_BLOCK)
            {
                disk[blk_counter].id = blk_counter + 1;

                if (++blk_counter >= blocks_utilized)
                    break;
                rec_counter = 0;
            }

            Record r;

            istringstream iss(line);
            string id,avg_rating,num_of_votes;

            getline(iss, id, '\t');
            getline(iss, avg_rating, '\t');
            getline(iss, num_of_votes, '\t');

            id = id.substr(2);
            r.id = stoi(id);
            r.avg_rating = stof(avg_rating);
            r.num_of_votes = stoi(num_of_votes);

            disk[blk_counter].records[rec_counter] = r;
            disk[blk_counter].size++;
            rec_counter++;
        }
    }

    // last block
    disk[blocks_utilized - 1].id = blocks_utilized;
    for (int i = rec_counter; i < RECORDS_PER_BLOCK; i++)
    {
        disk[blocks_utilized - 1].records[i].id = NULL;
        disk[blocks_utilized - 1].records[i].avg_rating = NULL;
        disk[blocks_utilized - 1].records[i].num_of_votes = NULL;
    }

    myfile.close();

}

int main()
{
    // printing of the database statistics etc.
#pragma region Database statistics

    // Total number of records in data
    // initialized as -1 for debugging
    int TOTAL_RECORD_COUNT = -1;
    // Total number of blocks utilized to store records
    // initialized as -1 for debugging
    int BLOCKS_WITH_RECORDS = -1;
    // Size of Database (Size of total blocks utilized in BYTES)
    int DATABASE_SIZE = -1;

    cout << "Disk capacity:\t" << DISK_SIZE << "B" << endl;
    cout << "Block size:\t" << BLOCK_SIZE << "B"  << endl;
    cout << "Blocks in disk:\t" << BLOCKS_IN_DISK << endl;
    cout << "Record size:\t" << RECORD_SIZE << "B"  << endl;
    cout << "Num of records in a block:\t" << RECORDS_PER_BLOCK << endl;

    // initialized based on data.tsv
    TOTAL_RECORD_COUNT = getTotalRecordCount();
    cout << "Total num of records:\t" << TOTAL_RECORD_COUNT << endl;

#pragma endregion

    // Experiment 1 - Initialise database and store records
#pragma region Experiment 1

    cout << "\n\tExperiment 1:" << endl << endl;

    BLOCKS_WITH_RECORDS = (int)ceil((float)TOTAL_RECORD_COUNT/(float)RECORDS_PER_BLOCK);
    cout << "Num of blocks utilized:\t" << BLOCKS_WITH_RECORDS << endl;

    DATABASE_SIZE = BLOCKS_WITH_RECORDS*BLOCK_SIZE;
    cout << "Size of database:\t" << static_cast<float>(DATABASE_SIZE)/1024/1024 << "MB\n" << endl;

    // disk memory, containing all data blocks
    Disk_Block* disk;

    disk = new Disk_Block[BLOCKS_WITH_RECORDS];
    retrieveData(disk, BLOCKS_WITH_RECORDS);

#pragma endregion

    // Experiment 2 - Initialise B+ tree and populate with records
#pragma region Experiment 2

    cout << "\tExperiment 2:" << endl << endl;

    cout << "Size of each pointer: " << POINTER_SIZE << "B" << endl;
    cout << "Number of maximum keys in a B+ tree node (n): " << N << endl;
    cout << "Number of maximum records in a B+ tree bucket: " << RECORDS_PER_BUCKET << endl;

    BPlusTree bpt;

    // TODO: populate B+ tree with actual data
    //for (int i = 0; i < BLOCKS_WITH_RECORDS; i++)
    //{
    //    for (int j = 0; j < disk[i].size; j++)
    //    {
    //        //cout << "Record - " << disk[i].records[j].toString() << endl;
    //        bpt.addRecord(&(disk[i].records[j]));
    //    }
    //}

    // for testing/debugging
    Record r1(4, 3.5, 10);
    Record r2(9, 2.7, 20);
    Record r3(214, 2.8, 30);
    Record r4(13, 1.7, 40);
    Record r5(2, 5.7, 50);
    Record r6(8, 5.7, 60);
    Record r7(7, 2.0, 70);
    Record r8(1, 3.9, 80);
    Record r9(214, 2.8, 90);
    Record r10(23, 2.2, 100);
    Record r11(23, 2.2, 35);

    bpt.addRecord(&r1);
    bpt.addRecord(&r2);
    bpt.addRecord(&r3);
    bpt.addRecord(&r4);
    bpt.addRecord(&r5);
    bpt.addRecord(&r6);
    bpt.addRecord(&r7);
    bpt.addRecord(&r8);
    bpt.addRecord(&r9);
    bpt.addRecord(&r10);
    bpt.addRecord(&r11);

    cout << "Number of nodes in the B+ tree: " << bpt.num_of_nodes << endl;
    cout << "Number of buckets in the B+ tree: " << bpt.num_of_buckets << endl;
    cout << "Height of the B+ tree: " << bpt.height << endl;

    bpt.displayTree(bpt.root);

#pragma endregion

    // Experiment 3 - Retrieval of records with a single key
#pragma region Experiment 3

    // testing retrieval of records based on key
    // Experiment 3
    cout << "\n\tExperiment 3:" << endl << endl;
    cout << "Number and Content of the Index Nodes the Process Accesses: " << endl;

    int k = 500;
//    int k = 12;
    float avg_rating = 0;
    float total_rating = 0;
    Record* recArray[5];
    int recCounter = 0;

    if (bpt.getBucket(k,false))
    {
        Bucket* b = bpt.getBucket(k,false);
        int bi = 1;

        cout << "Num of records per bucket: " << RECORDS_PER_BUCKET << "\n"<< endl;

        while (b->overflowed)
        {
            for (int i = 0; i < b->size; i++)
            {
                cout << "#" << bi << ": " << reinterpret_cast<Record*>(b->ptr[i])->toString() << endl;
                if(recCounter < 5)
                {
                    recArray[recCounter] = reinterpret_cast<Record*>(b->ptr[i]);
                    recCounter++;
                }
                total_rating += reinterpret_cast<Record*>(b->ptr[i])->avg_rating;
                bi++;
            }
            b = reinterpret_cast<Bucket*>(b->ptr[RECORDS_PER_BUCKET]);
        }
        for (int i = 0; i < b->size; i++)
        {
            cout << "#" << bi << ": " << reinterpret_cast<Record*>(b->ptr[i])->toString() << endl;
            if(recCounter < 5)
            {
                recArray[recCounter] = reinterpret_cast<Record*>(b->ptr[i]);
                recCounter++;
            }
            total_rating += reinterpret_cast<Record*>(b->ptr[i])->avg_rating;
            bi++;
        }

        avg_rating = total_rating/(bi-1);
    }
    else
        cout << "Record w NumOfVotes == " << k << " cannot be found !" << endl;

    // the number and the content of index nodes the process accesses
    bpt.getBucket(k,true);

    // the number and the content of data blocks the process accesses
    for(int i = 0; i < recCounter; i++)
    {
        for(int j = 0; j < BLOCKS_WITH_RECORDS; j++)
        {
            for(int m = 0; m < RECORDS_PER_BLOCK; m++)
            {
                if(recArray[i]->id == disk[j].records[m].id)
                {
                    cout << "\nDisk Block " << j+1 << endl;

                    for(int p = 0; p < RECORDS_PER_BLOCK; p++)
                    {
                        cout << "Record " << p+1 << " tconst value: " << disk[j].records[p].id << endl;
                    }
                }
            }
        }
    }

    // the average of ¡°averageRating¡¯s¡± of the records that are returned
    cout << "\nAverage Rating =  " << avg_rating << endl;

#pragma endregion

    // Experiment 4 - Retrieval of records with a range of keys
#pragma region Experiment 4



#pragma endregion

    // Experiment 5 - Deletion of records based on key
#pragma region Experiment 5

    cout << "\n\tExperiment 5:" << endl << endl;

    // delete records
    bpt.deleteRecord(70);

    // display results
    cout << "Number of nodes in the B+ tree: " << bpt.num_of_nodes << endl;
    cout << "Number of buckets in the B+ tree: " << bpt.num_of_buckets << endl;
    cout << "Height of the B+ tree: " << bpt.height << endl;

    bpt.displayTree(bpt.root);

#pragma endregion

    // for debugging the database storage
#pragma region debugging Disk_block

    // for debugging
//    int diskno = 0;
//
//    cout << "Input disk number to read records: ";
//    // lazy do error checking and handling
//    // must be 0 <= diskno < BLOCKS_WITH_RECORDS
//    cin >> diskno;
//
//    while(diskno != -1)
//    {
//        cout << endl << "Disk " << diskno << ":" << endl;
//        cout << "Disk id: " << disk[diskno-1].id << endl;
//        for (int i = 0; i < RECORDS_PER_BLOCK; i++)
//        {
//            cout << disk[diskno-1].records[i].id << " :\t" << disk[diskno-1].records[i].avg_rating << "\t| " << disk[diskno-1].records[i].num_of_votes << endl;
//        }
//        cout << endl;
//        cin >> diskno;
//    }


#pragma endregion


    // deletes memory from pointer
    // might be improper delete, need to check
    delete[] disk;

    return 0;
}
