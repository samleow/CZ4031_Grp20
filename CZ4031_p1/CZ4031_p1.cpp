
#include <iostream>
#include <vector>

using namespace std;

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
    int record_len;

    // using vector as a variable length list
    // preferred to use array
    // need recheck Disk_Block total size, coz vector may affect size
    vector<Record> records;
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

int main()
{

    // disk capacity (in MB)
    // 100 - 500
    int disk_cap = 100;
    cout << "Disk capacity:\t" << disk_cap << "MB" << endl;

    // block size (in B)
    // should get input for block size 100 or 500
    int block_size = 100;
    cout << "Block size:\t" << block_size << "B"  << endl;

    // number of blocks in the disk
    int blocks_in_disk = (disk_cap * pow(10,6)) / block_size;
    cout << "Blocks in disk:\t" << blocks_in_disk << endl;

    cout << "Record size:\t" << sizeof(Record) << "B"  << endl;

    // this is not accounting for header size !!
    // not sure if correct or not !!
    // if accounting for header size, need to be = floor((block_size - sizeof(Disk_Block.id) - sizeof(Disk_Block.record_len)) / sizeof(t))
    int records_per_block = floor(block_size / sizeof(Record));
    cout << "Num of records in a block:\t" << records_per_block << endl << endl;

    // read from data file here onwards
    // Get data and store to "disk"
    // need count number of records to store and check if disk capacity can hold all data

#pragma region testing block and record init

    // testing block and record init
    // for now testing with only one block
    Disk_Block b1;
    b1.id = 1;
    b1.record_len = sizeof(Record);
    b1.records.resize(records_per_block);

    for (int i=0; i<records_per_block; i++)
    {
        // dummy values
        Record r = {i, 1.5*i, 2*i};
        // this is passing by value
        // will be good to pass by reference
        b1.records[i] = r;
    }
    cout << "Block 1 size: " << sizeof(b1) << "B" << endl;
    cout << "vector size: " << sizeof(vector<Record>) << "B" << endl;
    cout << "Block 1 vector size: " << sizeof(Record)*b1.records.size() << "B" << endl;
    cout << "Block 1 actual size: " << sizeof(b1) + sizeof(Record) * b1.records.size() << "B" << endl << endl;

    for (int i=0; i<b1.records.size(); i++)
    {
        cout << "Block 1 record " << b1.records[i].id << "'s avg rating:\t\t" << b1.records[i].avg_rating << endl;
        cout << "Block 1 record " << b1.records[i].id << "'s num of votes:\t" << b1.records[i].num_of_votes << endl << endl;
    }
#pragma endregion

}
