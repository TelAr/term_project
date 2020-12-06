//this code is working on linux 5.4.67 ver, if version is not same, it can be caused problem
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
#define PARA_NUM 1000000

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

unsigned long long imp_time_avr[11];
unsigned long long nor_time_avr[11];

//setting what we want to find
//improve case, PARA_NUM/2 is worst case, normal case, near to 1 is worst case
int search_target;

struct mutex counter_lock;
struct mutex statement_lock;

struct mutex f_lock;
struct mutex b_lock;

struct task_struct *f_search=NULL;
struct task_struct *b_search=NULL;

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
//			printk("%d times insert\nrunning time:%llu\n", insert_count, add_to_list_count);
			mutex_unlock(&statement_lock);
		}
		
		mutex_unlock(&counter_lock);
	}
	do_exit(0);
	return 0; 
}



int insert_thread_create(void) {

	int i;
	
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);
	
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
		
		if(thread_search_over){//if one thread is over, then this boolean flag change True and run break 
		
			break;
		}

		if (current_node->data == search_target&&!thread_search_over) {//if search is completed
			thread_search_over=true;
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			mutex_unlock(&statement_lock);//unlock mutex about main (parent) thread
			break;
		}
		
	}	
	mutex_unlock(&f_lock);
	do_exit(0);

	return 0;
}

static int search_back(void * _arg) {

	struct my_node *current_node;

	list_for_each_entry_reverse(current_node, &my_list, list){//this api is travel back way
		

		
		if(thread_search_over){//if one thread is over, then this boolean flag change True and run break

			break;
		}
		
		if (current_node->data == search_target&&!thread_search_over) {//if search is completed
			thread_search_over=true;
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			mutex_unlock(&statement_lock);//unlock mutex about main (parent) thread
			break;
		}
	}	
	mutex_unlock(&b_lock);
	do_exit(0);

	return 0;
}


int search_thread_create(void) {

	
	thread_search_over=false;
	
	mutex_lock(&statement_lock);
	mutex_lock(&f_lock);
	mutex_lock(&b_lock);
	
	getnstimeofday(&spclock[0]);
	
	//we run two thread, one is search front way, and the other is search back way
	kthread_run(search_front,NULL,"search_front");
	kthread_run(search_back,NULL,"search_back");

	mutex_lock(&statement_lock);//lock mutex before search is over
	printk("i:%llu\n", add_to_list_count);
	
	mutex_lock(&f_lock);
	mutex_lock(&b_lock);
	
	mutex_unlock(&f_lock);
	mutex_unlock(&b_lock);
	mutex_unlock(&statement_lock);	
		
	return 0;
}


static int search_not_improve(void) {

	struct my_node *current_node;
	
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);
	
	list_for_each_entry(current_node, &my_list, list){

		if (current_node->data == search_target) {//if search is completed
			thread_search_over=true;
			getnstimeofday(&spclock[1]);
			add_to_list_count=calclock3(spclock);
			printk("n:%llu\n", add_to_list_count);
			break;
		}
		
	}	
	mutex_unlock(&statement_lock);//unlock mutex about main (parent) thread
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
//		printk("%d times delete\nrunning time:%llu\n", PARA_NUM, add_to_list_count);
		thread_over=true;
		mutex_unlock(&statement_lock);
	}

	do_exit(0);
	return 0;
}

int delete_thread_create(void) {


	int i;
	
	mutex_lock(&statement_lock);
	getnstimeofday(&spclock[0]);
	
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
	
	int i;
	
	INIT_LIST_HEAD(&my_list);
	mutex_init(&statement_lock);
	mutex_init(&counter_lock);
	mutex_init(&f_lock);
	mutex_init(&b_lock);
	
	for(i=0; i<11; i++)
	{
		imp_time_avr[i]=0;
		nor_time_avr[i]=0;
	}
	
	//insert
	
	insert_thread_create();
	
	//search

	for(i=0; search_target<PARA_NUM; i++)
	{
		search_target=i*100000;
		if(search_target==0) search_target=1;
		
		printk("seaching target:%d\n", search_target);
		
		//search improve ver
		search_thread_create();
	
		//search not improve
		search_not_improve();
	}	
	
	//delete

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
