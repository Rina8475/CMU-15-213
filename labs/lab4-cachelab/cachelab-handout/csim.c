#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachelab.h"

/* argument flag */
#define HELP_FLAG   (1 << 9 )           /* Set this value to avoid conflict with ASCII code */   
#define VERB_FLAG   (1 << 10)
#define S_FLAG      (1 << 11)
#define E_FLAG      (1 << 12)
#define B_FLAG      (1 << 13)
#define TRACE_FLAG  (1 << 14)
#define LABEL_FLAG  (S_FLAG | E_FLAG | B_FLAG | TRACE_FLAG)
#define FLAG_SET    (LABEL_FLAG | HELP_FLAG | VERB_FLAG)

#define MAXLINE 64      /* Assume the length of lines will not over 64 */

struct cache_line {
    unsigned char valid;
    unsigned int  target;
    unsigned int  time;
};

struct cache_set {
    struct cache_line* lines;
    unsigned line_size;
};

struct cache {
    struct cache_set* sets;
    unsigned set_size;
};

typedef struct cache Cache;

char *help_info = 
"Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
"Options:\n"
"  -h         Print this help message.\n"
"  -v         Optional verbose flag.\n"
"  -s <num>   Number of set index bits.\n"
"  -E <num>   Number of lines per set.\n"
"  -b <num>   Number of block offset bits.\n"
"  -t <file>  Trace file.\n\n"
"Examples:\n"
"  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
"  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n";

int hit_count = 0;
int miss_count = 0;
int evict_count = 0;
unsigned time = 0;

unsigned parse_argument(char *);        /* function declaration */
void print_err_msg(char *);
void cache_simulator(int s, int e, int b, FILE* fp, int verb);

int main(int argc, char *argv[]) {
    // parse argument
    int s, e, b, i;
    unsigned flag, temp;
    char *filename, msg[MAXLINE];
    FILE *fp;
    
    flag = temp = 0;
    for (i = 1; i < argc && argv[i][0] == '-'; i += 1) {
        temp = parse_argument(argv[i] + 1);
        if (temp & HELP_FLAG) {             // exam the flags
            print_err_msg("");              // only print help message
        } else if ((temp & FLAG_SET) == 0) {                 
            sprintf(msg, "%s: invalid option -- \'%c\'\n", argv[0], temp);
            print_err_msg(msg);
        } else if ((temp & LABEL_FLAG) != 0) {
            i += 1;
            switch (temp) {
            case S_FLAG:
                s = atoi(argv[i]);
                break;
            case E_FLAG:
                e = atoi(argv[i]);
                break;
            case B_FLAG:
                b = atoi(argv[i]);
                break;
            case TRACE_FLAG:
                filename = argv[i];
                break;
            default:
                fprintf(stderr, "invalid flag %d.\n", temp);
                break;
            }
        }
        flag |= temp;
    }

    if (i != argc) {
        sprintf(msg, "%s: Missing required command line argument\n", argv[0]);
        print_err_msg(msg);          
    } 
    
    if ((fp = fopen(filename, "r")) == NULL) {          // open file
        fprintf(stderr, "%s: No such file or directory\n", filename);
        exit(1);
    }


    // simulate the procedure
    cache_simulator(s, e, b, fp, (flag & VERB_FLAG) == VERB_FLAG);
    fclose(fp);                 // close file

    // print result
    printSummary(hit_count, miss_count, evict_count);
    return 0;
}

/* Parse an argument and return the flags */
unsigned parse_argument(char *arg) {
    unsigned flag = 0;

    for (int i = 0; arg[i] != '\0'; i += 1) 
        switch (arg[i]) {
        case 'h':
            flag |= HELP_FLAG;
            break;
        case 'v':
            flag |= VERB_FLAG;
            break;
        case 's':
            return S_FLAG;
        case 'E':
            return E_FLAG;
        case 'b':
            return B_FLAG;
        case 't':
            return TRACE_FLAG;
        default:
            return (unsigned char) arg[i];
        }
    return flag;
}

/* Print an error message and exit this program */
void print_err_msg(char *msg) {
    fprintf(stderr, "%s%s", msg, help_info);
    exit(1);
}


/*******************
 *  Cache Simulator
 *******************/

void init_cache(Cache *cache, int s, int e);        /* function declaration */
int cache_find(Cache cache, int block_idx);
int cache_put(Cache cache, int block_idx);
void do_instruct(Cache cache, char inst, int block_idx);
void do_instruct_print(Cache cache, char inst, int block_idx);

/** The behaviour of simulator accroding to write-back and 
 * write-allocate policy 
 * Parameter VERB decide if display the trace info
 * 
 * ! Attention: The counts of hit, miss, evict are return by global variable */
void cache_simulator(int s, int e, int b, FILE* fp, int verb) {
    unsigned addr, block_idx;
    char instruct, line[MAXLINE];
    Cache cache;
    
    init_cache(&cache, s, e);
    // read a line
    while (fgets(line, MAXLINE, fp) != NULL) {
        if (*line != ' ')       // Instruction cache access
            continue;
        sscanf(line, " %c %x, ", &instruct, &addr);         // the input address is hexadecimal
        block_idx = addr / (1 << b);            // B = 1 << b
        // deal a line
        if (verb) {
            char *end = strchr(line, '\n');         // Assume every line has char '\n'
            *end = '\0';                            // remove char '\n' at the end of the line
            fprintf(stdout, "%s", line + 1);        // remove the space at the begin of the line
            do_instruct_print(cache, instruct, block_idx);
        } else {
            do_instruct(cache, instruct, block_idx);
        }
    }
    
}

/* Accroding the instruction to calculate the counts */
void do_instruct(Cache cache, char inst, int block_idx) {
    if (cache_find(cache, block_idx)) {
        hit_count += 1;
    } else {
        evict_count += !cache_put(cache, block_idx);
        miss_count += 1;
    }
    if (inst == 'M')
        hit_count += 1; 
}

/* Accroding the instruction to calculate the counts and print the information */
void do_instruct_print(Cache cache, char inst, int block_idx) {
    if (cache_find(cache, block_idx)) {
        hit_count += 1;
        fprintf(stdout, " hit");
    } else {
        miss_count += 1;
        fprintf(stdout, " miss");
        if (!cache_put(cache, block_idx)) {
            evict_count += 1;
            fprintf(stdout, " eviction");
        }
    }
    if (inst == 'M') {
        hit_count += 1; 
        fprintf(stdout, " hit");
    }
    fputc('\n', stdout);
}


/***************************
 *  Data Structure of Cache
 ***************************/

void init_cache_set(struct cache_set *set, int e);          /* function declaration */
int cache_set_find(struct cache_set set, int target);
int cache_set_put(struct cache_set set, int target);

/* This part complement a Cache Object */

/* Cache method: init */
void init_cache(Cache *cache, int s, int e) {
    cache->set_size = (1 << s);             // S = 1 << s
    cache->sets = malloc(sizeof(struct cache_set) * (size_t) cache->set_size);
    for (int i = 0; i < cache->set_size; i += 1) 
        init_cache_set(&cache->sets[i], e);
}

/* Cache method: find a block whose index is BLOCK_IDX in the CACHE 
 If find this block successfully, then return True, else False */
int cache_find(Cache cache, int block_idx) {
    int set_idx, target;
    
    set_idx = block_idx % cache.set_size;
    target  = block_idx / cache.set_size;
    return cache_set_find(cache.sets[set_idx], target);
}

/* Cache method: put a block whose index is BLOCK_IDX into the CACHE
 If replace a blank block, then return True, else False */
int cache_put(Cache cache, int block_idx) {
    int set_idx, target;

    set_idx = block_idx % cache.set_size;
    target  = block_idx / cache.set_size;
    return cache_set_put(cache.sets[set_idx], target);
}


/* This part complement a cache_set Object */

/* cache_set method: init */
void init_cache_set(struct cache_set *set, int e) {
    set->line_size = e;
    set->lines = malloc(sizeof(struct cache_line) * (size_t) e);
    for(int i = 0; i < e; i += 1)           // initialize all the valid bits
        set->lines[i].valid = 0;
}

/* cache_set method: find a block whose target bits is TARGET in the SET 
 If find this block successfully, then return True, else False */
int cache_set_find(struct cache_set set, int target) {
    for (int i = 0; i < set.line_size; i += 1) 
        if (set.lines[i].valid && set.lines[i].target == target) {
            set.lines[i].time = time;       // flush time field
            time += 1;
            return 1;               // return True
        }
    return 0;                       // return False
}

/* cache_set method: put a block whose target bits is TARGET into the SET
 If replace a blank block, then return True, else False 
 ! Attention: This method use LRU policy */
int cache_set_put(struct cache_set set, int target) {
    int replace, min, i, result;
    
    replace = 0;
    min = set.lines[0].time;
    for (i = 0; i < set.line_size && set.lines[i].valid; i += 1)    // find invalid block or least recently used block
        if (set.lines[i].time < min) {
            replace = i;
            min = set.lines[i].time;
        }
    result = (i < set.line_size);
    if (result) {                       // find invalid block
        replace = i;
        set.lines[i].valid = 1;
    }
    set.lines[replace].target = target;
    set.lines[replace].time   = time;
    time += 1;
    return result;
}
