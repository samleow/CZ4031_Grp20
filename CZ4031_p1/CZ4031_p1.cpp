
#include <iostream>

using namespace std;

struct MovieRating
{
    string id;
    float avg_rating;
    int num_of_votes;
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
    cout << "Hello world!" << endl;
}
