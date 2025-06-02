#include "scheduler.h"
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>


// Helper function to calculate metrics
static Metrics calculate_metrics(Process proc[], int n) {
    Metrics metrics = {0, 0, 0};
    float total_turnaround = 0, total_waiting = 0, total_response = 0;
    
    for (int i = 0; i < n; i++) {
        int turnaround = proc[i].completionTime - proc[i].arrivalTime;
        int waiting = turnaround - proc[i].burstTime;
        int response = proc[i].startTime - proc[i].arrivalTime;
        
        total_turnaround += turnaround;
        total_waiting += waiting;
        total_response += response;
    }
    
    metrics.avgTurnaround = total_turnaround / n;
    metrics.avgWaiting = total_waiting / n;
    metrics.avgResponse = total_response / n;
    
    return metrics;
}

static bool isProcessActiveOrInQueue(int process_idx, int queue[], int queue_size, 
                                     int queue_front_true_idx, int n_max_procs, 
                                     int current_running_process_idx) {
    if (current_running_process_idx == process_idx) {
        return true;
    }
    for (int i = 0; i < queue_size; i++) {
        if (queue[(queue_front_true_idx + i) % n_max_procs] == process_idx) {
            return true;
        }
    }
    return false;
}

// ---------------- Scheduling Algorithms ----------------

// FCFS Scheduling
Metrics fcfs_metrics(Process proc[], int n) {
    // Sort processes by arrival time (simple bubble sort for small n)
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (proc[j].arrivalTime > proc[j+1].arrivalTime) {
                Process temp = proc[j];
                proc[j] = proc[j+1];
                proc[j+1] = temp;
            }
        }
    }
    
    int currentTime = 0;
    for (int i = 0; i < n; i++) {
        if (currentTime < proc[i].arrivalTime) {
            currentTime = proc[i].arrivalTime;
        }
        proc[i].startTime = currentTime;
        proc[i].completionTime = currentTime + proc[i].burstTime;
        currentTime = proc[i].completionTime;
    }
    
    return calculate_metrics(proc, n);
}

// SJF Scheduling (Non-preemptive)
Metrics sjf_metrics(Process proc[], int n) {
    // Sort processes by arrival time first
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (proc[j].arrivalTime > proc[j+1].arrivalTime) {
                Process temp = proc[j];
                proc[j] = proc[j+1];
                proc[j+1] = temp;
            }
        }
    }
    
    int currentTime = 0;
    int completed = 0;
    int *completed_procs = (int *)calloc(n, sizeof(int));
    
    while (completed < n) {
        int shortest_index = -1;
        int shortest_burst = INT_MAX;
        
        // Find the process with shortest burst time among arrived processes
        for (int i = 0; i < n; i++) {
            if (!completed_procs[i] && proc[i].arrivalTime <= currentTime && 
                proc[i].burstTime < shortest_burst) {
                shortest_burst = proc[i].burstTime;
                shortest_index = i;
            }
        }
        
        if (shortest_index == -1) {
            currentTime++;
            continue;
        }
        
        proc[shortest_index].startTime = currentTime;
        proc[shortest_index].completionTime = currentTime + proc[shortest_index].burstTime;
        currentTime = proc[shortest_index].completionTime;
        completed_procs[shortest_index] = 1;
        completed++;
    }
    
    free(completed_procs);
    return calculate_metrics(proc, n);
}

// Round Robin Scheduling 
Metrics rr_metrics(Process proc[], int n, int timeQuantum) {
    if (n == 0) {
        Metrics m = {0.0f, 0.0f, 0.0f};
        return m;
    }

    int *remaining_burst_time = (int *)malloc(n * sizeof(int));
    int *actual_start_time = (int *)malloc(n * sizeof(int)); // Records when process first gets CPU
    bool *is_completed = (bool *)malloc(n * sizeof(bool));

    // Initialize process states
    for (int i = 0; i < n; i++) {
        remaining_burst_time[i] = proc[i].burstTime;
        actual_start_time[i] = -1; // -1 means not started
        is_completed[i] = false;
        // Clear any previous metrics on the input proc array
        proc[i].startTime = 0;
        proc[i].completionTime = 0;
    }

    int *ready_queue = (int *)malloc(n * sizeof(int)); // Stores indices of processes
    int queue_front_idx = 0; // Points to the actual index in the array for front
    int queue_rear_idx = -1; // Points to the actual index in the array for rear
    int processes_in_queue = 0;

    int current_time = 0;
    int completed_processes_count = 0;
    int current_running_process_idx = -1;    // Index of process on CPU, -1 if idle
    int current_quantum_slice_used = 0; // Time current process has run in its current slice

    while (completed_processes_count < n) {
        // 1. Add newly arrived processes to the ready queue
        for (int i = 0; i < n; i++) {
            if (!is_completed[i] && proc[i].arrivalTime == current_time) {
                 if (!isProcessActiveOrInQueue(i, ready_queue, processes_in_queue, queue_front_idx, n, current_running_process_idx)) {
                    queue_rear_idx = (queue_rear_idx + 1) % n;
                    ready_queue[queue_rear_idx] = i;
                    processes_in_queue++;
                }
            }
        }

        // 2. If a process is currently running
        if (current_running_process_idx != -1) {
            int idx = current_running_process_idx;

            remaining_burst_time[idx]--;
            current_quantum_slice_used++;

            if (remaining_burst_time[idx] == 0) { // Process finished
                proc[idx].completionTime = current_time; // Finishes at the end of this time unit
                proc[idx].startTime = actual_start_time[idx];
                is_completed[idx] = true;
                completed_processes_count++;
                current_running_process_idx = -1; // CPU is now free
                current_quantum_slice_used = 0;
            } else if (current_quantum_slice_used == timeQuantum) { // Quantum expired
                // Add back to ready queue (if not completed)
                queue_rear_idx = (queue_rear_idx + 1) % n;
                ready_queue[queue_rear_idx] = idx;
                processes_in_queue++;
                current_running_process_idx = -1; // CPU is now free
                current_quantum_slice_used = 0;
            }
        }

        // 3. If CPU is free, try to pick a new process from the ready queue
        if (current_running_process_idx == -1 && processes_in_queue > 0) {
            current_running_process_idx = ready_queue[queue_front_idx];
            queue_front_idx = (queue_front_idx + 1) % n;
            processes_in_queue--;
            current_quantum_slice_used = 0; // Reset for the new process

            if (actual_start_time[current_running_process_idx] == -1) {
                actual_start_time[current_running_process_idx] = current_time; // Record start time
            }
        }
        
        // 4. Advance time if not all processes are completed
        //    or if all are done but we need to break the loop.
        if (completed_processes_count < n) {
            current_time++;
        } else {
            break; // All processes are completed
        }
    }

    free(remaining_burst_time);
    free(actual_start_time);
    free(is_completed);
    free(ready_queue);

    return calculate_metrics(proc, n);
}