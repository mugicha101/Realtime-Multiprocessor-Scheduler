#include "model.h"
#include <cassert>
#include <climits>
#include <iostream>

Job Task::next_job(int task_id) {
    Job job(this, task_id, next_job_id++, next_release, exec_time, next_release + relative_deadline);
    next_release += period;
    return job;
}

void Scheduler::init(const TaskSet& task_set) {}

ScheduleDecision Scheduler::schedule(const SimModel& model) {
    return ScheduleDecision(model.cores);
}

void ExecBlockStorage::add_block(const Job& job, Fraction start, Fraction end) {
    new_blocks.emplace_back(job.task_id, job.job_id, job.core, start, end,
        job.runtime >= job.exec_time ? ExecBlock::COMPLETED : job.deadline <= end ? ExecBlock::MISSED : ExecBlock::PREEMPTED
    );
}

void ExecBlockStorage::clear() {
    exec_blocks.clear();
    new_blocks.clear();
}

bool ExecBlockStorage::hasNext() {
    return !new_blocks.empty();
}

ExecBlock ExecBlockStorage::getNext() {
    ExecBlock block = new_blocks.front();
    new_blocks.pop_front();
    exec_blocks.push_back(block);
    return block;
}

void SimModel::sim(Fraction endTime) {
    while (missed == -1 && time < endTime) {
        // handle job releases at buffer time
        for (int i = 0; i < task_set.size(); ++i)
            if (task_set[i].next_release == time)
                active_jobs.push_back(task_set[i].next_job(i));

        // sort jobs by executing first then preemptive then fresh
        int next_executing = 0;
        int next_preempted = 0;
        int next_unexecuted = 0;
        for (Job& job : active_jobs) {
            if (job.running) ++next_preempted;
            else if (job.core != -1) ++next_unexecuted;
        }
        next_unexecuted += next_preempted;
        JobSet sorted_jobs(active_jobs.size());
        for (Job& job : active_jobs) {
            if (job.running) sorted_jobs[next_executing++] = std::move(job);
            else if (job.core != -1) sorted_jobs[next_preempted++] = std::move(job);
            else sorted_jobs[next_unexecuted++] = std::move(job);
        }
        swap(sorted_jobs, active_jobs);

        // schedule
        ScheduleDecision sd = scheduler->schedule(*this);
        assert(sd.core_state.size() == cores);
        for (int i = 0; i < active_jobs.size(); ++i)
            active_jobs[i].running = false;
        for (int i = 0; i < sd.core_state.size(); ++i) {
            if (sd.core_state[i] != -1) {
                active_jobs[sd.core_state[i]].core = i;
                active_jobs[sd.core_state[i]].running = true;
            }
        }
        Fraction delta_time = sd.next_event - time;

        // update exec blocks and buffer + handle job deadlines (and misses)
        int j = -1;
        for (int i = 0; i < active_jobs.size(); ++i) {
            Job& job = active_jobs[i];
            if (job.running) {
                Fraction block_runtime = std::min(job.exec_time - job.runtime, delta_time);
                job.runtime += block_runtime;
                ebs.add_block(job, time, time + block_runtime);
                if (job.runtime == job.exec_time) {
                    finished_jobs.push_back(job);
                    continue;
                }
            }
            if (job.deadline <= sd.next_event)
                missed = i;
            active_jobs[++j] = job;
        }
        active_jobs.resize(j+1);
        time = sd.next_event;
    }
}

void SimModel::reset(TaskSet task_set, Scheduler* scheduler, int cores) {
    this->task_set = task_set;
    this->scheduler = scheduler;
    this->cores = cores;
    time = 0;
    missed = -1;
    active_jobs.clear();
    finished_jobs.clear();
}