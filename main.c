#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "avl.h"
#include "map.h"

struct input 
{
    long long pid;
    long long duration;
    long long runtime;
    char action[128];
    long long weight;
    long long time;
};

struct scheduler
{
    size_t min_granularity;
    size_t sched_latency;
    size_t number_of_tasks;
    size_t sim_time;
    FILE *fin;
    int event_complete;
    struct input last_command;
    struct task *run_queue;
    struct hash *wake_queue_task_map;
};

static struct scheduler scheduler = {
    .min_granularity = 4, // ms
    .sched_latency = 20 // ms
    ,
};

char *start_task_str  = "START";
char *sleep_task_str  = "SLEEP";
char *wakeup_task_str = "WAKEUP";
char *exit_task_str = "EXIT";

static inline long long max(long long a, long long b)
{
    return a > b ? a : b;
}
static long long get_init_vmruntime(void)
{
    if (!scheduler.run_queue) 
    {
        return 0;
    }

    struct task *t = avl_find_min(scheduler.run_queue);
    return t ? t->vmruntime : 0;
}

void node_delete(long long pid, char is_exit) {
    struct task *bubbled = NULL;

    struct task *victim = avl_find_by_pid(scheduler.run_queue, pid);
    if (!victim) {
        #ifdef DEBUG
        fprintf(stderr, "node_delete: pid=%lld not found in AVL\n", pid);
        #endif
        return;
    }

    scheduler.run_queue = avl_delete(scheduler.run_queue, &bubbled, victim->pid, victim->vmruntime);
    if (!bubbled) {
        #ifdef DEBUG
        fprintf(stderr, "avl_delete failed for pid=%lld\n", pid);
        #endif
        return;
    }

    struct task *saved_heap = NULL;
    if (!is_exit) {
        saved_heap = malloc(sizeof *saved_heap);
        if (!saved_heap) {
            #ifdef DEBUG
            fprintf(stderr, "node_delete: oom saving payload for pid=%lld\n", pid);
            #endif
            return;
        }
        *saved_heap = *victim;         
        saved_heap->left = saved_heap->right = NULL;
        saved_heap->height = 1;
        saved_heap->pid = pid;         
    }

    if (is_exit) {
        free_wrapper(bubbled, "Node delete, exit");
    } else {
        free_wrapper(bubbled, "Node delete, sleep");
        map_insert(&scheduler.wake_queue_task_map, pid, saved_heap);
        #ifdef DEBUG
        fprintf(stdout, "wake_insert: key=%lld ptr=%p pid=%lld\n", pid, saved_heap, saved_heap->pid);
        #endif
    }
}


void new_task_event(long long pid, long long vmruntime) 
{
    #ifdef DEBUG
    avl_print_tree(scheduler.run_queue);
    map_print_all(scheduler.wake_queue_task_map);
    #endif
    struct task *t = malloc(sizeof(struct task));
    t->pid = pid;
    t->vmruntime = get_init_vmruntime();
    t->remaining_time = vmruntime;
    t->height = 1;
    t->left = t->right = NULL;

    scheduler.run_queue = avl_insert(scheduler.run_queue, t);
    scheduler.number_of_tasks++;

    printf("[TIME %zu] PID=%lld STARTED (runtime=%lld)\n",
           scheduler.sim_time, pid, vmruntime);
}

void sleep_task_event(long long pid) 
{
    #ifdef DEBUG
    avl_print_tree(scheduler.run_queue);
    #endif
    struct task *n = avl_find_by_pid(scheduler.run_queue, pid);
    if (!n) {
        #ifdef DEBUG
        fprintf(stderr, "SLEEP: PID %lld not found in runqueue\n", pid);
        #endif
        return;
    }


    #ifdef DEBUG
    fprintf(stdout, "[TIME %zu] PID=%lld went to SLEEP (remaining=%lld)\n",
           scheduler.sim_time, pid, n->remaining_time);
    #endif
    node_delete(pid, 0); // moves to wake map
    #ifdef DEBUG
    map_print_all(scheduler.wake_queue_task_map);
    #endif
}

void wakeup_task_event(long long pid) {
    #ifdef DEBUG
    avl_print_tree(scheduler.run_queue);
    map_print_all(scheduler.wake_queue_task_map);
    #endif
    struct task *wake_node = map_lookup(&scheduler.wake_queue_task_map, pid);
    if (!wake_node) {
        #ifdef DEBUG
        fprintf(stderr, "WAKEUP: PID %lld not found in sleep map\n", pid);
        #endif
        return;
    }

    assert(wake_node->pid == pid);
    map_delete(&scheduler.wake_queue_task_map, pid);
    scheduler.run_queue = avl_insert(scheduler.run_queue, wake_node);

    
    fprintf(stdout, "[TIME %zu] PID=%lld WOKE UP (vruntime=%lld, remaining=%lld)\n",
           scheduler.sim_time, wake_node->pid, wake_node->vmruntime, wake_node->remaining_time);
}

void exit_task_event(long long pid) {
    avl_print_tree(scheduler.run_queue);
    map_print_all(scheduler.wake_queue_task_map);
    struct task *n = avl_find_by_pid(scheduler.run_queue, pid);
    if (n) {
        node_delete(pid, 1);
        scheduler.number_of_tasks--;
        fprintf(stdout, "[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, pid);
        return;
    }

    n = map_lookup(&scheduler.wake_queue_task_map, pid);
    if (n) {
        map_delete(&scheduler.wake_queue_task_map, n->pid);
        free_wrapper(n, "Node delete, exit");
        scheduler.number_of_tasks--;
        fprintf(stdout, "[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, pid);
        return;
    }

    #ifdef DEBUG
    fprintf(stderr, "EXIT: Unknown PID %lld\n", pid);
    #endif
}

void process_next_event(void) {
    if (scheduler.last_command.time <= scheduler.sim_time && !scheduler.event_complete) {
        fprintf(stdout, "Process event: %lld %s %lld %lld\n", scheduler.last_command.time, 
            scheduler.last_command.action, scheduler.last_command.pid, scheduler.last_command.runtime);

        if (strcmp(scheduler.last_command.action, start_task_str) == 0) {
            new_task_event(scheduler.last_command.pid, scheduler.last_command.runtime);
        } else if (strcmp(scheduler.last_command.action, sleep_task_str) == 0) {
            sleep_task_event(scheduler.last_command.pid);
        } else if (strcmp(scheduler.last_command.action, wakeup_task_str) == 0) {
            wakeup_task_event(scheduler.last_command.pid);
        } else if (strcmp(scheduler.last_command.action, exit_task_str) == 0) {
            exit_task_event(scheduler.last_command.pid);
        } else {
            #ifdef DEBUG
            fprintf(stderr, "Unknown action: %s\n", scheduler.last_command.action);
            #endif
        }

        int eof = fscanf(scheduler.fin, "%lld %127s %lld %lld",
                         &scheduler.last_command.time,
                         scheduler.last_command.action,
                         &scheduler.last_command.pid,
                         &scheduler.last_command.runtime);

        if (eof == EOF) scheduler.event_complete = 1;
    }
    else if (scheduler.last_command.time > scheduler.sim_time && !scheduler.run_queue) {
        // only fast-forward if no runnable tasks
        scheduler.sim_time = scheduler.last_command.time;
    }
}

static void reschedule_task(struct task *t, size_t slice) {
    struct task *bubbled_node = NULL;

    // Remove from runqueue + hashmap
    scheduler.run_queue = avl_delete(scheduler.run_queue, &bubbled_node, t->pid, t->vmruntime);
    if (!bubbled_node) {
        #ifdef DEBUG
        fprintf(stderr, "reschedule_task: AVL delete failed for pid=%lld\n", t->pid);
        #endif
        return;
    }

    // Update times
    scheduler.sim_time += slice;
    t->vmruntime += slice;
    t->remaining_time -= slice;

    fprintf(stdout, "[TIME %zu] PID=%lld ran for %zu ms → new vruntime=%lld, remaining=%lld\n",
           scheduler.sim_time, t->pid, slice, t->vmruntime, t->remaining_time);

    // Reinsert if still alive
    if (t->remaining_time > 0) {
        scheduler.run_queue = avl_insert(scheduler.run_queue, t);
    } else {
        fprintf(stdout, "[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, t->pid);
        free_wrapper(t, "reschedule_task");
        scheduler.number_of_tasks--;
    }
}

int main() 
{
    scheduler.fin = fopen("scheduler_input.txt", "r");

    if (!scheduler.fin) 
    {
        #ifdef DEBUG
        fprintf(stderr, "cant open file\n");
        #endif
        return -1;
    }

    if (map_init(&scheduler.wake_queue_task_map, 11, NULL) < 0) 
    {
        #ifdef DEBUG
        fprintf(stderr, "cant init map\n");
        #endif
        return -1;
    }

    if (fscanf(scheduler.fin, "%lld %127s %lld %lld",
            &scheduler.last_command.time,
            scheduler.last_command.action,
            &scheduler.last_command.pid,
            &scheduler.last_command.runtime) == EOF) 
    {
        #ifdef DEBUG
        fprintf(stderr, "file data format error\n");
        #endif
        return -1;
    }

    while (!scheduler.event_complete || scheduler.run_queue) 
    {
        if (!scheduler.event_complete) 
        {
            process_next_event();
        }

        if (scheduler.run_queue && scheduler.number_of_tasks > 0) 
        {
            #ifdef DEBUG
            avl_print_tree(scheduler.run_queue);
            #endif
            struct task *n = avl_find_min(scheduler.run_queue);
            size_t slice = max(scheduler.min_granularity, scheduler.sched_latency / scheduler.number_of_tasks);

            if (!scheduler.event_complete) 
            {
                long long time_to_next_event = scheduler.last_command.time - scheduler.sim_time;
                if (time_to_next_event < (long long)slice) slice = (size_t)time_to_next_event;
            }

            reschedule_task(n, slice);
        }
    }

    free_map(scheduler.wake_queue_task_map);
    fclose(scheduler.fin);

    return 0;
}
