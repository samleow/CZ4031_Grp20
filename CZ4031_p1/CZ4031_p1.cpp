
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
// num_of_records = number of lines in data.tsv � 1 (header)
// record_size = sizeof(int) + sizeof(float) + sizeof(int)
// records_per_block = (block_size � header_size) / record_size
// 
// Block header : int block_id, int record_size(? May not need if all records have same size)
// 
// For packing of fields into records, use �Fixed format with fixed length�(easier, but wastes memory)
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
                if (++blk_counter >= 9)
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

    myfile.close();

}

int main()
{
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

    //cout << getTotalRecordCount() << endl << endl;

    // disk memory, containing all data blocks
    Disk_Block* disk;

    // for now just 9 blocks, coz stack mem not enough for all 9mil r
    //Disk_Block Disk[BLOCKS_IN_DISK];
    disk = new Disk_Block[9];
    retrieveData(disk);

#pragma region debugging Disk_block

    // for debugging
    int diskno = 0;
    for (diskno = 0; diskno < 9; diskno++)
    {
        cout << "Disk " << diskno << ":" << endl;
        cout << "Disk id: " << disk[diskno].id << endl;
        for (int i = 0; i < RECORDS_PER_BLOCK; i++)
        {
            cout << disk[diskno].records[i].id << " :\t" << disk[diskno].records[i].avg_rating << "\t| " << disk[diskno].records[i].num_of_votes << endl;
        }
        cout << endl;
    }

#pragma endregion


    // deletes memory from pointer
    delete[] disk;
}
