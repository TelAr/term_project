#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <linux/mutex.h>

#define BILLION 1000000000
#define PARA_NUM 100000

struct timespec spclock[2];
struct list_head my_list;
struct my_node *current_node;
struct list_head *p;

int insert_count=0;
int delete_count=1;

bool thread_over;
bool thread_search_over;

unsigned long long add_to_list_time;
unsigned long long add_to_list_count;

int search_target=PARA_NUM/2;
//int search_target=100;

struct mutex counter_lock;
struct mutex statement_lock;

struct my_node {

	struct list_head list;
	int data;

};


unsigned long long calclock3(struct timespec *spclock) {

	
	long temp, temp_n;
	unsigned long long timedelay=0;
	if(spclock[1].tv_nsec >= spclock[0].tv_nsec) {
	
		temp=spclock[1].tv_sec-spclock[0].tv_sec;
		temp_n=spclock[1].tv_nsec-spclock[0].tv_nsec;
		timedelay=BILLION*temp+temp_n;
	}
	else {
	
		
		temp=spclock[1].tv_sec-spclock[0].tv_sec-1;
		temp_n=BILLION+spclock[1].tv_nsec-spclock[0].tv_nsec;
		timedelay=BILLION*temp+temp_n;
	}
	return timedelay;
}

static int insert(void * _arg) {

	for( ; insert_count<PARA_NUM; ) {
	
		mutex_lock(&counter_lock);
		insert_count++;
		
		if(insert_count>PARA_NUM) {
			mutex_unlock(&counter_lock);
			break;
		}
		struct my_node *new = kmalloc(sizeof(struct my_node), GFP_KERNEL);
		new->data = insert_count;
		list_add(&new->list, &my_list);
//		printk("%d\n", insert_count);
		
		if(insert_count==PARA_NUM){
		
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			printk("%d times insert\nrunning time:%llu\n", insert_count, add_to_list_count);
			mutex_unlock(&statement_lock);
		}
		
		mutex_unlock(&counter_lock);
	}
	do_exit(0);
	return 0; 
}



int insert_thread_create(void) {

	int i;
	for(i=0;i<4;i++)
	{
		int* arg=(int*)kmalloc(sizeof(int), GFP_KERNEL);
		*arg = i;
		kthread_run(insert,(void*)arg,"insert");
	}
	
	return 0;
}

static int search_front(void * _arg) {

	struct my_node *current_node;
	
	list_for_each_entry(current_node, &my_list, list){
		

		
		
		if(thread_search_over){
		
			break;
		}
		
//		printk("%d\n", current_node->data);
		
		mutex_lock(&counter_lock);
		if (current_node->data == search_target&&!thread_search_over) {
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			printk("%d nodes search_f\nrunning time:%llu\n", PARA_NUM, add_to_list_count);
			thread_search_over=true;
			mutex_unlock(&counter_lock);
			mutex_unlock(&statement_lock);

			break;
		}
		
		mutex_unlock(&counter_lock);

	}	

	do_exit(0);

	return 0;
}

static int search_back(void * _arg) {

	struct my_node *current_node;

	list_for_each_entry_reverse(current_node, &my_list, list){
		

		
		if(thread_search_over){

			break;
		}
		
//		printk("%d\n", current_node->data);
		
		mutex_lock(&counter_lock);
		if (current_node->data == search_target&&!thread_search_over) {
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			printk("%d nodes search_b\nrunning time:%llu\n", PARA_NUM, add_to_list_count);
			thread_search_over=true;
			mutex_unlock(&counter_lock);
			mutex_unlock(&statement_lock);
			break;
		}
		mutex_unlock(&counter_lock);

	}	

	do_exit(0);

	return 0;
}


int search_thread_create(void) {

	
	thread_search_over=false;
	
	kthread_run(search_front,NULL,"search_front");
	kthread_run(search_back,NULL,"search_back");

	
	return 0;
}


static int delete(void * _arg) {

	struct my_node *current_node, *next_node;

	while(true){
	
		mutex_lock(&counter_lock);
	
		if(delete_count>=PARA_NUM){	
	
			mutex_unlock(&counter_lock);
			break;
		}
		list_for_each_entry_safe(current_node, next_node, &my_list, list){
		
			
			list_del(&current_node->list);
			kfree(current_node);
			delete_count++;
			break;
			
		}
	mutex_unlock(&counter_lock);
	}	

	if (!thread_over){
	
		thread_over=true;
		getnstimeofday(&spclock[1]);
		add_to_list_count=calclock3(spclock);
		printk("%d times delete\nrunning time:%llu\n", PARA_NUM, add_to_list_count);
		thread_over=true;
		mutex_unlock(&statement_lock);
	}

	do_exit(0);
	return 0;
}

int delete_thread_create(void) {


	int i;
	
	thread_over=false;
	
	for(i=0;i<4;i++)
	{
		int* arg=(int*)kmalloc(sizeof(int), GFP_KERNEL);
		*arg = i;
		kthread_run(delete,(void*)arg,"delete");
	}


	return 0;
}

void test_case(void) {
	
	INIT_LIST_HEAD(&my_list);
	mutex_init(&statement_lock);
	mutex_init(&counter_lock);
	
	//insert
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);
	
	insert_thread_create();
	
	
	//search	
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);

	search_thread_create();
	
	
	//delete
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);

	delete_thread_create();
	
	mutex_lock(&statement_lock);
	mutex_unlock(&statement_lock);

	return;
}

int __init linked_list_init(void) {

	printk("test case about: %d times in searching improved mutex_lock linked_list\n", PARA_NUM);
	test_case();
	return 0;
}

void __exit linked_list_cleanup(void) {

	printk("module cleanup\n");
}

module_init(linked_list_init);
module_exit(linked_list_cleanup);
MODULE_LICENSE("GPL");