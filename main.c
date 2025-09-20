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
    char *last_scheduler_command;
    struct input last_command;
    struct task *run_queue;
    struct hash *run_queue_task_map;
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

void node_delete(long long pid, char is_exit)
{
    struct task *bubbled_node = NULL;
    struct task *map_ptr = map_lookup(&scheduler.run_queue_task_map, pid);

    if (!map_ptr) {
        fprintf(stderr, "node_delete: pid %lld not present in run_map\n", pid);
        return;
    }

    struct task *saved_copy = NULL;
    if (!is_exit) {
        saved_copy = malloc(sizeof *saved_copy);
        if (!saved_copy) {
            fprintf(stderr, "oom\n");
            return;
        }
        *saved_copy = *map_ptr;   
        saved_copy->left = saved_copy->right = NULL;
        saved_copy->height = 1;
    }

    scheduler.run_queue = avl_delete(scheduler.run_queue, &bubbled_node, pid, map_ptr->vmruntime);
    if (!bubbled_node) {
        fprintf(stderr, "AVL delete failed for pid=%lld\n", pid);
        free(saved_copy);
        return;
    }

    /* Two cases: physical pointer removed equals map pointer, or it doesn't. */
    if (bubbled_node == map_ptr) {
        /* straightforward: the struct referenced by map was removed */
        map_delete(&scheduler.run_queue_task_map, pid);

        if (is_exit) {
            printf("Exiting PID=%lld, found in runqueue=%p, in sleep map=%p", pid, scheduler.run_queue, scheduler.wake_queue_task_map);
            free(bubbled_node); // fully exit
        } else {
            
            printf("Moving PID=%lld to sleep map", pid);
            bubbled_node->left = bubbled_node->right = NULL;
            bubbled_node->height = 1;
            map_insert(&scheduler.wake_queue_task_map, pid, bubbled_node);
            free(saved_copy); /* saved_copy not needed */
        }
    } else {
        /* two-child case: the tree overwrote the map_ptr contents with successor data,
           and bubbled_node is the physical node removed (successor). */

        long long successor_pid = bubbled_node->pid;

        map_delete(&scheduler.run_queue_task_map, successor_pid);
        map_insert(&scheduler.run_queue_task_map, successor_pid, map_ptr);

        map_delete(&scheduler.run_queue_task_map, pid);

        if (is_exit) {
            printf("Exiting PID=%lld, found in runqueue=%p, in sleep map=%p", pid, scheduler.run_queue, scheduler.wake_queue_task_map);
            free(bubbled_node);
        } else {
            printf("Moving PID=%lld to sleep map", pid);
            map_insert(&scheduler.wake_queue_task_map, pid, saved_copy);
            free(bubbled_node);
        }
    }
}

void new_task_event(long long pid, long long vmruntime) {
    struct task *t = malloc(sizeof(struct task));
    t->pid = pid;
    t->vmruntime = get_init_vmruntime();
    t->remaining_time = vmruntime;
    t->height = 1;
    t->left = t->right = NULL;

    map_insert(&scheduler.run_queue_task_map, pid, t);
    scheduler.run_queue = avl_insert(scheduler.run_queue, t);
    scheduler.number_of_tasks++;

    printf("[TIME %zu] PID=%lld STARTED (runtime=%lld)\n",
           scheduler.sim_time, pid, vmruntime);
}

void sleep_task_event(long long pid) {
    struct task *n = map_lookup(&scheduler.run_queue_task_map, pid);
    if (!n) {
        fprintf(stderr, "SLEEP: PID %lld not found in runqueue\n", pid);
        return;
    }

    node_delete(pid, 0); // moves to wake map
    printf("[TIME %zu] PID=%lld went to SLEEP (remaining=%lld)\n",
           scheduler.sim_time, pid, n->remaining_time);
}

void wakeup_task_event(long long pid) {
    struct task *wake_node = map_lookup(&scheduler.wake_queue_task_map, pid);
    if (!wake_node) {
        fprintf(stderr, "WAKEUP: PID %lld not found in sleep map\n", pid);
        return;
    }

    map_delete(&scheduler.wake_queue_task_map, pid);
    scheduler.run_queue = avl_insert(scheduler.run_queue, wake_node);
    map_insert(&scheduler.run_queue_task_map, wake_node->pid, wake_node);

    printf("[TIME %zu] PID=%lld WOKE UP (vruntime=%lld, remaining=%lld)\n",
           scheduler.sim_time, wake_node->pid, wake_node->vmruntime, wake_node->remaining_time);
}

void exit_task_event(long long pid) {
    struct task *n = map_lookup(&scheduler.run_queue_task_map, pid);
    if (n) {
        node_delete(pid, 1);
        scheduler.number_of_tasks--;
        printf("[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, pid);
        return;
    }

    n = map_lookup(&scheduler.wake_queue_task_map, pid);
    if (n) {
        map_delete(&scheduler.wake_queue_task_map, n->pid);
        free(n);
        scheduler.number_of_tasks--;
        printf("[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, pid);
        return;
    }

    fprintf(stderr, "EXIT: Unknown PID %lld\n", pid);
}

void process_next_event(void) {
    if (scheduler.last_command.time <= scheduler.sim_time && !scheduler.event_complete) {
        if (strcmp(scheduler.last_command.action, start_task_str) == 0) {
            new_task_event(scheduler.last_command.pid, scheduler.last_command.runtime);
        } else if (strcmp(scheduler.last_command.action, sleep_task_str) == 0) {
            sleep_task_event(scheduler.last_command.pid);
        } else if (strcmp(scheduler.last_command.action, wakeup_task_str) == 0) {
            wakeup_task_event(scheduler.last_command.pid);
        } else if (strcmp(scheduler.last_command.action, exit_task_str) == 0) {
            exit_task_event(scheduler.last_command.pid);
        } else {
            fprintf(stderr, "Unknown action: %s\n", scheduler.last_command.action);
        }

        int eof = fscanf(scheduler.fin, "%lld %127s %lld %lld %lld %lld",
                         &scheduler.last_command.time,
                         scheduler.last_command.action,
                         &scheduler.last_command.pid,
                         &scheduler.last_command.runtime,
                         &scheduler.last_command.weight,
                         &scheduler.last_command.duration);

        if (eof != 6) scheduler.event_complete = 1;
    }
    else if (scheduler.last_command.time > scheduler.sim_time && !scheduler.run_queue) {
        // only fast-forward if no runnable tasks
        scheduler.sim_time = scheduler.last_command.time;
    }
}

static void reschedule_task(struct task *t, size_t slice) {
    struct task *bubbled_node = NULL;

    // Remove from runqueue + hashmap
    map_delete(&scheduler.run_queue_task_map, t->pid);
    scheduler.run_queue = avl_delete(scheduler.run_queue, &bubbled_node, t->pid, t->vmruntime);
    if (!bubbled_node) {
        fprintf(stderr, "reschedule_task: AVL delete failed for pid=%lld\n", t->pid);
        return;
    }

    // Update times
    scheduler.sim_time += slice;
    t->vmruntime += slice;
    t->remaining_time -= slice;

    printf("[TIME %zu] PID=%lld ran for %zu ms → new vruntime=%lld, remaining=%lld\n",
           scheduler.sim_time, t->pid, slice, t->vmruntime, t->remaining_time);

    // Reinsert if still alive
    if (t->remaining_time > 0) {
        scheduler.run_queue = avl_insert(scheduler.run_queue, t);
        map_insert(&scheduler.run_queue_task_map, t->pid, t);
        assert(map_lookup(&scheduler.run_queue_task_map, t->pid) == t);
    } else {
        printf("[TIME %zu] PID=%lld EXITED\n", scheduler.sim_time, t->pid);
        free(t);
        scheduler.number_of_tasks--;
    }
}

int main() 
{
    scheduler.fin = fopen("scheduler_input.txt", "r");

    if (!scheduler.fin) 
    {
        fprintf(stderr, "cant open file\n");
        return -1;
    }

    map_init(&scheduler.run_queue_task_map, 11, NULL); 
    map_init(&scheduler.wake_queue_task_map, 11, NULL);

    if (fscanf(scheduler.fin, "%lld %127s %lld %lld %lld %lld",
            &scheduler.last_command.time,
            scheduler.last_command.action,
            &scheduler.last_command.pid,
            &scheduler.last_command.runtime,
            &scheduler.last_command.weight,
            &scheduler.last_command.duration) == EOF) 
    {
        return 0;
    }

    while (!scheduler.event_complete || scheduler.run_queue) 
    {
        if (!scheduler.event_complete) 
        {
            process_next_event();
        }

        if (scheduler.run_queue && scheduler.number_of_tasks > 0) 
        {
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

    free_map(scheduler.run_queue_task_map);
    free_map(scheduler.wake_queue_task_map);

    return 0;
}
