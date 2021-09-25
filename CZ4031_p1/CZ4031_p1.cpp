
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <math.h>

using namespace std;

#define DISK_SIZE           100000000
#define BLOCK_SIZE          100
#define BLOCKS_IN_DISK      (DISK_SIZE/BLOCK_SIZE)
#define RECORD_SIZE         sizeof(Record)
#define RECORDS_PER_BLOCK   ((BLOCK_SIZE-sizeof(int))/RECORD_SIZE)

#define POINTER_SIZE        sizeof(uintptr_t)//4
#define N                   floor((BLOCK_SIZE-POINTER_SIZE)/(POINTER_SIZE+sizeof(int)))

#define DATA_FILE           "dataTest.tsv"

struct Record
{
    int id;
    float avg_rating;
    int num_of_votes;

    int getRecordSize()
    {
        return sizeof(id) + sizeof(avg_rating) + sizeof(num_of_votes);
    }
};


struct Disk_Block
{
    // header includes id and record length
    // not sure if can omit header
    int id;

    Record records[RECORDS_PER_BLOCK];
};

class Node
{
public:
    uintptr_t* ptr;
    int* key;

    Node()
    {
        ptr = new uintptr_t[N + 1]{NULL};
        key = new int[N] {NULL};
    }

    ~Node()
    {
        //delete[] ptr;
        //delete[] key;
    }
};

class BPlusTree
{
public:

    Node* root;
    Node* first_leaf;
    int num_of_leaf_nodes;

    BPlusTree()
    {
        root = NULL;
        first_leaf = NULL;
        num_of_leaf_nodes = 0;
    }

    ~BPlusTree()
    {
        //delete root;
        //delete first_leaf;
    }

    void addNode(Node n)
    {

    }

    void addRecord(Record *r)
    {
        if (!root)
        {
            Node* n = new Node();
            root = n;
            first_leaf = n;
            num_of_leaf_nodes = 1;
            n->ptr[0] = (uintptr_t )r;
        }
        else
        {
            root->ptr[1] = (uintptr_t)r;
        }
    }
    
    Record* getRecord(int key)
    {
        // search from root

        if (!root)
        {
            return NULL;
        }

        return reinterpret_cast<Record *>(root->ptr[key]);
    }

    void displayTree(Node* n)
    {
        // from https://www.programiz.com/dsa/b-plus-tree
        if (n != NULL)
        {
            for (int i = 0; i < N; i++)
            {
                cout << n->key[i] << " ";
            }
            cout << "\n";
            if (!nodeIsLeaf(n))
            {
                for (int i = 0; i < N + 1; i++)
                {
                    displayTree(reinterpret_cast<Node*>(n->ptr[i]));
                }
            }
        }
    }

    bool nodeIsLeaf(Node* n)
    {
        Node* addr = NULL;
        for (int i = 0; i < num_of_leaf_nodes; i++)
        {
            addr = (first_leaf+i);
            if (addr == n)
            {
                return true;
            }
        }

        return false;
    }

};

// block_size = 100B / 500B
// disk_size = 100 - 500MB(we can try getting user input to modify disk size, or fix it to a value)
// blocks_in_disk = disk_size / block_size
// num_of_records = number of lines in data.tsv – 1 (header)
// record_size = sizeof(int) + sizeof(float) + sizeof(int)
// records_per_block = (block_size – header_size) / record_size
//
// Block header : int block_id, int record_size(? May not need if all records have same size)
//
// For packing of fields into records, use “Fixed format with fixed length”(easier, but wastes memory)
// For packing records into blocks, use unspanned.
//
// For saving to disk, we can try saving into a .dat file
//
// As for disk memory simulated on main memory (experiment 1), we can just save to stack

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

    cout << "\n\tExperiment 1:" << endl;

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

    Record r;
    r.id = 4;
    r.avg_rating = 3.5;
    r.num_of_votes = 6;

    Record r2;
    r2.id = 9;
    r2.avg_rating = 2.7;
    r2.num_of_votes = 18;

    bpt.addRecord(&r);
    bpt.addRecord(&r2);


    cout << bpt.getRecord(0)->id << endl;
    cout << bpt.getRecord(0)->avg_rating << endl;
    cout << bpt.getRecord(0)->num_of_votes << endl;
    cout << bpt.getRecord(1)->id << endl;
    cout << bpt.getRecord(1)->avg_rating << endl;
    cout << bpt.getRecord(1)->num_of_votes << endl;

    bpt.displayTree(bpt.root);

    //for (int i = 0; i < BLOCKS_WITH_RECORDS; i++)
    //{
    //    for (int j = 0; j < RECORDS_PER_BLOCK; j++)
    //    {
    //        //bpt.addRecord(disk[i].records[j]);
    //    }
    //}

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
