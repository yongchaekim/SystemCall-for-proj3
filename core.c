struct task_struct *find_pid_of_wrrtask(pid_t pid)
{
        struct task_struct *task;
        if(pid < 0)
                return NULL;
        // if pid is 0, set/get the weight of the current process.
        else if(pid == 0)
                task = current;
        //else find the process identified by pid.
        else
                task = find_task_by_vpid(pid);

        // checks if the task is using SCHED_WRR policy and checks the task if it is not equal to NULL.
        if(task->priority != SCHED_WRR || task == NULL)
                return NULL;
        return task;
}
asmlinkage int sched_setweight(pid_t pid, int weight)
{
        struct task_struct *task;
        int value;
        int oldvalue;
        struct rq *rq;
        int flags;

        // Check whether the weight is within 1 ~ 20.
        if(weight <= 1 || weight > 20)
                return -EINVAL;
        // Check if task is not equal to NULL
        if((task = find_pid_of_wrrtask(pid)) == NULL)
                return -EINVAL;

        oldvalue = task->wrr.weight;
        //check if the user is normal or a root
        if(current_cred()->uid != 0 && current_euid() != 0)
        {
                //check that normal users can only change weight of the same owner. If not return error.
                if(!check_same_owner(task))
                        return -EPERM;
                // Normal users can only decrease the weights.
                if(weight > task->wrr.weight)
                        return -EPERM;
                task->wrr.weight = weight;
        }
        else
        {
                //root can decrease or increase the weights.
                task->wrr.weight = weight;
        }

        //place a lock since we are modifying the main runqueue of the core.
        spin_lock_irqsave(&(lock), flags);
        rq = task_rq(task);
        // Get the change of the weight value
        value = task->wrr.weight - oldvalue;
        // change the wrr's weightsum value
        rq->wrr.weightsum += value;
        // place unlock here
        spin_unlock_irqrestore(&(lock), flags);

        return 0;
}
asmlinkage int sched_getweight(pid_t pid)
{
        struct task_struct *task;
        if((task = find_pid_of_wrrtask(pid)) == NULL)
                return -EINVAL;

        // return the task's weight
        return task->wrr.weight;
}
