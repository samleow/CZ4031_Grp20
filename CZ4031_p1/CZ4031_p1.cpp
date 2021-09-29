
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
#define RECORDS_PER_BLOCK   ((BLOCK_SIZE-sizeof(int))/RECORD_SIZE)
#define POINTER_SIZE        sizeof(uintptr_t)//4
#define DATA_FILE           "dataTest.tsv"
// TODO: change back N
const static int N = 4;// floor((BLOCK_SIZE - POINTER_SIZE) / (POINTER_SIZE + sizeof(int)));

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
    int id;

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
        delete[] ptr;
        delete[] key;
    }
};

// TODO: create a class/struct for an overflow block that stores pointers to records of the same key,
// and the last pointer pointing to another overflow block if first block overflows (single linked list method)

class BPlusTree
{
public:

    Node* root;

    // minimum and maximum keys
    int min_key_in_leaf = floor((N + 1) / 2);
    int min_key_in_nonleaf = floor(N / 2);

    BPlusTree()
    {
        root = NULL;
    }

    ~BPlusTree()
    {
        delete root;
    }

    // insert record into leaf node that has space and returns input position
    int insertIntoLeaf(Node* n, Record* r)
    {
        // TODO: change the storage of records into storage of overflow blocks holding records

        bool is_set = false;

        for (int i = 0; i < n->size; i++)
        {
            if (r->num_of_votes < n->key[i])
            {
                //for (int j = i; j < N - 1; j++)
                //{
                //    n->ptr[j + 1] = n->ptr[j];
                //    n->key[j + 1] = n->key[j];

                //}

                // shifts all keys after i to the right
                for(int j = N - 1; j > i; j--)
                {
                    n->ptr[j] = n->ptr[j - 1];
                    n->key[j] = n->key[j - 1];
                }
                // sets i to record
                n->ptr[i] = (uintptr_t)r;
                n->key[i] = r->num_of_votes;
                n->size++;
                is_set = true;
                return i;
            }
        }

        // if record is bigger than the current last key
        if (!is_set)
        {
            n->ptr[n->size] = (uintptr_t)r;
            n->key[n->size] = r->num_of_votes;
            n->size++;
        }
        return n->size - 1;
    }

    // searches through node recursively to get leaf node of record
    void searchForLeafNodeWithRecord(Node** n, Record* r)
    {
        // advances down all levels of tree
        while (!(*n)->isLeaf)
        {
            for (int i = 0; i < (*n)->size; i++)
            {
                // record under the left pointer of key
                if (r->num_of_votes < (*n)->key[i])
                {
                    (*n) = reinterpret_cast<Node*>((*n)->ptr[i]);
                    break;
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
    }

    // searches through node recursively to get leaf node of record
    void searchForLeafNodeWithKey(Node** n, int key)
    {
        // advances down all levels of tree
        while (!(*n)->isLeaf)
        {
            for (int i = 0; i < (*n)->size; i++)
            {
                // record under the left pointer of key
                if (key < (*n)->key[i])
                {
                    (*n) = reinterpret_cast<Node*>((*n)->ptr[i]);
                    break;
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
    }

    // insert a child node into a parent node and returns input position
    int insertChildNode(Node* p, Node* c, int key)
    {
        bool is_set = false;

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
                is_set = true;
                return i;
            }
        }

        // if child key is bigger than the last parent key
        if (!is_set)
        {
            p->size++;
            p->ptr[p->size] = (uintptr_t)c;
            p->key[p->size-1] = key;
        }
        return p->size;
    }

    // splits the full node into two and create a slot for insertion
    // returns the node to insert into and the input position
    tuple<Node*, int> splitFullNodeForInsert(Node* curr, Node* n, int key)
    {
        // TODO: not sure if all nonleaf ptr need + 1 from key pos
        // For nonleaf nodes might need to split based on child(ptr) key[0] instead of key
        // as nonleaf nodes might update parent instead of itself when split
        // Maybe can split this function into either for leaf or nonleaf nodes,
        // instead of everything in one function

        int input_pos = -1;
        int is_nonleaf = !curr->isLeaf ? 1 : 0;
        int min_key = curr->isLeaf ? min_key_in_leaf : min_key_in_nonleaf;
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
            for (int i = min_key; i < N; i++)
            {
                n->ptr[i - min_key] = curr->ptr[i];
                n->key[i - min_key] = curr->key[i];
                n->size++;
                curr->ptr[i] = NULL;
                curr->key[i] = NULL;
                curr->size--;
            }

            // set record into last key in new node
            /*n->ptr[n->size] = (uintptr_t)r;
            n->key[n->size] = r->num_of_votes;
            n->size++;*/
            return make_tuple(n, n->size);
        }
        // if input key is placed in between current keys and shifted
        else
        {
            // if need shift current node's input position to new node
            if (input_pos >= min_key)
            {
                // shift all keys after minimum num from current to new node
                for (int i = min_key; i < N; i++)
                {
                    // offset for keys after input position
                    int o = 0;
                    o = (i >= input_pos) ? 1 : 0;

                    n->ptr[i - min_key + o] = curr->ptr[i];
                    n->key[i - min_key + o] = curr->key[i];
                    n->size++;
                    curr->ptr[i] = NULL;
                    curr->key[i] = NULL;
                    curr->size--;
                }

                // set record into input position of new node
                /*n->ptr[N - input_pos - 1] = (uintptr_t)r;
                n->key[N - input_pos - 1] = r->num_of_votes;
                n->size++;*/

                //cout << "Insert key: " << key << endl;
                //cout << "New node input pos: " << input_pos + n->size - N << endl;
                return make_tuple(n, input_pos + n->size - N);// N - input_pos - 1);
            }
            // if input position is to stay in current node
            else
            {
                cout << "Insert key: " << key << endl;
                cout << "Input pos: " << input_pos << endl;
                for (int i = N - 1; i >= input_pos; i--)
                {
                    // if keys need to be shifted to new node
                    // how I got (i - min_key + 1):
                    // n pos starts from the rightmost shift key
                    // n->ptr[i + shift_amount(N - min_key + 1) - N]
                    // where i is iterator moving left, shift_amount is the amount to shift right and N being the size of the node
                    if (i - min_key + 1 >= 0)
                    {
                        cout << "Shifting " << curr->key[i] << " from curr " << i << " to new " << i - min_key + 1 << endl;
                        n->ptr[i - min_key + 1] = curr->ptr[i];
                        n->key[i - min_key + 1] = curr->key[i];
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
                /*curr->ptr[input_pos] = (uintptr_t)r;
                curr->key[input_pos] = r->num_of_votes;
                curr->size++;*/
                return make_tuple(curr, input_pos);
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
            // TODO: change to overflow block instead of record
            Node* n = new Node();
            root = n;
            n->isLeaf = true;
            n->ptr[0] = (uintptr_t )r;
            n->key[0] = r->num_of_votes;
            n->size++;
        }
        else
        {
            // initialise current node pointer to root of tree
            Node* curr = root;

            // searches through tree till it hit the leaf node w the key
            searchForLeafNodeWithRecord(&curr, r);

            // if leaf node have space
            if (curr->size < N)
            {
                // inserting a records into a leaf node with space
                int input_pos = insertIntoLeaf(curr, r);
                // if record is inserted into first key of leaf node
                if (input_pos == 0)
                {
                    // TODO: need to update parent node if inserting on the first key
                    // also may need to update further parent nodes
                }
            }
            else
            {
                // creating a new leaf node and splitting records equally
                Node* n = new Node();
                curr->ptr[N] = (uintptr_t)n;
                n->isLeaf = true;

                // TODO: change to overflow block instead of record
                Node* t; int input_pos;
                // split full node equally between curr and n nodes
                tie(t, input_pos) = splitFullNodeForInsert(curr, n, r->num_of_votes);
                // sets record into input position
                t->ptr[input_pos] = (uintptr_t)r;
                t->key[input_pos] = r->num_of_votes;
                t->size++;

                // if current leaf node have no parent, create parent and set as root
                if (curr == root)
                {
                    Node* p = new Node();
                    p->ptr[0] = (uintptr_t)curr;
                    p->key[0] = n->key[0];
                    p->ptr[1] = (uintptr_t)n;
                    p->size++;
                    root = p;
                }
                else
                {
                    // TODO: update parents and iterate through the top
                    // if parent node is full, need to split and update parent's parent and iterate to top

                    // this whole else statement might need to loop till parent is root

                    Node* p = getParentNode(root, curr);
                    if (!p)
                    {
                        cout << "ERROR : B+ Tree linkage problem! Child Node with first Key " << n->key[0] << " will not be inserted into the tree." << endl;
                        return;
                    }

                    // if parent node still have space
                    if (p->size < N)
                    {
                        // shift keys in parent node to insert key
                        insertChildNode(p, n, n->key[0]);
                    }
                    else
                    {
                        // split parent nodes and shift keys
                        // need to loop through parents till root

                    }
                }
            }

        }
    }

    // get the first record with given key
    Record* getRecord(int key)
    {
        // TODO: need to implement retrieval of all records in overflow block of key

        if (!root)
            return NULL;

        Node* curr = root;
        searchForLeafNodeWithKey(&curr, key);

        /*if (!curr)
            return NULL;*/

        for (int i = 0; i < curr->size; i++)
        {
            // returns record if key match
            if (key == curr->key[i])
                // TODO: change this from returning a Record to return an overflow block with all records inside
                // overflow block should be a block that works like a single linked list
                return reinterpret_cast<Record*>(curr->ptr[i]);
        }

        return NULL;
    }

// TODO: remove references etc
#pragma region Referenced from online

    Node* getParentNode(Node* tree, Node* child)
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
                parent = getParentNode(reinterpret_cast<Node*>(tree->ptr[i]), child);
                if (parent)
                    return parent;
            }
        }
        return parent;
    }

    // from https://www.programiz.com/dsa/b-plus-tree
    void displayTree(Node* n)
    {
        if (n != NULL)
        {
            cout << "\t";
            for (int i = 0; i < N; i++)
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
            string id = line.substr(2, 7);
            r.id = stoi(id);
            string avg_rating = line.substr(10, 3);
            r.avg_rating = stof(avg_rating);
            string num_of_votes = line.substr(14);
            r.num_of_votes = stoi(num_of_votes);

            disk[blk_counter].records[rec_counter] = r;
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

#pragma region Experiment 1

    cout << "\n\tExperiment 1:" << endl << endl;

    BLOCKS_WITH_RECORDS = (int)ceil((float)TOTAL_RECORD_COUNT/(float)RECORDS_PER_BLOCK);
    cout << "Num of blocks utilized:\t" << BLOCKS_WITH_RECORDS << endl;

    DATABASE_SIZE = BLOCKS_WITH_RECORDS*BLOCK_SIZE;
    cout << "Size of database:\t" << static_cast<float>(DATABASE_SIZE)/1024/1024 << "MB\n" << endl;

    // disk memory, containing all data blocks
    Disk_Block* disk;

    // for now just 9 blocks, coz stack mem not enough for all 9mil r
    //Disk_Block Disk[BLOCKS_IN_DISK];
    disk = new Disk_Block[BLOCKS_WITH_RECORDS];
    retrieveData(disk, BLOCKS_WITH_RECORDS);

#pragma endregion

#pragma region Experiment 2

    cout << "\n\tExperiment 2:" << endl;

    cout << "\nSize of each pointer: " << POINTER_SIZE << "B" << endl;
    cout << "Number of maximum keys in a B+ tree node (n): " << N << endl;

    BPlusTree bpt;

    // TODO: populate B+ tree with actual data
    //for (int i = 0; i < BLOCKS_WITH_RECORDS; i++)
    //{
    //    for (int j = 0; j < RECORDS_PER_BLOCK; j++)
    //    {
    //        //bpt.addRecord(disk[i].records[j]);
    //    }
    //}

    // for testing/debugging
    Record r1(4, 3.5, 1);
    Record r2(9, 2.7, 2);
    Record r3(214, 2.8, 5);
    Record r4(13, 1.7, 4);
    Record r5(2, 5.7, 3);
    Record r6(8, 5.7, 9);
    Record r7(7, 2.0, 12);
    Record r8(1, 3.9, 18);
    Record r9(214, 2.8, 6);
    Record r10(2, 5.7, 20);
    Record r11(8, 5.7, 8);
    Record r12(7, 2.0, 7);
    Record r13(6, 3.9, 11);
    Record r14(3, 3.3, 10);
    Record r15(13, 1.3, 15);
    Record r16(21, 2.7, 14);

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
    bpt.addRecord(&r12);
    bpt.addRecord(&r13);
    bpt.addRecord(&r14);
    bpt.addRecord(&r15);/**/
    //bpt.addRecord(&r16);

    bpt.displayTree(bpt.root);

    // testing retrieval of records based on key
    int k = 10;
    if(bpt.getRecord(k))
        cout << "Record w NumOfVotes == " << k << ": " << bpt.getRecord(k)->toString() << endl;
    else
        cout << "Record w NumOfVotes == " << k << " cannot be found !" << endl;

#pragma endregion


#pragma region debugging Disk_block

    // for debugging
    //int diskno = 0;

    //cout << "Input disk number to read records: ";
    //// lazy do error checking and handling
    //// must be 0 <= diskno < BLOCKS_WITH_RECORDS
    //cin >> diskno;

    //cout << endl << "Disk " << diskno << ":" << endl;
    //cout << "Disk id: " << disk[diskno-1].id << endl;
    //for (int i = 0; i < RECORDS_PER_BLOCK; i++)
    //{
    //    cout << disk[diskno-1].records[i].id << " :\t" << disk[diskno-1].records[i].avg_rating << "\t| " << disk[diskno-1].records[i].num_of_votes << endl;
    //}
    //cout << endl;

#pragma endregion


    // deletes memory from pointer
    delete[] disk;

    return 0;
}
