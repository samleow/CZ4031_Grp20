
#include <iostream>
#include <vector>
#include <fstream>
#include <string>

using namespace std;

#define DISK_SIZE           100000000
#define BLOCK_SIZE          100
#define BLOCKS_IN_DISK      DISK_SIZE/BLOCK_SIZE
#define RECORD_SIZE         sizeof(Record)
#define RECORDS_PER_BLOCK   (BLOCK_SIZE-sizeof(int))/RECORD_SIZE

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
    myfile.open("data.tsv");

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

void retrieveData(Disk_Block *disk)
{
    ifstream myfile;
    myfile.open("data.tsv");

    string line;
    bool skippedFirstLine = false;

    while (getline(myfile, line))
    {
        if (!skippedFirstLine)
            skippedFirstLine = true;
        else
        {
            //string id = line.substr(2,7);
            cout << line << endl;
            break;
        }
    }

    myfile.close();

}

int main()
{

    //Disk_Block Disk[BLOCKS_IN_DISK];

    // disk capacity
    cout << "Disk capacity:\t" << DISK_SIZE << "B" << endl;

    // block size
    cout << "Block size:\t" << BLOCK_SIZE << "B"  << endl;

    // number of blocks in the disk
    cout << "Blocks in disk:\t" << BLOCKS_IN_DISK << endl;

    cout << "Record size:\t" << RECORD_SIZE << "B"  << endl;

    // this is not accounting for header size !!
    // not sure if correct or not !!
    // if accounting for header size, need to be = floor((block_size - sizeof(Disk_Block.id) - sizeof(Disk_Block.record_len)) / sizeof(t))
    cout << "Num of records in a block:\t" << RECORDS_PER_BLOCK << endl << endl;

    // read from data file here onwards
    // Get data and store to "disk"
    // need count number of records to store and check if disk capacity can hold all data

    cout << getTotalRecordCount() << endl << endl;

#pragma region testing block and record init

    // testing block and record init
    // for now testing with only one block
    Disk_Block b1;
    b1.id = 1;
    

    for (int i=0; i<RECORDS_PER_BLOCK; i++)
    {
        // dummy values
        Record r = {i, 1.5*i+1, 2*i+1};
        // this is passing by value
        // will be good to pass by reference
        b1.records[i] = r;
    }
    cout << "Block 1 size: " << sizeof(b1) << "B" << endl;

    for (int i=0; i<RECORDS_PER_BLOCK; i++)
    {
        cout << "Block 1 record " << b1.records[i].id << "'s avg rating:\t\t" << b1.records[i].avg_rating << endl;
        cout << "Block 1 record " << b1.records[i].id << "'s num of votes:\t" << b1.records[i].num_of_votes << endl << endl;
    }
#pragma endregion

}
