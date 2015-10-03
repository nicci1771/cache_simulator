#include<iostream>
#include<cmath>
using namespace std;

char trace_file[100];
FILE *fp_trace=0;
char trace_str[30];

int L1_size = -1;
int L1_assoc = -1;
int L1_blocksize = -1;

int L2_size = -1;
int L2_assoc = -1;
int L2_blocksize = -1;

enum replacement_policy_t{
	LRU,
	FIFO
};
enum write_policy_t{
	WRITE_BACK,
	WRITE_THROUGH
};

enum write_allocate_policy_t{
	NO_WRITE_ALLOCATE,
	WRITE_ALLOCATE
};

replacement_policy_t replacement_policy = LRU;
write_policy_t write_policy = WRITE_BACK;
write_allocate_policy_t write_allocate_policy = NO_WRITE_ALLOCATE;

class MEM_OP{
public:
	char opcode;
	unsigned long long addr;
}mem_op;

class BLOCK{
public:
	int valid_bit;
	int dirty_bit;
	int lru_counter;
	int fifo_counter;
	unsigned long long addr;
	unsigned long long tag;
};

class SET{
public:
	int assoc;
	BLOCK *blocks;
};

class CACHE{
public:
	int cache_level;
	int size;
	int assoc;
	int num_sets;
	int num_tag_bits;
	int num_index_bits;
	int num_blockoffset_bits;
	int block_size;
	SET *sets;
}L1_cache,L2_cache;

class SIM{
public:
	int L1_reads;
	int L1_read_misses;
	int L1_writes;
	int L1_write_misses;
	float L1_miss_rate;
	
	int L2_reads;
	int L2_read_misses;
	int L2_writes;
	int L2_write_misses;
	float L2_miss_rate;
}sim;

void set_cache_params(CACHE *Cache)
{
	if(Cache->cache_level == 1){
		Cache->size = L1_size;
		Cache->assoc = L1_assoc;
		Cache->block_size = L1_blocksize;
	}
	else if(Cache->cache_level == 2){
		Cache->size = L2_size;
		Cache->assoc = L2_assoc;
		Cache ->block_size = L2_blocksize;
	}
	//int total_bits = (int)(log((double)Cache->size)/log(2.0));
	int total_bits = 32;
	Cache->num_sets = (int)(Cache->size/(Cache->assoc * Cache->block_size));
	Cache->num_index_bits = (int)(log((double)Cache->num_sets)/log(2.0));
	Cache->num_blockoffset_bits = (int)(log((double)Cache->block_size)/log(2.0));
	Cache->num_tag_bits = total_bits - Cache->num_index_bits - Cache->num_blockoffset_bits;
	/*if(Cache->cache_level == 1){
		printf("L1 Cache->num_sets=%d\n",Cache->num_sets);
		printf("L1 Cache->num_index_bits=%d\n",Cache->num_index_bits);
		printf("L1 Cache->num_blockoffset_bits=%d\n",Cache->num_blockoffset_bits);
		printf("L1 Cache->num_tag_bits=%d\n",Cache->num_tag_bits);
	}*/
}

void allocate_cache(CACHE *Cache)
{
	int i, j;

	Cache->sets = (SET *)malloc(sizeof(SET) * Cache->num_sets);
	if (!Cache->sets) {
		printf("Memory Allocation Failed!\n");
		exit(1);
	}

	for (i=0 ; i < Cache->num_sets ; i++) {
		Cache->sets[i].assoc = Cache->assoc;
		Cache->sets[i].blocks = (BLOCK *)malloc(sizeof(BLOCK)*Cache->assoc);
		if (!Cache->sets[i].blocks) {
			printf("Memory Allocation Failed\n");
			exit(1);
		}

		for (j=0 ; j < Cache->assoc ; j++) {
			Cache->sets[i].blocks[j].valid_bit = 0;
			Cache->sets[i].blocks[j].dirty_bit = 0;
			Cache->sets[i].blocks[j].lru_counter = 0;
			Cache->sets[i].blocks[j].fifo_counter = 0;
			Cache->sets[i].blocks[j].tag = -1;
			Cache->sets[i].blocks[j].addr = 0;
		}
	}
}

bool handle_read_request(CACHE *Cache, unsigned long long mem_addr, CACHE *next_Cache){
	unsigned long long int tag;
	unsigned int index;

	unsigned long long tmp = mem_addr;
	/*printf("addr :%lld\n",tmp);
	printf("num_index_bits £º%d\n",Cache->num_index_bits);
	printf("num_blockoffset_bits £º%d\n",Cache->num_blockoffset_bits);*/
	index = (tmp >> Cache->num_blockoffset_bits) & ((1 << Cache->num_index_bits) - 1);
	tag = (tmp >> (Cache->num_blockoffset_bits+Cache->num_index_bits));
	//printf("index :%d\n",index);
	//printf("index tag: %d %d\n",index,tag);

	if(Cache->cache_level == 1)
		sim.L1_reads++;
	else if(Cache->cache_level == 2)
		sim.L2_reads++;
	int hit = 0;
	int i;
	for(i = 0; i < Cache->assoc; i++){
		if(replacement_policy == LRU){
			if(Cache->sets[index].blocks[i].valid_bit == 1)
				Cache->sets[index].blocks[i].lru_counter++;
			if(Cache->sets[index].blocks[i].valid_bit == 1 && Cache->sets[index].blocks[i].tag == tag){
				Cache->sets[index].blocks[i].lru_counter = 0;
				hit = 1;
			}
		}
		else if(replacement_policy == FIFO){
			if(Cache->sets[index].blocks[i].valid_bit == 1)
				Cache->sets[index].blocks[i].fifo_counter++;
			if(Cache->sets[index].blocks[i].valid_bit == 1 && Cache->sets[index].blocks[i].tag == tag){
				hit = 1;
			}
		}
	}

	if(hit == 1)
		return true;
	//hit == 0
	//find one to fill -> i

	if(Cache->cache_level == 1)
		sim.L1_read_misses++;
	else if(Cache->cache_level == 2)
		sim.L2_read_misses++;
	int maxlru = 0;
	int maxnum = 0;
	int maxfifo = 0;
	for(i = 0; i < Cache->assoc; i++){
		if(Cache->sets[index].blocks[i].valid_bit == 0)
			break;
		if(replacement_policy == LRU){
			if(Cache->sets[index].blocks[i].lru_counter > maxlru){
				maxlru = Cache->sets[index].blocks[i].lru_counter;
				maxnum = i;
			}
		}
		else if(replacement_policy == FIFO){
			if(Cache->sets[index].blocks[i].fifo_counter > maxfifo){
				maxlru = Cache->sets[index].blocks[i].lru_counter;
				maxnum = i;
			}
		}
	}
	if(i == Cache->assoc)
		i = maxnum; //find i to be replaced

	if(next_Cache){
		handle_read_request(next_Cache, mem_addr, NULL);
	}
	Cache->sets[index].blocks[i].valid_bit = 1;
	Cache->sets[index].blocks[i].tag = tag;
	Cache->sets[index].blocks[i].lru_counter = 0;
	Cache->sets[index].blocks[i].fifo_counter = 0;
	Cache->sets[index].blocks[i].addr = mem_addr;
	return true;
}

void invalidate(CACHE* Cache, unsigned long long mem_addr){
	unsigned long long tag;
	unsigned int index;

	unsigned long long tmp = mem_addr;
	index = (tmp >> Cache->num_blockoffset_bits) & ((1 << Cache->num_index_bits) - 1);
	tag = (tmp >> (Cache->num_blockoffset_bits+Cache->num_index_bits));
	for(int i = 0; i < Cache->assoc; i++){
		if(Cache->sets[index].blocks[i].valid_bit == 1 && Cache->sets[index].blocks[i].tag == tag){
				Cache->sets[index].blocks[i].valid_bit = 0;
				break;
		}
	}
}

bool handle_write_request(CACHE *Cache, unsigned long long mem_addr, CACHE *next_Cache){
	unsigned long long tag;
	unsigned int index;
	unsigned int memblock_addr;

	unsigned long long tmp = mem_addr;
	index = (tmp >> Cache->num_blockoffset_bits) & ((1 << Cache->num_index_bits) - 1);
	tag = (tmp >> (Cache->num_blockoffset_bits+Cache->num_index_bits));

	/*if(Cache->cache_level == 1)
		sim.L1_writes++;
	else if(Cache->cache_level == 2)
		sim.L2_writes++;*/
	if(Cache->cache_level == 2)
		sim.L2_writes++;
	int hit = 0;
	int hitline = 0;
	int i;
	for(i = 0; i < Cache->assoc; i++){
		if(replacement_policy == LRU){
			if(Cache->sets[index].blocks[i].valid_bit == 1)
				Cache->sets[index].blocks[i].lru_counter++;
			if(Cache->sets[index].blocks[i].valid_bit == 1 && Cache->sets[index].blocks[i].tag == tag){
				Cache->sets[index].blocks[i].lru_counter = 0;
				hit = 1;
				hitline = i;
			}
		}
		else if(replacement_policy == FIFO){
			if(Cache->sets[index].blocks[i].valid_bit == 1)
				Cache->sets[index].blocks[i].fifo_counter++;
			if(Cache->sets[index].blocks[i].valid_bit == 1 && Cache->sets[index].blocks[i].tag == tag){
				hit = 1;
				hitline = i;
			}
		}
	}

	if(hit == 1){
		if(write_policy == WRITE_BACK)
			Cache->sets[index].blocks[hitline].dirty_bit = 1;
		//else if(write_policy == WRITE_THROUGH && next_Cache)
			//handle_write_request(next_Cache, mem_addr, NULL);
		invalidate(&L1_cache,mem_addr);

		return true;
	}
	//hit == 0
	//find one to fill -> i
	/*if(Cache->cache_level == 1)
		sim.L1_write_misses++;
	else if(Cache->cache_level == 2)
		sim.L2_write_misses++;*/
	if(Cache->cache_level == 2)
		sim.L2_write_misses++;
	int maxlru = 0;
	int maxnum = 0;
	int maxfifo = 0;
	for(i = 0; i < Cache->assoc; i++){
		if(Cache->sets[index].blocks[i].valid_bit == 0)
			break;
		if(replacement_policy == LRU){
			if(Cache->sets[index].blocks[i].lru_counter > maxlru){
				maxlru = Cache->sets[index].blocks[i].lru_counter;
				maxnum = i;
			}
		}
		else if(replacement_policy == FIFO){
			if(Cache->sets[index].blocks[i].fifo_counter > maxfifo){
				maxlru = Cache->sets[index].blocks[i].fifo_counter;
				maxnum = i;
			}
		}
	}
	if(i == Cache->assoc)
		i = maxnum; //find i to be replaced

	/*if(next_Cache){
		if(Cache->sets[index].blocks[i].dirty_bit == 1)
			handle_write_request(next_Cache,Cache->sets[index].blocks[i].addr,NULL);
		handle_write_request(next_Cache, mem_addr, NULL);
	}*/
	if(write_allocate_policy == WRITE_ALLOCATE)
	{
		Cache->sets[index].blocks[i].valid_bit = 1;
		Cache->sets[index].blocks[i].tag = tag;
		Cache->sets[index].blocks[i].lru_counter = 0;
		Cache->sets[index].blocks[i].fifo_counter = 0;
		Cache->sets[index].blocks[i].addr = mem_addr;
	}
	return true;
}

bool validate_params(char *params[]){

	L1_size = atoi(params[1]);
	L1_blocksize = atoi(params[2]);
	L1_assoc = atoi(params[3]);
	L2_size = atoi(params[4]);
	L2_blocksize = atoi(params[5]);
	L2_assoc = atoi(params[6]);
	if(!strcmp(params[7],"f"))
		replacement_policy = FIFO;
	else if(!strcmp(params[7],"l"))
		replacement_policy = LRU;

	if(!strcmp(params[8],"wb"))
		write_policy = WRITE_BACK;
	else if(!strcmp(params[8],"wt"))
		write_policy = WRITE_THROUGH;

	if(!strcmp(params[9],"a"))
		write_allocate_policy = WRITE_ALLOCATE;
	else if(!strcmp(params[9],"n"))
		write_allocate_policy = NO_WRITE_ALLOCATE;

	strcpy(trace_file,params[10]);
	fp_trace = fopen(trace_file, "r");
	if (fp_trace == NULL) {
		printf("Error while opening the trace file!\n");
		return false;
	}
	return true;
}

void print_params()
{
	printf("===== Simulator configuration =====\n");

	printf("L1_SIZE:               %d\n", L1_size);
	printf("L1_BLOCKSIZE:          %d\n", L1_blocksize);
	printf("L1_ASSOC:              %d\n", L1_assoc);
	printf("L2_SIZE:               %d\n", L2_size);
	printf("L2_BLOCKSIZE:          %d\n", L2_blocksize);
	printf("L2_ASSOC:              %d\n", L2_assoc);
	printf("REPLACEMENT_POLICY     %d\n", replacement_policy);
	printf("WRITE_POLICY           %d\n", write_policy);
	printf("WRITE_ALLOCATE_POLICY  %d\n", write_allocate_policy);
	printf("trace file:            %s\n", trace_file);
}

void initialize_cache_sim_params(SIM *sim_res)
{
	sim_res->L1_reads = 0;
	sim_res->L1_read_misses	= 0;
	sim_res->L1_writes = 0;
	sim_res->L1_write_misses = 0;
	sim_res->L1_miss_rate = 0;

	sim_res->L2_reads = 0;
	sim_res->L2_read_misses = 0;

	sim_res->L2_writes = 0;
	sim_res->L2_write_misses = 0;
}

void calculate_and_print_cache_sim_params(SIM *sim_res)
{

	sim_res->L1_miss_rate = (float)(sim_res->L1_read_misses + sim_res->L1_write_misses) / (float)(sim_res->L1_reads + sim_res->L1_writes);

	if (L2_size)
		sim_res->L2_miss_rate = (float)(sim_res->L2_read_misses) / (float) (sim_res->L2_reads);

	printf("===== Simulation results =====\n");
	printf("a. number of L1 reads:                 %d\n", sim_res->L1_reads);
	printf("b. number of L1 read misses:           %d\n", sim_res->L1_read_misses);
	printf("c. number of L1 writes:                %d\n", sim_res->L1_writes);
	printf("d. number of L1 write misses:          %d\n", sim_res->L1_write_misses);
	printf("e. L1 miss rate:                       %f\n", sim_res->L1_miss_rate);

	printf("h. number of L2 reads:                 %d\n", sim_res->L2_reads);
	printf("i. number of L2 read misses:           %d\n", sim_res->L2_read_misses);
	printf("l. number of L2 writes:                %d\n", sim_res->L2_writes);
	printf("m. number of L2 write misses:          %d\n", sim_res->L2_write_misses);
	if (L2_size)
		printf("n. L2 miss rate:                       %f\n", sim_res->L2_miss_rate);
	else
		printf("n. L2 miss rate:                       0\n");
}

int main(int argc,char *argv[])
{
	BLOCK retblock;
	unsigned int opnum = 0;
	if(argc != 11){
		cout << argc << endl;
		printf("Usage:\nSimulator <L1_SIZE> <L1_BLOCKSIZE> <L1_ASSOC> <L2_SIZE> <L2_BLOCKSIZE> <L2_ASSOC>");
		printf("<replacement_policy f/l> <write_policy wb/wt> <write_allocate_policy a/n> <tracefile>\n");
		exit(1);
	}
	if (!validate_params(argv)) {
		printf("Exiting.....\n");
		exit(1);
	}
	print_params();
	L1_cache.cache_level = 1;
	set_cache_params(&L1_cache);
	allocate_cache(&L1_cache);
	L2_cache.cache_level = 2;
	if(L2_size){
		set_cache_params(&L2_cache);
		allocate_cache(&L2_cache);
	}
	initialize_cache_sim_params(&sim);
	//printf("===== initialization completes =====\n");
	//int ncount = 0;
	while (fgets(trace_str, 30, fp_trace)) {
		sscanf(trace_str, "%c %x\n", &mem_op.opcode, &mem_op.addr);	
		//printf("%d\n",ncount++);

		opnum += 1;
		if (mem_op.opcode == '0') {
			if (L2_size) {
				handle_read_request(&L1_cache,mem_op.addr, &L2_cache);
			} else {
				handle_read_request(&L1_cache,mem_op.addr, NULL);
			}
		} else if (mem_op.opcode == '1') {
			/*if (L2_size) {
				handle_write_request(&L1_cache, mem_op.addr, &L2_cache);
			} else {
				handle_write_request(&L1_cache, mem_op.addr, NULL);
			}*/
			if(L2_size){
				handle_write_request(&L2_cache, mem_op.addr, NULL);
			}
			else{
				printf("lack of L2_size\n");
				return 1;
			}
		}
	}
	//printf("===== Simulation  =====\n");
	calculate_and_print_cache_sim_params(&sim);
	return 0;

}