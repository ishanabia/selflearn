#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NUM_PAGES 256
#define PAGE_SIZE 256
#define FRAME_SIZE 256
#define TLB_SIZE 16
#define MASK 255
#define OFFSET 8
#define CHARS 10

// TLB with page numbers, frame numbers and an index (to keep track of FIFO)
struct TLB {
    unsigned char page[TLB_SIZE];
    unsigned char frame[TLB_SIZE];
    int index;
};

// page table holding frame number, valid bit and counter
struct PageTable {
    char table[NUM_PAGES];
    int valid[NUM_PAGES];
    int counter[NUM_PAGES];
};

int main(int argc, char *argv[]) {

    // check if args are correct
    if(argc != 4) {
        printf("Usage: ./<executable> <memory size> <backing store> <address list>\n");
        exit(0);
    }

    // number of frames: 128 or 256
    int size = atoi(argv[1]);
    // backing store file
    const char *backing = argv[2];
    // address list
    const char *input = argv[3];
    // output file
    const char *output;
    int num_frames;

    // determine number of frames in main memory
    if (size == 256) {
        output = "output256.csv";
        num_frames = 256;
    } else if(size == 128) {
        output = "output128.csv";
        num_frames = 128;
    } else {
        printf("Invalid memory size. Please choose either 128 or 256.\n");
        exit(0);
    }

    // open the files
    FILE *backing_store = fopen(backing, "r");
    FILE *input_fp = fopen(input, "r");
    FILE *output_fp = fopen(output, "w");

    // check for errors
    if(backing_store == NULL || input_fp == NULL) {
        printf("Failed to open file.\n");
        exit(0);
    }

    // init main memory (either 128 or 256 frames)
    char main_mem[num_frames][FRAME_SIZE];

    // init page table
    struct PageTable pt;
    memset(pt.table, -1, sizeof(pt.table));
    memset(pt.valid, 0, sizeof(pt.valid));
    memset(pt.counter, -1, sizeof(pt.counter));

    // init TLB
    struct TLB tlb;
    tlb.index = 0;
    memset(tlb.page, -1, sizeof(tlb.page));
    memset(tlb.frame, -1, sizeof(tlb.frame));

    char logic[CHARS];
    int count = 0, page_fault = 0, free_frame = 0, hits = 0;

    // read each line
    while(fgets(logic, CHARS, input_fp) != NULL) {
        // increment total address counter
        count++;
        // get logical address, page number and offset
        int logical = atoi(logic);
        int page = (logical >> OFFSET) & MASK;
        int offset = logical & MASK;

        int frame = 0, new_frame = 0, hit = 0;
        // check if the page is in the TLB
        for (int i = 0; i < TLB_SIZE; i++) {
            if(tlb.page[i] == page) {
                frame = tlb.frame[i];
                hit = 1;
                hits++;
                break;
            }
        }

        // if the page is not in the TLB
        if(hit == 0) {
            // if the page is also not in main memory
            if(pt.valid[page] == 0) {
                // increment page fault counter
                page_fault++;
                // init the memory contents
                char mem_contents[FRAME_SIZE];
                memset(mem_contents, 0, sizeof(mem_contents));

                // search the backing store for the value
                if (fseek(backing_store, page * FRAME_SIZE, SEEK_SET) != 0)
                    printf("fseek: error\n");

                if (fread(mem_contents, sizeof(char), FRAME_SIZE, backing_store) == 0)
                    printf("fread: error\n");

                // if there is room in main memory
                if (free_frame < num_frames) {
                    // load backing store contents into main mem
                    for(int i = 0; i < FRAME_SIZE; i++) {
                        main_mem[free_frame][i] = mem_contents[i];
                    }
                    // set the new frame
                    new_frame = free_frame;
                    // add frame number to page table
                    pt.table[page] = new_frame;
                    pt.valid[page] = 1;
                    // increment free frame
                    free_frame++;
                } else { // if there is no room in main memory, need page replacement
                    // find least recently used frame in main memory
                    // the page table with largest counter is the least recently used
                    int least = 0;
                    for (int i = 0; i < NUM_PAGES; i++) {
                        if(pt.counter[i] > pt.counter[least]) {
                            least = i;
                        }
                    }
                    // set the new frame to be the least recently used frame
                    new_frame = pt.table[least];
                    // load backing store contents into new frame on main mem
                    for(int i = 0; i < FRAME_SIZE; i++) {
                        main_mem[new_frame][i] = mem_contents[i];
                    }
                    // load frame number into the page table and set the valid bit
                    pt.table[page] = new_frame;
                    pt.valid[page] = 1;
                    // update replaced page to be invalid (not in main memory)
                    pt.table[least] = -1;
                    pt.valid[least] = 0;
                    pt.counter[least] = -1;
                }
            }
            // get the frame number from page table
            frame = pt.table[page];
            // update the TLB with the newly loaded page (FIFO)
            tlb.page[tlb.index] = page;
            tlb.frame[tlb.index] = pt.table[page];
            tlb.index = (tlb.index + 1) % TLB_SIZE;
        }
        // update counter to indicate the page was accessed
        pt.counter[page] = 0;
        // increment all the page counters in the page table
        for(int i = 0; i < NUM_PAGES; i++) {
            if(pt.counter[i] > -1) {
                pt.counter[i]++;
            }
        }
        // get the physical address and the value from main memory
        int physical = ((unsigned char)frame * FRAME_SIZE) + offset;
        int value = *((char*)main_mem + physical);
        // output info to file
        fprintf(output_fp, "%d,%d,%d\n", logical, physical, value);
    }
    // output stats to file
    fprintf(output_fp, "Page Faults Rate, %.2f%%,\n", (page_fault / (count * 1.0)) * 100);
    fprintf(output_fp, "TLB Hits Rate, %.2f%%,", (hits / (count * 1.0)) * 100);
    // close files
    fclose(input_fp);
    fclose(output_fp);
    fclose(backing_store);

    return(0);
}
