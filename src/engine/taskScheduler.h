#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

namespace Atlantis
{
constexpr int32_t MAX_TASKS = 64;

class ATask
{
public:

    int32_t MinRange;
    std::function<void(int32_t, int32_t, uint32_t, void*)> Task;
    void* TaskContext;
    int32_t StartIndex;
    int32_t EndIndex;

    bool IsParentTask = false;
    std::atomic<int32_t> SubTaskCount{ 0 };
    ATask* ParentTask = nullptr;

    void execute(int32_t workerIndex)
    {
        if (!IsParentTask)
        {
            Task(StartIndex, EndIndex, workerIndex, TaskContext);
        }
    }
};

class ATaskScheduler
{
public:

    static unsigned int GetThreadCount()
    {
        unsigned int threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0)
        {
            std::cout << "CPU multithreading not detected!" << std::endl;
            threadCount = 1;
        }

        // TODO: we want to have 1 thread per physical core
        // but there isn't a reliable way to get the number of physical cores
        // we can only rely on the number of logical cores or arbitrary math
        if (threadCount > 10)
        {
            threadCount /= 2;
        }

        // one thread is reserved for the render thread
        if (threadCount > 1)
        {
            threadCount -= 1;
        }

        return threadCount;
    }

    ATaskScheduler(size_t numThreads = GetThreadCount())
        : _stop(false)
        , _taskCount(0)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            _workers.emplace_back(&ATaskScheduler::workerThread, this, i);
        }
    }

    ~ATaskScheduler()
    {
        {
            std::unique_lock<std::mutex> lock(_mutex_taskCount);
            std::unique_lock<std::mutex> lock2(_mutex_taskQueue);
            _stop = true;
        }

        _condition_taskCount.notify_all();
        _condition_taskQueue.notify_all();

        for (std::thread& worker : _workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        ResetTaskCount();
    }

    ATask& GetNewTask()
    {
        // TODO: this will crash if we exceed maxTasks
        std::unique_lock<std::mutex> lock(_mutex_taskCount);
        ATask& task = _tasks[_taskCount];
        ++_taskCount;
        return task;
    }

    int32_t GetTaskCount() { return _taskCount; }

    void ResetTaskCount()
    {
        // first clear all the lambda functions in the tasks
        // this prevents lingering lambdas causing crashes at shutdown
        for (int i = 0; i < _taskCount; ++i)
        {
            _tasks[i].Task = nullptr;
        }

        _taskCount = 0;
        _completedTaskCount = 0;
    }

    int32_t GetWorkerCount() { return _workers.size(); }

    void AddTask(ATask* task)
    {
        std::vector<ATask*> subtasks;

        if (task->EndIndex > task->MinRange)
        {
            task->IsParentTask = true;

            int32_t setSize = task->EndIndex;
            int32_t minRange = task->MinRange;
            task->ParentTask = nullptr;

            int32_t i = 0;
            while (setSize > 0)
            {
                int32_t currentSetSize = (std::min)(minRange, setSize);
                setSize -= minRange;

                ATask* subTask = new ATask();
                subTask->MinRange = minRange;
                subTask->Task = task->Task;
                subTask->TaskContext = task->TaskContext;
                subTask->StartIndex = i * minRange;
                subTask->EndIndex = subTask->StartIndex + currentSetSize;
                subTask->ParentTask = task;

                subtasks.push_back(subTask);
                ++i;
            }

            task->SubTaskCount = i;
        }

        {
            std::unique_lock<std::mutex> lock(_mutex_taskQueue);
            _taskQueue.push(task);
            for (ATask* subtask : subtasks)
            {
                _taskQueue.push(subtask);
            }
        }

        _condition_taskQueue.notify_all();
    }

    void WaitforTask(ATask* task)
    {
        std::unique_lock<std::mutex> lock(_mutex_taskCount);
        _condition_taskCount.wait(
            lock,
            [this, task]
            { return _completedTasks.find(task) != _completedTasks.end(); });
        // _completedTasks.erase(task);
    }

    void WaitforAllTasks()
    {
        std::unique_lock<std::mutex> lock(_mutex_taskCount);
        _condition_taskCount.wait(
            lock, [this] { return _completedTaskCount == _taskCount; });
        _completedTasks.clear();
        ResetTaskCount();
    }

private:

    void workerThread(size_t workerIndex)
    {
        while (true)
        {
            ATask* task = nullptr;

            {
                std::unique_lock<std::mutex> lock(_mutex_taskQueue);

                _condition_taskQueue.wait(
                    lock, [this] { return _stop || !_taskQueue.empty(); });

                if (_stop && _taskQueue.empty())
                {
                    return;
                }

                task = _taskQueue.front();
                _taskQueue.pop();
            }

            task->execute(static_cast<int32_t>(workerIndex));

            {
                std::unique_lock<std::mutex> lock(_mutex_taskCount);

                if (task->ParentTask)
                {
                    task->ParentTask->SubTaskCount -= 1;
                    delete task;
                }
                else if (!task->IsParentTask)
                {
                    _completedTasks.insert(task);
                    _completedTaskCount++;
                }
                else
                {
                    _condition_taskCount.wait(
                        lock, [this, &task] { return task->SubTaskCount == 0; });

                    if (task->SubTaskCount == 0)
                    {
                        _completedTasks.insert(task);
                        _completedTaskCount++;
                    }
                }
            }

            _condition_taskCount.notify_all();
        }
    }

    ATask _tasks[MAX_TASKS];
    std::atomic<int32_t> _taskCount;
    std::atomic<int32_t> _completedTaskCount;
    std::vector<std::thread> _workers;
    std::mutex _mutex_taskCount;
    std::mutex _mutex_taskQueue;
    std::condition_variable _condition_taskCount;
    std::condition_variable _condition_taskQueue;
    std::queue<ATask*> _taskQueue;
    std::set<ATask*> _completedTasks;
    bool _stop;
};
}
