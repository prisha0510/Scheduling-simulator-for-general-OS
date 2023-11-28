#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 1<<16
#define calculation_precision 1e1

typedef struct {
    char pid[10];
    int generated_time;
    int job_length;
    int remaining_time;
    int scheduled_time;
    int completion_time;
    int turnaround_time;
    int response_time;
    int remaining_timeslice;
} Process;

Process* in_cpu;
int curr_queue;
char in_cpu_prev[10];
Process* in_queue1;

// Priority Queue implementation

struct Node {
    Process* data;
    struct Node* next;
};

typedef struct {
    Process **data;
    int capacity;
    int size;
    bool (*comparator)(Process*, Process*);
    bool priority;
    struct Node* front;
    struct Node* rear;
    Process* top;
} 
PriorityQueue;

struct Node* createNode(Process* process) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (newNode == NULL) {
        exit(1);
    }
    newNode->data = process;
    newNode->next = NULL;
    return newNode;
}

PriorityQueue* createPriorityQueue(bool (*cmp)(Process*, Process*)) {
    PriorityQueue *pq = (PriorityQueue *)malloc(sizeof(PriorityQueue));
    pq->capacity = MAX_PROCESSES;
    pq->size = 0;
    pq->data = (Process **)malloc(pq->capacity * sizeof(Process *));
    pq->comparator = cmp;
    if(cmp==NULL)
        pq->priority = false;
    else
        pq->priority = true;
    return pq;
}

void swap(Process **a, Process **b) {
    Process *temp = *a;
    *a = *b;
    *b = temp;
}

void heapifyUp(PriorityQueue *pq, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if(pq->comparator(pq->data[parent], pq->data[index])) {
            swap(&(pq->data[parent]), &(pq->data[index]));
            index = parent;
        } 
        else
            break;
    }
}

void heapifyDown(PriorityQueue *pq, int index) {
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    int smallest = index;

    if (left < pq->size && !pq->comparator(pq->data[left], pq->data[smallest]))
        smallest = left;

    if (right < pq->size && !pq->comparator(pq->data[right], pq->data[smallest]))
        smallest = right;

    if (smallest != index) {
        swap(&(pq->data[index]), &(pq->data[smallest]));
        heapifyDown(pq, smallest);
    }
}

void insert(PriorityQueue *pq, Process *process) {
    if(pq->priority==true){
        if (pq->size >= pq->capacity) {
            pq->capacity *= 2;
            pq->data = (Process **)realloc(pq->data, pq->capacity * sizeof(Process *));
        }
        pq->data[pq->size] = process;
        pq->size++;
        heapifyUp(pq, pq->size - 1);
    }
    else{
        struct Node* newNode = createNode(process);
        pq->size++;

        if (pq->rear == NULL) {
            pq->front = pq->rear = newNode;
            pq->top = newNode->data;
            return;
        }
        pq->rear->next = newNode;
        pq->rear = newNode;
    }
}

Process* top(PriorityQueue *pq) {
    if(pq->priority==true){
        if(pq->size > 0)
            return pq->data[0];
        else 
            return NULL;
    }
    else
        return pq->top;  
}

void pop(PriorityQueue *pq) {
    if(pq->priority==true){
        if (pq->size > 0) {
            swap(&(pq->data[0]), &(pq->data[pq->size - 1]));
            pq->size--;
            heapifyDown(pq, 0);
        }
    }
    else{
        if (pq->front == NULL) {
            return;
        }
        struct Node* temp = pq->front;
        pq->front = pq->front->next;
        pq->size--;
        if (pq->front == NULL) {
            pq->rear = NULL;
            pq->top = NULL;
        }
        else
            pq->top = pq->front->data;
        free(temp);
    } 
}

PriorityQueue *buffer_pq;

// Helper functions

double exponential_random(double mean) {
    return -mean * log(1.0 - ((double) rand() / RAND_MAX));
}

int sample_normal_distribution(double mean, double stddev) {
    return (int)(mean + stddev * sqrt(-2 * log((double) rand() / RAND_MAX)) * cos(2 * M_PI * ((double) rand() / RAND_MAX)));
}

int max(int a, int b){
    if(a>b)
        return a;
    return b;
}

// Will work only when the buffer_pq contains processes, will be empty otherwise
void copy_from_buffer(PriorityQueue* pq){
    while(buffer_pq->size){
        Process* proc = top(buffer_pq);
        pop(buffer_pq);
        insert(pq, proc);
    }
    in_cpu = NULL;
    curr_queue = 0;
}

// Book-keeping for starting a process
void start_process(Process* proc, int t){
    proc->scheduled_time = t;
    proc->response_time = proc->scheduled_time - proc->generated_time;
}

void update_process(Process* proc, int t, int pq_num){
    (proc->remaining_time)--;
    (proc->remaining_timeslice)--;
    if(pq_num == 1)
        in_queue1 = proc;
}

// Book-keeping for ending a process
void complete_process(Process* proc, int t){
    proc->completion_time = t+1;
    proc->turnaround_time = proc->completion_time - proc->generated_time;
    proc->remaining_timeslice = 0;
}

// Comparators

// here comparator = generated_time
bool cmp_arrival_time(Process* p1, Process* p2){
    if (p1->generated_time > p2->generated_time)
        return true;
    else if(p1->generated_time == p2->generated_time && p1->pid > p2->pid)
        return true;
    return false;
}

// here comparator = job length
bool cmp_job_length(Process* p1, Process* p2){
    if (p1->job_length > p2->job_length)
        return true;
    else if(p1->job_length == p2->job_length && p1->generated_time > p2->generated_time)
        return true;
    else if(p1->job_length == p2->job_length && p1->generated_time == p2->generated_time && p1->pid > p2->pid)
        return true;
    return false;
}

// here comparator = remaining_time
bool cmp_remaining_time(Process* p1, Process* p2){
    if (p1->remaining_time > p2->remaining_time)
        return true;
    else if(p1->remaining_time == p2->remaining_time && p1->pid > p2->pid)
        return true;
    return false;
}

void print_details(Process *processes, int n) {
    double avg_response_time = 0;
    double avg_turnaround_time = 0;

    for (int i = 0; i < n; i++) {
        avg_response_time += processes[i].response_time/calculation_precision;
        avg_turnaround_time += processes[i].turnaround_time/calculation_precision;
    }
    avg_response_time /= n;
    avg_turnaround_time /= n;
    printf("\n%.3f %.3f\n", avg_turnaround_time, avg_response_time);
}

void add_new_processes(PriorityQueue *pq, Process *processes, int n, int time, int last_arrival_time){
    if(time<=last_arrival_time)
        for(int i = 0;i<n;i++)
            if(processes[i].generated_time==time){
                if(in_cpu==NULL)
                    insert(pq, &processes[i]);
                else
                    insert(buffer_pq, &processes[i]);
            }
}

bool schedule_process(PriorityQueue *pq1, PriorityQueue *pq_current, PriorityQueue *pq_next , int timeslice, int t, int pq_num, bool interfere, bool last_queue){
    Process *current_process;
    if(in_cpu!=NULL)
        current_process = in_cpu; 
    else
        current_process = top(pq_current);

    if(current_process!=NULL){
        if(strcmp(in_cpu_prev, "-1")==0)
            printf("%s %.3f ", current_process->pid, t/calculation_precision);
        else if(strcmp(in_cpu_prev, current_process->pid)!=0)
            printf("%.3f %s %.3f ", t/calculation_precision, current_process->pid, t/calculation_precision);
    }
    else if(current_process==NULL && strcmp(in_cpu_prev, "-1")!=0 && last_queue)
        printf("%.3f ", (t)/calculation_precision);

    if(current_process==NULL && last_queue)
        strcpy(in_cpu_prev, "-1");
    else if(current_process!=NULL)
        strcpy(in_cpu_prev, current_process->pid);

    if(current_process!=NULL){
        if(current_process->scheduled_time == -1)
            start_process(current_process, t);

        update_process(current_process, t, pq_num);
        
        if(current_process->remaining_time == 0){
            complete_process(current_process, t);
            pop(pq_current);
            copy_from_buffer(pq1);
        }
        else if(timeslice!=-1 && current_process->remaining_timeslice==0){
            current_process->remaining_timeslice = timeslice;
            insert(pq_next, current_process);
            pop(pq_current);
            copy_from_buffer(pq1);
        }
        else if(interfere==false){
            in_cpu = current_process;
            curr_queue = pq_num;
        }
            
        return true;
    }
    else
        return false;
}

void boost(PriorityQueue *pq1, PriorityQueue *pq2, PriorityQueue *pq3, int tsmlfq1, int t){
    while(pq2->size){
        Process* current_process = top(pq2);
        pop(pq2);
        if(in_cpu!=NULL){
            if(strcmp(current_process->pid, in_cpu->pid)!=0){
                current_process->remaining_timeslice = tsmlfq1;
                insert(pq1,current_process);
            }
        }
        else{
            current_process->remaining_timeslice = tsmlfq1;
            insert(pq1,current_process);
        }
    }
    while(pq3->size){
        Process* current_process = top(pq3);
        pop(pq3);
        if(in_cpu!=NULL){
            if(strcmp(current_process->pid, in_cpu->pid)!=0){
                current_process->remaining_timeslice = tsmlfq1;
                insert(pq1,current_process);
            }      
        }
        else{
            current_process->remaining_timeslice = tsmlfq1;
            insert(pq1,current_process);
        }
            
    }
    if(in_cpu!=NULL){
        if(in_queue1!=NULL && in_cpu->pid==in_queue1->pid)
            pop(pq1);

        PriorityQueue *pq1_copy = createPriorityQueue(NULL);
        while(pq1->size){
            Process* proc = top(pq1);
            pop(pq1);
            insert(pq1_copy, proc);
        }
        insert(pq1, in_cpu);
        while(pq1_copy->size){
            Process* proc = top(pq1_copy);
            pop(pq1_copy);
            insert(pq1, proc);
        }
        in_cpu = NULL;
        curr_queue = 0;
    }
}

void scheduler(Process *processes_original, int n, bool (*cmp)(Process*, Process*), int timeslice, bool interfere){
    PriorityQueue *pq = createPriorityQueue(cmp);
    buffer_pq = createPriorityQueue(cmp);
    int last_arrival_time = INT_MIN; 
    Process processes[n];

    for(int i=0;i<n;i++){
        last_arrival_time = max(last_arrival_time, processes_original[i].generated_time);
        processes[i] = processes_original[i];
    }  

    int time = 0;
    for(time = 0; time<=last_arrival_time || pq->size>0; time++){
        add_new_processes(pq, processes, n, time, last_arrival_time);
        schedule_process(pq, pq, pq, timeslice, time, -1, interfere, true);                 
    }
    printf("%.3f ", (time)/calculation_precision);   
    print_details(processes, n);
}

void scheduler_mlfq(Process *processes_original, int n, int tsmlfq1, int tsmlfq2, int tsmlfq3, int bmlfq){
    PriorityQueue *pq1 = createPriorityQueue(NULL);
    PriorityQueue *pq2 = createPriorityQueue(NULL);
    PriorityQueue *pq3 = createPriorityQueue(NULL);
    buffer_pq = createPriorityQueue(NULL);
    int last_arrival_time = INT_MIN;

    Process processes[n];

    for(int i=0;i<n;i++){
        last_arrival_time = max(last_arrival_time, processes_original[i].generated_time);
        processes[i] = processes_original[i];
        processes[i].remaining_timeslice = tsmlfq1;
    }

    int time = 0;
    bool scheduled = false;
    for(time = 0; time<=last_arrival_time || pq1->size || pq2->size || pq3->size; time++){
        add_new_processes(pq1, processes, n, time, last_arrival_time);
        if(time % bmlfq == 0)
            boost(pq1, pq2, pq3, tsmlfq1, time);
        
        if(in_cpu!=NULL){
            if(curr_queue==1)
                schedule_process(pq1, pq1, pq2, tsmlfq2, time, 1, false, false);
            else if(curr_queue==2)
                schedule_process(pq1, pq2 ,pq3, tsmlfq3, time, 2, false, false);
            else if(curr_queue==3)
                schedule_process(pq1, pq3, pq3, tsmlfq3, time, 3, false, true);    
        }   
        else{
            scheduled = schedule_process(pq1, pq1, pq2, tsmlfq2, time, 1, false, false);
            if(!scheduled)
                scheduled = schedule_process(pq1, pq2 ,pq3, tsmlfq3, time, 2, false, false);
            if(!scheduled)
                schedule_process(pq1, pq3, pq3, tsmlfq3, time, 3, false, true); 
        }          
    }
    printf("%.3f ", (time)/calculation_precision);
    print_details(processes, n);
}

void first_come_first_serve(Process *processes, int n){
    in_cpu = NULL;
    strcpy(in_cpu_prev, "-1");
    curr_queue = 0;
    scheduler(processes, n, cmp_arrival_time, -1, false);
}

void shortest_job_first(Process *processes, int n){
    in_cpu = NULL;
    strcpy(in_cpu_prev, "-1");
    curr_queue = 0;
    scheduler(processes, n, cmp_job_length, -1, false);
}

void shortest_remaining_time_first(Process *processes, int n){
    in_cpu = NULL;
    strcpy(in_cpu_prev, "-1");
    curr_queue = 0;
    scheduler(processes, n, cmp_remaining_time, -1, true);
}

void round_robin(Process *processes, int n, int timeslice){
    in_cpu = NULL;
    strcpy(in_cpu_prev, "-1");
    curr_queue = 0;
    scheduler(processes, n, NULL, timeslice, true);
}

void mlfq(Process *processes, int n, int tsmlfq1, int tsmlfq2, int tsmlfq3, int bmlfq){
    in_cpu = NULL;
    strcpy(in_cpu_prev, "-1");
    curr_queue = 0;
    scheduler_mlfq(processes, n, tsmlfq1, tsmlfq2, tsmlfq3, bmlfq);
}

int input(FILE* file, Process* processes, int timeslice){
    char temp;
    char curr[20];
    char last_temp;
    int mod2counter = 0;
    int process_num = 0;

    while((temp = fgetc(file))!=EOF){
        last_temp = temp;
        if(temp == ' '){
            if(mod2counter==0){
                strcpy(processes[process_num].pid, curr);
                mod2counter = 1 - mod2counter;
            }
            else{
                processes[process_num].generated_time = calculation_precision*atof(curr);
                mod2counter = 1 - mod2counter;
            }
            memset(curr, '\0', sizeof(curr));
        }
        else if(temp == '\n'){
            processes[process_num].job_length = calculation_precision*atof(curr);
            processes[process_num].remaining_time = processes[process_num].job_length;
            processes[process_num].scheduled_time = -1;
            processes[process_num++].remaining_timeslice = timeslice;
            memset(curr, '\0', sizeof(curr));
        }
        else{
            int length = strlen(curr);
            curr[length] = temp;
        }
    }
    if(last_temp!='\n'){
        processes[process_num].job_length = calculation_precision*atof(curr);
        processes[process_num].remaining_time = processes[process_num].job_length;
        processes[process_num].scheduled_time = -1;
        processes[process_num++].remaining_timeslice = timeslice;
    }
    return process_num;
}

void solve(FILE* file, int timeslice, int tsmlfq1, int tsmlfq2, int tsmlfq3, int bmlfq){
    Process processes[MAX_PROCESSES];
    int process_num = input(file, processes, timeslice);
    first_come_first_serve(processes, process_num);

    round_robin(processes, process_num, timeslice);

    shortest_job_first(processes, process_num);

    shortest_remaining_time_first(processes, process_num);

    mlfq(processes, process_num, tsmlfq1, tsmlfq2, tsmlfq3, bmlfq);
}

int main(int argc, char *argv[]) {
    in_cpu = NULL;
	FILE* input_file;
    FILE* output_file;
    int timeslice;
    int tsmlfq1, tsmlfq2, tsmlfq3, bmlfq;
	if(argc>1){
        input_file = freopen(argv[1], "r", stdin);
        output_file = freopen(argv[2],"w", stdout);
        timeslice = calculation_precision*atoi(argv[3]);
        tsmlfq1 = calculation_precision*atoi(argv[4]);
        tsmlfq2 = calculation_precision*atoi(argv[5]);
        tsmlfq3 = calculation_precision*atoi(argv[6]);
        bmlfq = calculation_precision*atoi(argv[7]);
        solve(input_file, timeslice, tsmlfq1, tsmlfq2, tsmlfq3, bmlfq);
        fclose(input_file);
        fclose(output_file);
    }
}
